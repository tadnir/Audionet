#ifndef AUDIONET_LOGGER_H
#define AUDIONET_LOGGER_H

#include <stdio.h>

//#define VERBOSE
#ifdef VERBOSE
#define LOG_VERBOSE(fmt, ...) LOG("V " fmt, ##__VA_ARGS__)
#else
#define LOG_VERBOSE(fmt, ...)
#endif // VERBOSE

#define DEBUG
#ifdef DEBUG
#define LOG_DEBUG(fmt, ...) LOG("D", fmt, ##__VA_ARGS__)
#else
#define LOG_DEBUG(fmt, ...)
#endif // DEBUG

#define LOG_INFO(fmt, ...) LOG("I", fmt, ##__VA_ARGS__)
#define LOG_WARNING(fmt, ...) LOG("W", fmt, ##__VA_ARGS__)
#define LOG_ERROR(fmt, ...) LOG("E", fmt, ##__VA_ARGS__)
#define LOG_FATAL(fmt, ...) LOG("F", fmt, ##__VA_ARGS__)

#define xstr(x) str(x)
#define str(x) #x
#define LOG(level, fmt, ...) do { printf(xstr(__LINE__) "\t" level "\t" fmt "\n", ##__VA_ARGS__); fflush(stdout); } while (0)

#endif //AUDIONET_LOGGER_H
