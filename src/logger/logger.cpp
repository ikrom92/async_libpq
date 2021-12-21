#include "logger.hpp"
#include <cstdarg>
#include <ctime>
#include <iostream>
#include <sstream>

enum class log_level {
    error, info
};

static void write_log(log_level level, const char* log) {
    
    // get time
    std::time_t now = std::time(nullptr);
    struct tm local = *localtime(&now);
    char time[50];
    strftime(time, sizeof(time), "[%Y-%m-%d %H:%M:%S] ", &local);
    
    std::stringstream final_log;
    final_log << time << log << std::endl;
    
    // print log to console
    if (level == log_level::error) {
        std::cerr << final_log.str();
    }
    else {
        std::cout << final_log.str();
    }
}

void log_error(const char* fmt, ...) {
    
    // get formatted log
    char log[1024];
    va_list args;
    va_start(args, fmt);
    vsprintf(log, fmt, args);
    va_end(args);
    
    write_log(log_level::error, log);
}

void log_info(const char* fmt, ...) {
    // get formatted log
    char log[1024];
    va_list args;
    va_start(args, fmt);
    vsprintf(log, fmt, args);
    va_end(args);
    
    write_log(log_level::info, log);
}
