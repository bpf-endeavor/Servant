#include <stdarg.h>
#include "log.h"

// stdout is the default output stream
int _output_log_fd = 1;

void msg(enum log_level level, const char *func, const char *file, int line,
    const char *fmt, ...) {
  va_list args;
  va_start(args, fmt);
  char message[MAX_LOG_MESSAGE_SIZE + 1];
  char *str_lvl;
  if (level == LVL_INFO) {
    str_lvl = "INFO";
  } else if (level == LVL_DEBUG) {
    str_lvl = "DEBUG";
  } else {
    str_lvl = "ERROR";
  }

  vsnprintf(message, MAX_LOG_MESSAGE_SIZE, fmt, args);
  dprintf(_output_log_fd, "[%s] %s(%s:%d): %s", str_lvl, func, file, line, (char *)message);
  va_end(args);
}

void set_output_log_file(int fd) {
  _output_log_fd = fd;
}

