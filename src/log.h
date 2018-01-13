#pragma once

#define LOG_LEVEL_CRITICAL 1
#define LOG_LEVEL_ERROR    2
#define LOG_LEVEL_WARN     3
#define LOG_LEVEL_INFO     4
#define LOG_LEVEL_DEBUG    5

#define LOG_CRITICAL(fmt, args...) LOG(LOG_LEVEL_CRITICAL, "CRITICAL", __FILE__, __LINE__, fmt, ##args)
#define LOG_ERROR(fmt, args...) LOG(LOG_LEVEL_ERROR, "ERROR", __FILE__, __LINE__, fmt, ##args)
#define LOG_WARN(fmt, args...)  LOG(LOG_LEVEL_WARN, "WARN",  __FILE__, __LINE__, fmt, ##args)
#define LOG_INFO(fmt, args...)  LOG(LOG_LEVEL_INFO, "INFO",  __FILE__, __LINE__, fmt, ##args)
#define LOG_DEBUG(fmt, args...) LOG(LOG_LEVEL_DEBUG, "DEBUG", __FILE__, __LINE__, fmt, ##args)

#define LOG(level, level_str, file, line, fmt, args...) \
do { \
	_log(level, "level=s file=s line=d " fmt, level_str, file, line, ##args); \
} while (0)

#define LOG_OK 0
#define LOG_ERROR_UNEXPECTED_CHAR 1
#define LOG_ERROR_OUT_OF_SPACE 2

void _log(int level, const char *fmt, ...);

void set_log_level(int level);
int set_log_level_string(const char *s);

//_log("level=%s src.file=%s src.line=%s " fmt, level, __LINE__, __FILE__, ##args);
