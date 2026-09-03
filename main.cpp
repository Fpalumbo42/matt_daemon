#include <iostream>

#include <unistd.h>

#include "MattDaemon.hpp"

int main() {
    if (getuid() != 0) {
        std::cerr << "Matt_daemon: this program must be run as root." << std::endl;
        return 1;
    }
    try {
        MattDaemon daemon;
        daemon.run();
    } catch (const std::exception& e) {
        std::cerr << e.what() << std::endl;
        return 1;
    }
    return 0;
}
