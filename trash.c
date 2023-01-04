#include <assert.h>
#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <libgen.h>
#include <linux/limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#define __USE_XOPEN
#include <time.h>
#include <unistd.h>

#include "util.h"
#include "trash.h"

struct trashent {
	char *deletedfilepath;
	time_t deletiontime;
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
void committrashent(struct trashent *trashent);
void deletetrashent(struct trashent *trashent);
void restoretrashent(struct trashent *trashent);

struct trashent *readinfofile(const char *infofilepath);

void asserttrash(Trash *trash);
Trash *createtrash(const char *path);
void rewindtrash(Trash *trash);
struct trashent *readTrash(Trash *trash);


struct trashent *
createtrashent()
{
	struct trashent *trashent = xmalloc(sizeof(*trashent));
	trashent->deletedfilepath = NULL;
	trashent->deletiontime = 0;
	trashent->infofilepath = NULL;
	trashent->filesfilepath = NULL;

	return trashent;
}

void freetrashent(struct trashent *trashent)
{
	free(trashent->deletedfilepath);
	free(trashent->infofilepath);
	free(trashent->filesfilepath);

	free(trashent);
}

void
deletetrashent(struct trashent *trashent)
{
	if (remove(trashent->filesfilepath) < 0) {
		if (errno == ENOTEMPTY || errno == EEXIST)
			remove_directory(trashent->filesfilepath);
		else if (errno != ENOENT)
			die("remove:");
	}

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

time_t
strtotime(char *str)
{
	struct tm tp = (struct tm){0};
	strptime(str, "%Y-%m-%dT%H:%M:%S", &tp);
	return mktime(&tp);
}

char *
timetostr(time_t time)
{

	int buflen = 1024;
	char buf[buflen + 1];
	size_t res = strftime(buf, buflen,
					   "%Y-%m-%dT%H:%M:%S", localtime(&time));
	if (!res)
		die("strftime:");

	char *deletiondate = xmalloc((strlen(buf) + 1) * sizeof(char)) ;
	strcpy(deletiondate, buf);

	return deletiondate;
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

	char *encoded_deletedfilepath = NULL;
	char *deletiondate = NULL;

	char *line = NULL;
	size_t len = 0;
	ssize_t nread;
	while ((nread = getline(&line, &len, infofile)) != -1) {
		if (strncmp(line, "Path=", strlen("Path=")) == 0) {
			encoded_deletedfilepath = xmalloc((nread + 1) * sizeof(*encoded_deletedfilepath));
			strcpy(encoded_deletedfilepath, line);
		} else if (strncmp(line, "DeletionDate=",
					strlen("DeletionDate=")) == 0) {
			deletiondate = xmalloc((nread + 1) * sizeof(*deletiondate));
			strcpy(deletiondate, line);
		}
	};
	free(line);
	fclose(infofile);

	if (!encoded_deletedfilepath || !deletiondate)
		return NULL;

	if (encoded_deletedfilepath[strlen(encoded_deletedfilepath) - 1] == '\n')
		encoded_deletedfilepath[strlen(encoded_deletedfilepath) - 1] = '\0';

	if (deletiondate[strlen(deletiondate) - 1] == '\n')
		deletiondate[strlen(deletiondate) - 1] = '\0';

	// Remove the prefix Path= and DeletionDate=
	memmove(encoded_deletedfilepath,
		 encoded_deletedfilepath + strlen("Path="),
		 strlen(encoded_deletedfilepath) - strlen("Path=") + 1);
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

	char *deletedfilepath = fullpath_decode(encoded_deletedfilepath);
	trashent->deletedfilepath = deletedfilepath;
	trashent->deletiontime = strtotime(deletiondate);
	trashent->infofilepath = xmalloc((strlen(infofilepath) + 1) * sizeof(char));
	strcpy(trashent->infofilepath, infofilepath);
	trashent->filesfilepath = xmalloc((strlen(trashdirpath) + strlen("/files/") + trashfilenamelen + 1) * sizeof(char));
	sprintf(trashent->filesfilepath, "%s%s%s", trashdirpath, "/files/", trashfilename);

	free(deletiondate);
	free(encoded_deletedfilepath);

	return trashent;
}

void
committrashent(struct trashent *trashent)
{
	FILE *trashinfofile = fopen(trashent->infofilepath, "w");
	if (!trashinfofile)
		die("fopen: cannot open '%s':", trashent->infofilepath);


	char *encoded_deletedfilepath = fullpath_encode(trashent->deletedfilepath);

	char *deletiondate = timetostr(trashent->deletiontime);
	int result = fprintf(trashinfofile,
					  "[Trash Info]\n"
					  "Path=%s\n"
					  "DeletionDate=%s\n",
					  encoded_deletedfilepath, deletiondate);

	if (rename(trashent->deletedfilepath, trashent->filesfilepath) < 0)
		die("cannot trash '%s':", trashent->deletedfilepath);

	fclose(trashinfofile);
	free(deletiondate);
	free(encoded_deletedfilepath);
}

/*
 * Create in an atomic fashion an empty file in $Trash/files,
 * The file in $trash/files filename is based on the original filename.
 */
char *
getvalidtrashfilesfilename(Trash *trash,
						   const char *filename,
						   char *buf, size_t bufsize)
{
	char trashfilesfilepath[PATH_MAX];
	char trashinfofilepath[PATH_MAX];

	sprintf(trashfilesfilepath, "%s/%s", trash->filesdirpath, filename);
	sprintf(trashinfofilepath, "%s/%s.trashinfo", trash->infodirpath, filename);

	int i = 1;
	errno = 0;
	int fd = open(trashinfofilepath, O_CREAT | O_EXCL, S_IRUSR | S_IWUSR);
	while (fd < 0 && errno == EEXIST) {
		unsigned long trashfilesfilepathlen = strlen(trashfilesfilepath);
		unsigned long trashinfofilepathlen = strlen(trashinfofilepath);

		sprintf(trashfilesfilepath + trashfilesfilepathlen, "_%d", i);

		trashinfofilepath[trashinfofilepathlen - strlen(".trashinfo")] = '\0';
		sprintf(trashinfofilepath + trashinfofilepathlen, "_%d.trashinfo", i);

		if (strlen(trashinfofilepath) >= PATH_MAX)
			die("file name is too long");

		i++;

		fd = open(trashinfofilepath, O_CREAT | O_EXCL);
	}

	if (fd < -1)
		die("trash: cannot trash file %s:", trashfilesfilepath);

	close(fd);

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

Trash *
createtrash(const char *trashpath)
{
	assert(trashpath != NULL);

	Trash *trash = xmalloc(sizeof(*trash));

	int trashpathlen = strlen(trashpath);
	trash->trashdirpath = xmalloc(trashpathlen + 1);
	trash->filesdirpath = xmalloc(
			(trashpathlen + strlen("/files") + 1) * sizeof(char));
	trash->infodirpath = xmalloc(
			(trashpathlen + strlen("/info") + 1) * sizeof(char));

	strcpy(trash->trashdirpath, trashpath);
	sprintf(trash->filesdirpath, "%s%s", trashpath, "/files");
	sprintf(trash->infodirpath, "%s%s", trashpath, "/info");

	xmkdir(trash->filesdirpath);
	xmkdir(trash->infodirpath);

	trash->trashdir = opendir(trash->trashdirpath);
	if (!trash->trashdir)
		die("opendir: cannot open directory '%s':", trashpath);
	trash->infodir = opendir(trash->infodirpath);
	if (!trash->infodir)
		die("opendir: cannot open directory '%s':", trash->infodir);

	return trash;
}

/*
 * trashpath is /path/to/trash/directory.
 * If trashpath is NULL use the default trash directory (e.g. XDG_DATA_HOME/Trash).
 */
Trash *
opentrash(const char *trashpath)
{
	Trash *trash;
	if (!trashpath) {
		char defaulttrash[PATH_MAX + 1];
		const char *home = xgetenv("HOME", NULL);
		if (!home)
			die("HOME is not set");

		char defaultxdgdata[strlen(home) + strlen("/.local/share") + 1];
		sprintf(defaultxdgdata, "%s%s", home, "/.local/share");

		sprintf(defaulttrash, "%s/Trash", xgetenv("XDG_DATA_HOME", defaultxdgdata));
		trash = createtrash(defaulttrash);
		return trash;
	}

	trash = createtrash(trashpath);
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
istrashablepath(Trash *trash, const char *path)
{
	char fullpath[PATH_MAX];
	if (!realpath(path, fullpath))
		die("realpath:");

	return (
		strncmp(trash->trashdirpath, fullpath, strlen(fullpath)) &&
		strcmp(trash->filesdirpath, fullpath) &&
		strcmp(trash->infodirpath, fullpath)
	);
}

int
trashput(Trash *trash, const char *path)
{
	asserttrash(trash);
	assert(path != NULL);

	if (!file_exists(path))
		die("'%s' doesn't exit:", path);
	// Prevent trashing a component of the trash directory path
	if (!istrashablepath(trash, path))
		die("cannot trash '%s'", path);


	struct trashent *trashent = createtrashent();

	char fullpath[PATH_MAX];
	if (!realpath(path, fullpath))
		die("realpath:");

	char buf[PATH_MAX];
	// Get the basename of fullpath
	char *fullpath_copy = buf;
	strcpy(fullpath_copy, fullpath);
	char *trashfilesfilename = basename(fullpath_copy);
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

	trashent->deletiontime = time(NULL);
	strcpy(trashent->deletedfilepath, fullpath);
	sprintf(trashent->filesfilepath,
		 "%s/%s", trash->filesdirpath, trashfilesfilename);
	sprintf(trashent->infofilepath,
		 "%s/%s.trashinfo", trash->infodirpath, trashfilesfilename);


	committrashent(trashent);
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
		char *deletiondate = timetostr(trashent->deletiontime);
		printf("%s %s\n", deletiondate, trashent->deletedfilepath);

		freetrashent(trashent);
		free(deletiondate);
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

		if (!strcmp(deletedfilename, pattern))
			deletetrashent(trashent);

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

		int found = 0;
		if (!strcmp(deletedfilename, pattern)) {
			restoretrashent(trashent);
			deletetrashent(trashent);
			printf("restore: %s\n", trashent->deletedfilepath);

			found = 1;
		}

		freetrashent(trashent);
		if (found)
			break;
	}
}
