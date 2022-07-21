#ifndef STRASH_H
#define STRASH_H
typedef struct trash Trash;

Trash *opentrash();
void closetrash(Trash *);

int trashput(Trash *, const char *);
void trashlist(Trash *);
void trashclean(Trash *);
#endif
