#ifndef LOG_H
#define LOG_H
#include <stdio.h>

#define MAX_LOG_MESSAGE_SIZE 1023

#define INFO(fmt, args...) msg(LVL_INFO, __func__, __FILE__, __LINE__, fmt, ##args)
#define DEBUG(fmt, args...) msg(LVL_DEBUG, __func__, __FILE__, __LINE__, fmt, ##args)
#define ERROR(fmt, args...) msg(LVL_ERROR, __func__, __FILE__, __LINE__, fmt, ##args)

enum log_level {
  LVL_INFO = 500,
  LVL_DEBUG,
  LVL_ERROR,

};

extern int _output_log_fd;

/**
 * Write a message to output stream 
 */
void msg(enum log_level level, const char *func, const char *file, int line,
    const char *fmt, ...);

/**
 * Set to what output stream logs should be written
 *
 * default values is stdout
 */
void set_output_log_file(int fd);
#endif
