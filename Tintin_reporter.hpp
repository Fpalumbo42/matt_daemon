#ifndef TINTIN_REPORTER_HPP
#define TINTIN_REPORTER_HPP

#include <string>
#include <ctime>
#include <fstream>
#include <fcntl.h>
#include <unistd.h>

class Tintin_reporter {
public:
    Tintin_reporter();
    explicit Tintin_reporter(std::string path);
    Tintin_reporter(const Tintin_reporter& other);
    Tintin_reporter& operator=(const Tintin_reporter& other);
    ~Tintin_reporter();

    void log(const std::string& level, const std::string& msg) const;

private:
    std::string _path;
};

#endif
