#ifndef AUDIONET_LOGGER_H
#define AUDIONET_LOGGER_H

#include <stdio.h>

//#define VERBOSE
#ifdef VERBOSE
#define LOG_VERBOSE(fmt, ...) LOG(DEBUG, fmt, ##__VA_ARGS__)
#else
#define LOG_VERBOSE(fmt, ...)
#endif // VERBOSE

#define DEBUG
#ifdef DEBUG
#define LOG_DEBUG(fmt, ...) LOG(DEBUG, fmt, ##__VA_ARGS__)
#else
#define LOG_DEBUG(fmt, ...)
#endif // DEBUG

#define LOG_INFO(fmt, ...) LOG(INFO, fmt, ##__VA_ARGS__)
#define LOG_WARNING(fmt, ...) LOG(ERROR, fmt, ##__VA_ARGS__)
#define LOG_ERROR(fmt, ...) LOG(ERROR, fmt, ##__VA_ARGS__)
#define LOG_FATAL(fmt, ...) LOG(FATAL, fmt, ##__VA_ARGS__)

#define LOG(level, fmt, ...) do { printf(fmt "\n", ##__VA_ARGS__); fflush(stdout); } while (0)

#endif //AUDIONET_LOGGER_H
