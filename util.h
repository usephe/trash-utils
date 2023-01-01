#ifndef UTIL_H
#define UTIL_H
void die(const char *fmt, ...);

void *xmalloc(size_t size);

const char *xgetenv(const char *const env, const char *fallback);

int strendswith(const char *str, const char *suffix);

void xmkdir(char *path);

int file_exists(const char *file);


char * uri_encode(const char* originalText);
char * uri_decode(const char* encodedText);
char * fullpath_encode(char *path);
char * fullpath_decode(char *path);
#endif
