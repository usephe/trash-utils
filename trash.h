#ifndef STRASH_H
#define STRASH_H
#include <stdio.h>

struct trashinfo {
	char *filepath;
	char *deletiondate;
	int isvalid;
};


int trash(const char *path);
void listtrash();
void cleantrash();
void restoretrash();

struct trashinfo *create_trashinfo();
void free_trashinfo(struct trashinfo *trashinfo);
#endif
