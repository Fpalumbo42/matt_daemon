#include "Tintin_reporter.hpp"

Tintin_reporter::Tintin_reporter() : _path("/var/log/matt_daemon/matt_daemon.log") {}

Tintin_reporter::Tintin_reporter(std::string path) : _path(std::move(path)) {}

Tintin_reporter::Tintin_reporter(const Tintin_reporter& other) : _path(other._path) {}

Tintin_reporter& Tintin_reporter::operator=(const Tintin_reporter& other) {
    if (this != &other) {
        _path = other._path;
    }
    return *this;
}

Tintin_reporter::~Tintin_reporter() {}


void Tintin_reporter::log(const std::string& level, const std::string& msg) const {
    int fd = open(_path.c_str(), O_WRONLY | O_CREAT | O_APPEND, 0644);
    if (fd == -1)
        return;

    std::time_t now = std::time(nullptr);
    struct tm timeinfo;
    localtime_r(&now, &timeinfo);

    char stamp[32];
    std::strftime(stamp, sizeof(stamp), "[%d/%m/%Y-%H:%M:%S]", &timeinfo);

    std::string line = std::string(stamp) + " [ " + level + " ] - Matt_daemon: " + msg + "\n";

    write(fd, line.c_str(), line.length());

    close(fd);
}