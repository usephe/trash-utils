#ifndef STRASH_H
#define STRASH_H
typedef struct trash Trash;

Trash *opentrash(const char *);
void closetrash(Trash *);

int trashput(Trash *, const char *);
void trashlist(Trash *);
void trashclean(Trash *);
void trashremove(Trash *, char *);
#endif
