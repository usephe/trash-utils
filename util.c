#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>

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
