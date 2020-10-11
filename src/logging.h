#ifndef _LOGGING_H_
#define _LOGGING_H_

#include <string>

std::string UTCTimestamp(void);
void FatalError(const char *fmt, ...);
void Warning(const char *fmt, ...);
void Output(const char *fmt, ...);

#endif
