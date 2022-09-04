#ifndef UTIL_H
#define UTIL_H

void die(const char *fmt, ...);

const char *xgetenv(const char *const env, const char *fallback);

#endif
