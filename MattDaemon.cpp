#include "MattDaemon.hpp"

#include <csignal>
#include <cstring>
#include <stdexcept>
#include <string>

#include <fcntl.h>
#include <netinet/in.h>
#include <sys/file.h>
#include <sys/select.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <unistd.h>

#ifndef MSG_NOSIGNAL
# define MSG_NOSIGNAL 0
#endif

/*
sig_atomic_t -> integer read/written in one step, so a signal handler can set it safely.
volatile -> tells the compiler to re-read g_signal from memory every time, never keep it
            in a register, because the signal handler can change it at any moment.
*/
static volatile sig_atomic_t g_signal = 0;

static void signalHandler(int sig) { g_signal = sig; }

MattDaemon::MattDaemon() {
    mkdir("/var/log/matt_daemon", 0755);
}

MattDaemon::~MattDaemon() {
    for (int fd : _clients)
        close(fd);
    if (_serverFd != -1)
        close(_serverFd);
    if (_lockFd != -1) {
        flock(_lockFd, LOCK_UN);
        close(_lockFd);
        unlink("/var/lock/matt_daemon.lock");
    }
}

void MattDaemon::run() {
    takeLock();
    _log.log("INFO", "Started.");
    _log.log("INFO", "Creating server.");
    createServer();
    _log.log("INFO", "Server created.");
    _log.log("INFO", "Entering Daemon mode.");
    daemonize();
    _log.log("INFO", "started. PID: " + std::to_string(getpid()) + ".");
    setupSignals();
    serve();
    _log.log("INFO", "Quitting.");
}

/*
LOCK_EX -> Only one process can hold this lock at any given time.
LOCK_NB -> returns -1 immediately if another instance already holds the lock
*/
void MattDaemon::takeLock() {
    _lockFd = open("/var/lock/matt_daemon.lock", O_CREAT | O_RDWR, 0644);
    if (_lockFd == -1 || flock(_lockFd, LOCK_EX | LOCK_NB) == -1) {
        if (_lockFd != -1) {
            close(_lockFd);
            _lockFd = -1;
        }
        _log.log("ERROR", "Error file locked.");
        throw std::runtime_error("Can't open :/var/lock/matt_daemon.lock");
    }
}

/*
AF_INET -> Used of the protocol ipv4
SOCK_STREAM -> Connect to TCP protocol
SO_REUSEADDR -> to unlock the port, when the connection is down
SOL_SOCKET -> to target the the socket in the OS layer (TCP, UDP...)
bind -> link the descriptor to the socket struct
*/
void MattDaemon::createServer() {
    int opt = 1;
    
    _serverFd = socket(AF_INET, SOCK_STREAM, 0);
    if (_serverFd == -1)
        throw std::runtime_error("Can't create socket.");
    
    setsockopt(_serverFd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    sockaddr_in addr = {};
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = INADDR_ANY;
    addr.sin_port = htons(PORT);


    int bind_res   = bind(_serverFd, reinterpret_cast<sockaddr*>(&addr), sizeof(addr));
    int listen_res = listen(_serverFd, 3);

    if (bind_res == -1 || listen_res == -1) {
        throw std::runtime_error("Can't bind or listen on port 4242.");
    }
}

/*
Setsid -> give an id to the process and protect it to be killed from a closing terminal or a parent killed.
*/
void MattDaemon::daemonize() {
    pid_t pid = fork();

    if (pid < 0)
        throw std::runtime_error("fork failed.");
    if (pid > 0)
        _exit(0);

    setsid();
    if (chdir("/") == -1)
        _log.log("ERROR", "chdir failed.");

    int devnull = open("/dev/null", O_RDWR);

    if (devnull != -1) {
        dup2(devnull, STDIN_FILENO);
        dup2(devnull, STDOUT_FILENO);
        dup2(devnull, STDERR_FILENO);
        if (devnull > 2)
            close(devnull);
    }
}

/*
NSIG -> number of signal supported.
*/
void MattDaemon::setupSignals() {
    struct sigaction sa = {};

    sa.sa_handler = signalHandler;
    sigemptyset(&sa.sa_mask);

    for (int sig = 1; sig < NSIG; ++sig)
        sigaction(sig, &sa, nullptr);
}


/*
FD_SET -> conenct a server to be notify when a client connect itself to the server
*/
void MattDaemon::serve() {
    while (true) {

        fd_set fds;

        FD_ZERO(&fds);
        FD_SET(_serverFd, &fds);

        int maxFd = _serverFd;
        for (int fd : _clients) {
            FD_SET(fd, &fds);
            if (fd > maxFd)
                maxFd = fd;
        }

        int activite = select(maxFd + 1, &fds, nullptr, nullptr, nullptr);
        if (g_signal != 0) {
            _log.log("INFO", "Signal handler: " + std::string(strsignal(g_signal))
                             + " (" + std::to_string(g_signal) + ").");
            return;
        }
        if (activite == -1) {
            continue;
        }
        if (FD_ISSET(_serverFd, &fds))
            acceptClient();
        std::vector<int> ready;
        for (int fd : _clients)
            if (FD_ISSET(fd, &fds))
                ready.push_back(fd);
        for (int fd : ready)
            if (!handleClient(fd))
                return;
    }
}

/*
MSG_NOSIGNAL -> no SIGPIPE if the client already left, send just returns -1.
*/
void MattDaemon::acceptClient() {
    int fd = accept(_serverFd, nullptr, nullptr);

    if (fd == -1)
        return;
    if (_clients.size() >= 3) {
        static const char msg[] = "Matt_daemon: too many clients (max 3), connection refused.\n";
        send(fd, msg, sizeof(msg) - 1, MSG_NOSIGNAL);
        _log.log("ERROR", "Too many clients (max 3), connection refused.");
        close(fd);
        return;
    }
    _clients.push_back(fd);
    _log.log("INFO", "Client id " + std::to_string(fd) + " accepted.");
}

bool MattDaemon::handleClient(int fd) {
    char buf[1024];
    ssize_t n = recv(fd, buf, sizeof(buf) - 1, 0);

    if (n <= 0) {
        close(fd);
        std::erase(_clients, fd);
        return true;
    }
    std::string data(buf, static_cast<size_t>(n));
    size_t start = 0;
    
    while (start < data.size()) {
        size_t end = data.find('\n', start);
        if (end == std::string::npos)
            end = data.size();
        std::string line = data.substr(start, end - start);
        start = end + 1;
        if (!line.empty() && line.back() == '\r')
            line.pop_back();
        if (line == "quit") {
            _log.log("INFO", "Request quit.");
            return false;
        }
        if (!line.empty())
            _log.log("LOG", "User input: " + line);
    }
    return true;
}
