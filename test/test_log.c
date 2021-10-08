#include <assert.h>
#include  "ctest.h"
#include "../src/log.h"
#include <fcntl.h>

int read_full_line(FILE *f, char *buf, int bufsize)
{
  char c;
  int i = 0;
  while((c = fgetc(f)) != EOF && i < bufsize-1) {
    buf[i++] = c;
    if (c == '\n') {
      break;
    }
  }
  buf[i] = 0;
  return i;
}

CTEST(SUITE_LOG, test1)
{
  int fd;
  FILE *f;
  fd = open("/tmp/test_log.txt",  O_CREAT | O_TRUNC | O_RDWR, S_IRUSR | S_IWUSR);
  f = fdopen(fd, "w+");
  assert(f != NULL);
  // This the actual code being tested
  set_output_log_file(fd);
  INFO("This is a test\n");
  DEBUG("2 * 2 = %d\n", 4);
  // Check if log is written
  fflush(f);
  rewind(f);
  const int bufsize = 1024;
  char buf[bufsize] = {};
  read_full_line(f, buf, bufsize);
  /* printf("\n\n%s\n", buf); */
  ASSERT_STR("[INFO] ctest_SUITE_LOG_test1_run(test_log.c:29): This is a test\n", buf);
  read_full_line(f, buf, bufsize);
  ASSERT_STR("[DEBUG] ctest_SUITE_LOG_test1_run(test_log.c:30): 2 * 2 = 4\n", buf);
  fclose(f);
}
