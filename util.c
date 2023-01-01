#include <assert.h>
#include <ctype.h>
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

void *
xmalloc(size_t size)
{
	void *p = malloc(size);
	if (!p)
		die("malloc:");

	return p;
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

int
file_exists(const char *file)
{
	struct stat sb;
	return lstat(file, &sb) == 0;
}

char *
uri_decode(const char *encoded_str)
{
	unsigned long len = strlen(encoded_str);
	// allocate memory for the worst possible case (all characters are unreserved characters)
	char *str = xmalloc((len + 1) * sizeof(*str));

	char buf[16];
    size_t pos = 0;
    for (unsigned long i = 0; i < len; i++) {
		switch (encoded_str[i]) {
		case '%': {
			int n = sprintf(buf, "%.2s", encoded_str + i + 1);
			buf[n] = '\0';
			// debug_print("\tbuf = %s\n", buf);
			long d = strtol(buf, NULL, 16);
			// debug_print("\td = %ld\n", d);
			pos += sprintf(str + pos, "%c", (char)d);
			i += 2;
		}
			break;
		default:
			str[pos++] = encoded_str[i];
		}
	}

	str[pos] = '\0';

	return str;
}

char *
uri_encode(const char *str)
{
	unsigned long len = strlen(str);
	// allocate memory for the worst possible case (all characters need to be encoded)
	char *encoded_str = xmalloc(
			(len * 3 + 1) * sizeof(*encoded_str));

    size_t pos = 0;
    for (unsigned long i = 0; i < len; i++) {
		if (
			isalnum((unsigned char) str[i]) ||
			str[i] == '-' ||
			str[i] == '_' ||
			str[i] == '.' ||
			str[i] == '~'
		) {
			encoded_str[pos++] = str[i];
		} else {
			pos += sprintf(encoded_str + pos, "%%%X", (unsigned char)str[i]);
		}
	}
	encoded_str[pos] = '\0';

	return encoded_str;
}

char *
fullpath_encode(char *path)
{
	assert(path != NULL);
	assert(path[0] == '/');


	printf("path = %s\n", path);
	unsigned long pathlen = strlen(path);
	char *pathcopy = xmalloc((pathlen + 1) * sizeof(*pathcopy));
	strcpy(pathcopy, path);
	char *encoded_path = xmalloc((pathlen * 3 + 1) * sizeof(*encoded_path));
	*encoded_path = '\0';

	for(char *tok = strtok(pathcopy, "/"); tok; tok = strtok(NULL, "/")) {
		char *encoded_tok = uri_encode(tok);

		strcat(encoded_path, "/");
		strcat(encoded_path, encoded_tok);

		free(encoded_tok);
	}
	free(pathcopy);

	printf("encoded_path = %s\n", encoded_path);
	return encoded_path;
}

char *
fullpath_decode(char *encoded_path)
{
	printf("fullpath_decode: encoded_path = %s\n", encoded_path);
	assert(encoded_path != NULL);
	assert(encoded_path[0] == '/');

	unsigned long encoded_pathlen = strlen(encoded_path);
	char *encoded_pathcopy = xmalloc((encoded_pathlen + 1) * sizeof(*encoded_pathcopy));
	strcpy(encoded_pathcopy, encoded_path);
	char *path = xmalloc((encoded_pathlen + 1) * sizeof(*path));
	*path = '\0';

	for(char *encoded_tok = strtok(encoded_pathcopy, "/"); encoded_tok; encoded_tok = strtok(NULL, "/")) {
		char *tok = uri_decode(encoded_tok);

		strcat(path, "/");
		strcat(path, tok);

		free(tok);
	}
	free(encoded_pathcopy);

	printf("fullpath_decode: path = %s\n", path);
	return path;
}
