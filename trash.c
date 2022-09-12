#include <assert.h>
#include <dirent.h>
#include <errno.h>
#include <libgen.h>
#include <linux/limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <time.h>

#include "util.h"
#include "trash.h"

struct trashent {
	char *deletedfilepath;
	char *deletiondate;
	char *infofilepath;
	char *filesfilepath;
};

struct trash {
	DIR *trashdir;
	DIR *infodir;
	char *trashdirpath;
	char *filesdirpath;
	char *infodirpath;
};


struct trashent *createtrashent();
void freetrashent(struct trashent *trashent);
void deletetrashent(struct trashent *trashent);
void restoretrashent(struct trashent *trashent);

struct trashent *readinfofile(const char *infofilepath);
int writeinfofile(struct trashent *trashent);

void asserttrash(Trash *trash);
Trash *createtrash(const char *path);
void rewindtrash(Trash *trash);
struct trashent *readTrash(Trash *trash);


struct trashent *
createtrashent()
{
	struct trashent *trashent = xmalloc(sizeof(*trashent));
	trashent->deletedfilepath = NULL;
	trashent->deletiondate = NULL;
	trashent->infofilepath = NULL;
	trashent->filesfilepath = NULL;

	return trashent;
}

void freetrashent(struct trashent *trashent)
{
	free(trashent->deletedfilepath);
	free(trashent->deletiondate);
	free(trashent->infofilepath);
	free(trashent->filesfilepath);

	free(trashent);
}

void
deletetrashent(struct trashent *trashent)
{
	if (remove(trashent->filesfilepath) < 0)
		if (errno != ENOENT)
			die("remove:");

	if (remove(trashent->infofilepath) < 0)
		die("remove: cannot remove file '%s':", trashent->infofilepath);
}

void
restoretrashent(struct trashent *trashent)
{
	if (file_exists(trashent->deletedfilepath))
		die("Refusing to overwite existing file '%s'", trashent->deletedfilepath);

	if (rename(trashent->filesfilepath, trashent->deletedfilepath) < 0)
		die("cannot restore '%s':", trashent->filesfilepath);
}

struct trashent *
readinfofile(const char *infofilepath)
{
	if (!strendswith(infofilepath, ".trashinfo"))
		return NULL;

	struct trashent *trashent = createtrashent();
	struct stat statbuf;

	FILE *infofile = fopen(infofilepath, "r");
	if (!infofile)
		die("fopen: cannot open file '%s':", infofilepath);

	if (lstat(infofilepath, &statbuf) < 0)
		die("stat:");

	if (!S_ISREG(statbuf.st_mode))
		return NULL;

	char *deletedfilepath = NULL;
	char *deletiondate = NULL;

	char *line = NULL;
	size_t len = 0;
	ssize_t nread;
	while ((nread = getline(&line, &len, infofile)) != -1) {
		if (strncmp(line, "Path=", strlen("Path=")) == 0) {
			deletedfilepath = xmalloc((nread + 1) * sizeof(*deletedfilepath));
			strcpy(deletedfilepath, line);
		} else if (strncmp(line, "DeletionDate=",
					strlen("DeletionDate=")) == 0) {
			deletiondate = xmalloc((nread + 1) * sizeof(*deletedfilepath));
			strcpy(deletiondate, line);
		}
	};
	free(line);
	fclose(infofile);

	if (!deletedfilepath || !deletiondate)
		return NULL;

	if (deletedfilepath[strlen(deletedfilepath) - 1] == '\n')
		deletedfilepath[strlen(deletedfilepath) - 1] = '\0';

	if (deletiondate[strlen(deletiondate) - 1] == '\n')
		deletiondate[strlen(deletiondate) - 1] = '\0';

	// Remove the prefix Path= and DeletionDate=
	memmove(deletedfilepath, deletedfilepath + strlen("Path="),
			strlen(deletedfilepath) - strlen("Path=") + 1);
	memmove(deletiondate,
			deletiondate + strlen("DeletionDate="),
			strlen(deletiondate) - strlen("DeletionDate=") + 1);

	char infofilepath_copy[strlen(infofilepath) + 1];
	strcpy(infofilepath_copy, infofilepath);
	char *infofilename = basename(infofilepath_copy);
	char *infodirpath = dirname(infofilepath_copy);
	char *trashdirpath = dirname(infodirpath);

	int trashfilenamelen = strlen(infofilename) - strlen(".trashinfo");
	char trashfilename[trashfilenamelen + 1];
	strncpy(trashfilename, infofilename, trashfilenamelen);
	trashfilename[trashfilenamelen] = '\0';

	trashent->deletedfilepath = deletedfilepath;
	trashent->deletiondate = deletiondate;
	trashent->infofilepath = xmalloc((strlen(infofilepath) + 1) * sizeof(char));
	strcpy(trashent->infofilepath, infofilepath);
	trashent->filesfilepath = xmalloc((strlen(trashdirpath) + strlen("/files/") + trashfilenamelen + 1) * sizeof(char));
	sprintf(trashent->filesfilepath, "%s%s%s", trashdirpath, "/files/", trashfilename);

	return trashent;
}

int
writeinfofile(struct trashent *trashent)
{
	FILE *trashinfofile = fopen(trashent->infofilepath, "w");
	if (!trashinfofile)
		die("fopen: cannot open '%s':", trashent->infofilepath);

	int result = fprintf(trashinfofile,
					  "[Trash Info]\n"
					  "Path=%s\n"
					  "DeletionDate=%s\n",
					  trashent->deletedfilepath, trashent->deletiondate);

	fclose(trashinfofile);

	return result;
}

char *
getvalidtrashfilesfilename(
	Trash *trash, const char *filename, char *buf, size_t bufsize)
{
	char trashfilesfilepath[PATH_MAX];
	char trashinfofilepath[PATH_MAX];

	sprintf(trashfilesfilepath, "%s/%s", trash->filesdirpath, filename);
	sprintf(trashinfofilepath, "%s/%s.trashinfo", trash->infodirpath, filename);

	int i = 1;
	while (file_exists(trashfilesfilepath) || file_exists(trashinfofilepath)) {
		sprintf(trashfilesfilepath + strlen(trashfilesfilepath), "_%d", i);
		trashinfofilepath[strlen(trashinfofilepath) - strlen(".trashinfo")] = '\0';
		sprintf(trashinfofilepath + strlen(trashinfofilepath), "_%d.trashinfo", i);
		if (strlen(trashinfofilepath) >= PATH_MAX)
			die("file name is too long");
		i++;
	}

	char *trashfilesfilename = basename(trashfilesfilepath);
	if (strlen(trashfilesfilename) >= bufsize)
		return NULL;

	strcpy(buf, trashfilesfilename);
	return buf;
}

void
asserttrash(Trash *trash)
{
	assert(trash != NULL);

	assert(trash->trashdir != NULL);
	assert(trash->infodir != NULL);

	assert(trash->trashdir != NULL);
	assert(trash->filesdirpath != NULL);
	assert(trash->infodirpath != NULL);
}

// if path is NULL used XDG_DATA_HOME/Trash as the trash location instead
Trash *
createtrash(const char *path)
{
	Trash *trash = xmalloc(sizeof(*trash));
	char trashloc[PATH_MAX + 1];
	int trashdirlen = 0;

	if (path) {
		strcpy(trashloc, path);
	} else {
		const char *home = xgetenv("HOME", NULL);
		if (!home)
			die("HOME is not set");

		char defaultxdgdata[strlen(home) + strlen("/.local/share") + 1];
		sprintf(defaultxdgdata, "%s%s", home, "/.local/share");

		strcpy(trashloc, xgetenv("XDG_DATA_HOME", defaultxdgdata));
	}

	trashdirlen = strlen(trashloc) + strlen("/Trash");
	char *trashpath = xmalloc(trashdirlen + 1);
	sprintf(trashpath, "%s%s", trashloc, "/Trash");

	trash->trashdirpath = trashpath;

	trash->filesdirpath = xmalloc(
			(trashdirlen + strlen("/files") + 1) * sizeof(char));
	sprintf(trash->filesdirpath, "%s%s", trashpath, "/files");

	trash->infodirpath = xmalloc(
			(trashdirlen + strlen("/info") + 1) * sizeof(char));
	sprintf(trash->infodirpath, "%s%s", trashpath, "/info");

	xmkdir(trash->filesdirpath);
	xmkdir(trash->infodirpath);

	trash->trashdir = opendir(trashpath);
	if (!trash->trashdir)
		die("opendir: cannot open directory '%s':", trashpath);
	trash->infodir = opendir(trash->infodirpath);
	if (!trash->infodir)
		die("opendir: cannot open directory '%s':", trash->infodir);

	return trash;
}

Trash *
opentrash(const char *trashpath)
{
	Trash *trash = createtrash(trashpath);
	return trash;
}

void
closetrash(Trash *trash)
{
	asserttrash(trash);

	if (closedir(trash->trashdir) < 0)
		die("closedir:");

	if (closedir(trash->infodir) < 0)
		die("closedir:");

	free(trash->trashdirpath);
	free(trash->infodirpath);
	free(trash->filesdirpath);
	free(trash);
}

void
rewindtrash(Trash *trash)
{
	asserttrash(trash);
	rewinddir(trash->infodir);
}

struct trashent *
readTrash(Trash *trash)
{
	asserttrash(trash);
	struct trashent *trashent;
	struct dirent *dp = NULL;

	errno = 0;
	while ((dp = readdir(trash->infodir)) != NULL) {
		if (strcmp(dp->d_name, ".") == 0 || strcmp(dp->d_name, "..") == 0)
			continue;

		if (!strendswith(dp->d_name, ".trashinfo"))
			continue;

		int infofilepathlen = strlen(trash->infodirpath) + 1 + strlen(dp->d_name);
		char infofilepath[infofilepathlen + 1];
		sprintf(infofilepath, "%s/%s", trash->infodirpath, dp->d_name);

		errno = 0;
		trashent = readinfofile(infofilepath);
		if (!trashent) {
			if (errno != 0)
				die("readinfofile:");
			else
				die("readinfofile");
		}

		break;
	}

	if (dp == NULL && errno != 0)
		die("readir:");

	return dp != NULL ? trashent : NULL;
}

int
trashput(Trash *trash, const char *path)
{
	asserttrash(trash);
	assert(path != NULL);

	if (!file_exists(path))
		die("'%s' doesn't exit:", path);

	time_t time_now = time(NULL);

	char fullpath[PATH_MAX];
	if (!realpath(path, fullpath))
		die("realpath:");

	if (
		!strncmp(trash->trashdirpath, fullpath, strlen(fullpath)) ||
		!strcmp(trash->filesdirpath, fullpath) ||
		!strcmp(trash->infodirpath, fullpath)
	)
		die("cannot trash directory '%s'", path);


	struct trashent *trashent = createtrashent();

	// get the basename of fullpath
	char *path_copy = xmalloc((strlen(fullpath) + 1) * sizeof(char));
	strcpy(path_copy, fullpath);
	char *trashfilesfilename = basename(path_copy);

	char buf[PATH_MAX];
	trashfilesfilename = getvalidtrashfilesfilename(trash,
							 trashfilesfilename,
							 buf, sizeof(buf));


	trashent->deletedfilepath = xmalloc((strlen(fullpath) + 1) * sizeof(char));
	trashent->filesfilepath = xmalloc(
			(strlen(trash->filesdirpath) + 1 +
		   strlen(trashfilesfilename) + 1) * sizeof(char));
	trashent->infofilepath = xmalloc(
			(strlen(trash->infodirpath) + 1 +
			strlen(trashfilesfilename) +
			strlen(".trashinfo") + 1)
			* sizeof(char));
	sprintf(trashent->filesfilepath,
		 "%s/%s", trash->filesdirpath, trashfilesfilename);
	sprintf(trashent->infofilepath,
		 "%s/%s.trashinfo", trash->infodirpath, trashfilesfilename);
	strcpy(trashent->deletedfilepath, fullpath);

	int deletiondatelen = 1024;
	trashent->deletiondate = xmalloc(deletiondatelen * sizeof(char));
	if (!strftime(trashent->deletiondate, deletiondatelen,
			   "%Y-%m-%dT%H:%M:%S", localtime(&time_now))) {
		die("strftime:");
	}

	writeinfofile(trashent);

	if (rename(fullpath, trashent->filesfilepath) < 0)
		die("cannot trash '%s':");

	free(path_copy);
	freetrashent(trashent);

	return 0;
}

void
trashlist(Trash *trash)
{
	asserttrash(trash);

	rewindtrash(trash);

	struct trashent *trashent;

	while ((trashent = readTrash(trash)) != NULL) {
		printf("%s %s\n", trashent->deletiondate, trashent->deletedfilepath);

		freetrashent(trashent);
	}
}

void
trashclean(Trash *trash)
{
	asserttrash(trash);

	rewindtrash(trash);

	struct trashent *trashent;
	while ((trashent = readTrash(trash)) != NULL)
		deletetrashent(trashent);
}

void
trashremove(Trash *trash, char *pattern)
{
	asserttrash(trash);
	assert(pattern != NULL);

	rewindtrash(trash);

	struct trashent *trashent;

	while ((trashent = readTrash(trash)) != NULL) {
		char deletedfilepath_copy[strlen(trashent->deletedfilepath) + 1];
		strcpy(deletedfilepath_copy, trashent->deletedfilepath);
		char *deletedfilename = basename(deletedfilepath_copy);

		// TODO: support removing none empty directories from Trash
		if (!strcmp(deletedfilename, pattern)) {
			if (remove(trashent->filesfilepath) < 0)
				die("remove:");
			printf("trashfilesfilepath: %s\n", trashent->filesfilepath);

			if (remove(trashent->infofilepath) < 0)
				die("remove:");
			printf("trashinfofilepath: %s\n", trashent->infofilepath);

			printf("\n");
		}

		freetrashent(trashent);
	}
}

void
trashrestore(Trash *trash, char *pattern)
{
	asserttrash(trash);
	assert(pattern != NULL);

	rewindtrash(trash);

	struct trashent *trashent;

	while ((trashent = readTrash(trash))) {
		char deletedfilepath_copy[strlen(trashent->deletedfilepath) + 1];
		strcpy(deletedfilepath_copy, trashent->deletedfilepath);
		char *deletedfilename = basename(deletedfilepath_copy);

		if (!strcmp(deletedfilename, pattern)) {
			restoretrashent(trashent);
			deletetrashent(trashent);
			printf("restore: %s\n", trashent->deletedfilepath);
			break;
		}

		freetrashent(trashent);
	}
}
