#pragma once

#define LOG_ERROR(fmt, args...) LOG("ERROR", fmt, ##args)
#define LOG_WARN(fmt, args...) LOG("WARN", fmt, ##args)
#define LOG_INFO(fmt, args...) LOG("INFO", fmt, ##args)
#define LOG_DEBUG(fmt, args...) LOG("DEBUG", fmt, ##args)

#define LOG(level, fmt, args...) \
do { \
	_log("level=s " fmt, level, ##args); \
} while (0)

void _log(const char *fmt, ...);

//_log("level=%s src.file=%s src.line=%s " fmt, level, __LINE__, __FILE__, ##args);
