#ifndef AUDIONET_LOGGER_H
#define AUDIONET_LOGGER_H

#include <stdio.h>


#define LOG_DEBUG(fmt, ...) LOG(DEBUG, fmt, ##__VA_ARGS__)
#define LOG_INFO(fmt, ...) LOG(INFO, fmt, ##__VA_ARGS__)
#define LOG_ERROR(fmt, ...) LOG(ERROR, fmt, ##__VA_ARGS__)

#define LOG(level, fmt, ...) do { printf(fmt "\n", ##__VA_ARGS__); fflush(stdout); } while (0)

#endif //AUDIONET_LOGGER_H
