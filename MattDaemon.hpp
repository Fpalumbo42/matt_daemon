#ifndef MATT_DAEMON_HPP
#define MATT_DAEMON_HPP

#include <vector>
#include "Tintin_reporter.hpp"

#define PORT 4242

class MattDaemon {
public:
    MattDaemon();
    MattDaemon(const MattDaemon& other) = delete;
    MattDaemon& operator=(const MattDaemon& other) = delete;
    ~MattDaemon();

    void run();

private:
    void takeLock();
    void createServer();
    void daemonize();
    void setupSignals();
    void serve();
    void acceptClient();
    bool handleClient(int fd);

    Tintin_reporter _log;
    int _lockFd = -1;
    int _serverFd = -1;
    std::vector<int> _clients;
};

#endif
