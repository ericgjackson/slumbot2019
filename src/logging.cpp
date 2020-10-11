#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include <cstdarg>
#include <string>

#include "logging.h"

using std::string;

string UTCTimestamp(void) {
  string timestamp;
  time_t current_time = time(NULL);
  // timestamp is empty string if we can't obtain current time
  if (current_time != ((time_t)-1)) {
    struct tm buf;
    char str[26];
    // Annoyingly, asctime_r() adds a newline at the end
    timestamp = asctime_r(gmtime_r(&current_time, &buf), str);
    int sz = timestamp.size();
    if (sz > 0) timestamp.resize(sz - 1);
  }
  return timestamp;
}

void PrintUTCTimestamp(FILE *stream) {
  string timestamp = UTCTimestamp();
  fprintf(stream, "%s: ", timestamp.c_str());
}

void Output(const char *fmt, ...) {
  // Print timestamp before message
  PrintUTCTimestamp(stdout);
  va_list arg;
  va_start(arg, fmt);
  vfprintf(stdout, fmt, arg);
  va_end(arg);
  fflush(stdout);
}

void FatalError(const char *fmt, ...) {
  // Print timestamp before message
  PrintUTCTimestamp(stderr);
  va_list arg;
  va_start(arg, fmt);
  fprintf(stderr, "FATAL ERROR: ");
  vfprintf(stderr, fmt, arg);
  va_end(arg);
  exit(-1);
}

void Warning(const char *fmt, ...) {
  // Print timestamp before message
  PrintUTCTimestamp(stderr);
  va_list arg;
  va_start(arg, fmt);
  fprintf(stderr, "WARNING: ");
  vfprintf(stderr, fmt, arg);
  va_end(arg);
}
