#include <stdio.h>
#include <stdarg.h>
#include "log.h"

// stdout is the default output stream
int _output_log_fd = 1;

void msg(enum log_level level, const char *func, const char *file, int line,
    const char *fmt, ...) {
  va_list args;
  va_start(args, fmt);
  char message[MAX_LOG_MESSAGE_SIZE + 1];
  vsnprintf(message, MAX_LOG_MESSAGE_SIZE, fmt, args);
  char *str_lvl;
  if (level == LVL_INFO) {
    str_lvl = "INFO";
    dprintf(_output_log_fd, "%s", (char *)message);
  } else if (level == LVL_DEBUG) {
    str_lvl = "DEBUG";
    dprintf(_output_log_fd, "\033[0;94;49m");
    dprintf(_output_log_fd, "[%s] %s(%s:%d): %s", str_lvl, func, file, line, (char *)message);
    dprintf(_output_log_fd, "\033[0m");
  } else {
    str_lvl = "ERROR";
    dprintf(_output_log_fd, "\033[0;31;49m");
    dprintf(_output_log_fd, "[%s] %s(%s:%d): %s", str_lvl, func, file, line, (char *)message);
    dprintf(_output_log_fd, "\033[0m");
  }
  va_end(args);
}

void set_output_log_file(int fd) {
  _output_log_fd = fd;
}

