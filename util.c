#include <assert.h>
#include <errno.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

#include "util.h"

void
die(const char *fmt, ...)
{
	int errnocpy = errno;
	va_list ap;

	va_start(ap, fmt);
	vfprintf(stderr, fmt, ap);
	va_end(ap);

	if (fmt[0] && fmt[strlen(fmt) - 1] == ':') {
		fputc(' ', stderr);
		fprintf(stderr, "%s\n", strerror(errnocpy));
	} else
		fputc('\n', stderr);

	exit(EXIT_FAILURE);
}

const char *
xgetenv(const char *const env, const char *fallback) {
	const char *value = getenv(env);

	return value && value[0] ? value : fallback;
}

int
strendswith(const char *str, const char *suffix)
{
	assert(str != NULL && suffix != NULL);
	int srclen = strlen(str);
	int suffixlen = strlen(suffix);

	if (suffixlen > srclen)
		return 0;

	return strcmp(str + (srclen - suffixlen), suffix) == 0;
}

void
xmkdir(char *path)
{
    char *sep = strrchr(path, '/');
    if(sep != NULL) {
        *sep = '\0';
        xmkdir(path);
        *sep = '/';
    }

	if (!path || !path[0])
		return;

	if(mkdir(path, 0777) && errno != EEXIST)
		die("mkdir '%s':", path);
}
