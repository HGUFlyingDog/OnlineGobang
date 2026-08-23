#pragma once
#include <fstream>
#include <iostream>
#include <time.h>

#define INFO 0
#define DBG 1
#define ERR 2
#define DEFAULT_LOG_LEVEL INFO

#define LOG(level, format, ...)                                                                \
    do                                                                                         \
    {                                                                                          \
        if (level >= DEFAULT_LOG_LEVEL)                                                        \
        {                                                                                      \
            time_t t = time(NULL);                                                             \
            struct tm *lt = localtime(&t);                                                     \
            char buf[32] = {0};                                                                \
            strftime(buf, 31, "%H:%M:%S", lt);                                                 \
            fprintf(stdout, "[%s %s:%d]" format "\n", buf, __FILE__, __LINE__, ##__VA_ARGS__); \
        }                                                                                      \
    } while (0)

#define INFO_LOG(format, ...) LOG(INFO, format, ##__VA_ARGS__)
#define DBG_LOG(format, ...) LOG(DBG, format, ##__VA_ARGS__)
#define ERR_LOG(format, ...) LOG(ERR, format, ##__VA_ARGS__)