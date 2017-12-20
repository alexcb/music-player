#pragma once

#define LOG_CRITICAL(fmt, args...) LOG("CRITICAL", __FILE__, __LINE__, fmt, ##args)
#define LOG_ERROR(fmt, args...) LOG("ERROR", __FILE__, __LINE__, fmt, ##args)
#define LOG_WARN(fmt, args...)  LOG("WARN",  __FILE__, __LINE__, fmt, ##args)
#define LOG_INFO(fmt, args...)  LOG("INFO",  __FILE__, __LINE__, fmt, ##args)
#define LOG_DEBUG(fmt, args...) LOG("DEBUG", __FILE__, __LINE__, fmt, ##args)

#define LOG(level, file, line, fmt, args...) \
do { \
	_log("level=s file=s line=d " fmt, level, file, line, ##args); \
} while (0)

#define LOG_OK 0
#define LOG_ERROR_UNEXPECTED_CHAR 1
#define LOG_ERROR_OUT_OF_SPACE 2

void _log(const char *fmt, ...);

//_log("level=%s src.file=%s src.line=%s " fmt, level, __LINE__, __FILE__, ##args);
