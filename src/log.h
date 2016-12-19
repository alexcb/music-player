#pragma once

#define LOG_ERROR(fmt, args...) LOG("ERROR", ##args)
#define LOG_WARN(fmt, args...) LOG("ERROR", ##args)
#define LOG_INFO(fmt, args...) LOG("ERROR", ##args)
#define LOG_DEBUG(fmt, args...) LOG("ERROR", ##args)

#define LOG(level, fmt, args...) \
do { \
	_log("level=%s src.file=%s src.line=%s " fmt, level, __LINE__, __FILE__, ##args); \
} while (0)

void _log(const char *fmt, ...);
