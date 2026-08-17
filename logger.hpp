#pragma once
#include <fstream>
#include <iostream>
#include <time.h>

#define INFO 0
#define DBG 1
#define ERR 2
#define DEFAULT_LOG_LEVEL INFO

#define LOG(level, formart, ...)                                                            \
    if (level >= DEFAULT_LOG_LEVEL)                                                         \
    {                                                                                       \
        time_t t = time(NULL);                                                              \
        struct tm *lt = localtime(&t);                                                      \
        char buf[32] = {0};                                                                 \
        strftime(buf, 31, "%H:%M:%S", lt);                                                  \
        fprintf(stdout, "[%s %s:%d]" formart "\n", buf, __FILE__, __LINE__, ##__VA_ARGS__); \
    }

#define INFO_LOG(formart, ...) LOG(INFO, formart, ##__VA_ARGS__)
#define DBG_LOG(formart, ...) LOG(DBG, formart, ##__VA_ARGS__)
#define ERR_LOG(formart, ...) LOG(ERR, formart, ##__VA_ARGS__)