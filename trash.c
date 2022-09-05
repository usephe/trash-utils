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

struct trashinfo {
	char *deletedfilepath;
	char *deletiondate;
	char *trashinfofilepath;
	char *trashfilesfilepath;
};

struct trash {
	DIR *trashdir;
	DIR *infodir;
	char *filesdirpath;
	char *infodirpath;
};


struct trashinfo *create_trashinfo();
void free_trashinfo(struct trashinfo *trashinfo);

struct trashinfo *readinfofile(const char *infofilepath);
int writeinfofile(struct trashinfo *trashinfo);

void asserttrash(Trash *trash);
Trash *createtrash(const char *path);
struct trashinfo *readTrash(Trash *trash);


struct trashinfo *
create_trashinfo()
{
	struct trashinfo *trashinfo = malloc(sizeof(*trashinfo));
	if (!trashinfo)
		die("malloc:");
	trashinfo->deletedfilepath = NULL;
	trashinfo->deletiondate = NULL;
	trashinfo->trashinfofilepath = NULL;
	trashinfo->trashfilesfilepath = NULL;

	return trashinfo;
}

void free_trashinfo(struct trashinfo *trashinfo)
{
	free(trashinfo->deletedfilepath);
	free(trashinfo->deletiondate);
	free(trashinfo->trashinfofilepath);
	free(trashinfo->trashfilesfilepath);

	free(trashinfo);
}

struct trashinfo *
readinfofile(const char *infofilepath)
{
	if (!strendswith(infofilepath, ".trashinfo"))
		return NULL;

	struct trashinfo *trashinfo = create_trashinfo();
	struct stat statbuf;

	FILE *infofile = fopen(infofilepath, "r");
	if (!infofile)
		die("fopen:");

	if (stat(infofilepath, &statbuf) < 0)
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
			deletedfilepath = malloc((nread + 1) * sizeof(*deletedfilepath));
			strcpy(deletedfilepath, line);
		} else if (strncmp(line, "DeletionDate=",
					strlen("DeletionDate=")) == 0) {
			deletiondate = malloc((nread + 1) * sizeof(*deletedfilepath));
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

	trashinfo->deletedfilepath = deletedfilepath;
	trashinfo->deletiondate = deletiondate;
	trashinfo->trashinfofilepath = malloc((strlen(infofilepath) + 1) * sizeof(char));
	strcpy(trashinfo->trashinfofilepath, infofilepath);
	trashinfo->trashfilesfilepath = malloc((strlen(trashdirpath) + strlen("/files/") + trashfilenamelen + 1) * sizeof(char));
	sprintf(trashinfo->trashfilesfilepath, "%s%s%s", trashdirpath, "/files/", trashfilename);

	return trashinfo;
}

int
writeinfofile(struct trashinfo *trashinfo)
{
	FILE *trashinfofile = fopen(trashinfo->trashinfofilepath, "w");
	if (!trashinfofile)
		die("fopen: can't open %s:", trashinfo->trashinfofilepath);

	int result = fprintf(trashinfofile,
					  "[Trash Info]\n"
					  "Path=%s\n"
					  "DeletionDate=%s\n",
					  trashinfo->deletedfilepath, trashinfo->deletiondate);

	fclose(trashinfofile);

	return result;
}

char *
getvalidtrashfilesfilename(
	Trash *trash, const char *filename, char *buf, size_t bufsize)
{
	char trashfilesfilepath[PATH_MAX];
	char trashinfofilepath[PATH_MAX];
	struct stat statbuf;

	sprintf(trashfilesfilepath, "%s/%s", trash->filesdirpath, filename);
	sprintf(trashinfofilepath, "%s/%s.trashinfo", trash->infodirpath, filename);

	int i = 1;
	while (stat(trashfilesfilepath, &statbuf) == 0 ||
		stat(trashinfofilepath, &statbuf) == 0) {
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
	assert(trash->filesdirpath != NULL && trash->infodirpath != NULL);
}

// if path is NULL used XDG_DATA_HOME/Trash as the trash location instead
Trash *
createtrash(const char *path)
{
	Trash *trash = malloc(sizeof(*trash));
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
	char trashpath[trashdirlen + 1];
	sprintf(trashpath, "%s%s", trashloc, "/Trash");

	trash->filesdirpath = malloc(
			(trashdirlen + strlen("/files") + 1) * sizeof(char));
	sprintf(trash->filesdirpath, "%s%s", trashpath, "/files");

	trash->infodirpath = malloc(
			(trashdirlen + strlen("/info") + 1) * sizeof(char));
	sprintf(trash->infodirpath, "%s%s", trashpath, "/info");

	xmkdir(trash->filesdirpath);
	xmkdir(trash->infodirpath);

	trash->trashdir = opendir(trashpath);
	if (!trash->trashdir)
		die("opendir: can't open directory '%s':", trashpath);
	trash->infodir = opendir(trash->infodirpath);
	if (!trash->infodir)
		die("opendir: can't open directory '%s':", trash->infodir);

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

	free(trash->infodirpath);
	free(trash->filesdirpath);
	free(trash);
}

struct trashinfo *
readTrash(Trash *trash)
{
	asserttrash(trash);
	struct trashinfo *trashinfo;
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
		trashinfo = readinfofile(infofilepath);
		if (!trashinfo) {
			if (errno != 0)
				die("readinfofile:");
			else
				die("readinfofile");
		}

		break;
	}

	if (dp == NULL && errno != 0)
		die("readir:");

	return dp != NULL ? trashinfo : NULL;
}

int
trashput(Trash *trash, const char *path)
{
	asserttrash(trash);
	assert(path != NULL);
	time_t time_now = time(NULL);

	char fullpath[PATH_MAX];
	if (!realpath(path, fullpath))
		die("realpath:");

	path = fullpath;

	struct stat statbuf;

	// check if the path exists
	if (stat(path, &statbuf) < 0)
		die("%s doesn't exit:", path);

	struct trashinfo *trashinfo = create_trashinfo();

	// get the basename of path
	char *path_copy = malloc((strlen(path) + 1) * sizeof(char));
	strcpy(path_copy, path);
	char *trashfilesfilename = basename(path_copy);

	char buf[PATH_MAX];
	trashfilesfilename = getvalidtrashfilesfilename(trash,
							 trashfilesfilename,
							 buf, sizeof(buf));


	trashinfo->deletedfilepath = malloc((strlen(path) + 1) * sizeof(char));
	trashinfo->trashfilesfilepath = malloc(
			(strlen(trash->filesdirpath) + 1 +
		   strlen(trashfilesfilename) + 1) * sizeof(char));
	trashinfo->trashinfofilepath = malloc(
			(strlen(trash->infodirpath) + 1 +
			strlen(trashfilesfilename) +
			strlen(".trashinfo") + 1)
			* sizeof(char));
	sprintf(trashinfo->trashfilesfilepath,
		 "%s/%s", trash->filesdirpath, trashfilesfilename);
	sprintf(trashinfo->trashinfofilepath,
		 "%s/%s.trashinfo", trash->infodirpath, trashfilesfilename);
	strcpy(trashinfo->deletedfilepath, path);

	int deletiondatelen = 1024;
	trashinfo->deletiondate = malloc(deletiondatelen * sizeof(char));
	if (!strftime(trashinfo->deletiondate, deletiondatelen,
			   "%Y-%m-%dT%H:%M:%S", localtime(&time_now))) {
		die("strftime:");
	}

	writeinfofile(trashinfo);

	// move path into Trash/file directory
	if (rename(path, trashinfo->trashfilesfilepath) < 0)
		die("can't move the trash direcotry:");

	free_trashinfo(trashinfo);

	return 0;
}

void
trashrestore(Trash *trash, char *pattern)
{
	assert(0 && "trashrestore: not implemented");
	asserttrash(trash);
	assert(pattern != NULL);

}

void
trashlist(Trash *trash)
{
	asserttrash(trash);
	struct trashinfo *trashinfo;

	while ((trashinfo = readTrash(trash)) != NULL) {
		printf("%s %s\n", trashinfo->deletiondate, trashinfo->deletedfilepath);

		free_trashinfo(trashinfo);
	}
}

void
trashclean(Trash *trash)
{
	asserttrash(trash);

	struct trashinfo *trashinfo;
	while ((trashinfo = readTrash(trash)) != NULL) {
		if (remove(trashinfo->trashfilesfilepath) < 0)
			die("remove:");
		printf("removed %s\n", trashinfo->trashfilesfilepath);

		if (remove(trashinfo->trashinfofilepath) < 0)
			die("remove:");
		printf("removed %s\n", trashinfo->trashinfofilepath);
	}
}

void
trashremove(Trash *trash, char *pattern)
{
	asserttrash(trash);
	assert(pattern != NULL);

	struct trashinfo *trashinfo;

	while ((trashinfo = readTrash(trash)) != NULL) {
		char deletedfilepath_copy[strlen(trashinfo->deletedfilepath) + 1];
		strcpy(deletedfilepath_copy, trashinfo->deletedfilepath);
		char *deletedfilename = basename(deletedfilepath_copy);

		// TODO: support removing none empty directories from Trash
		if (!strcmp(deletedfilename, pattern)) {
			if (remove(trashinfo->trashfilesfilepath) < 0)
				die("remove:");
			printf("trashfilesfilepath: %s\n", trashinfo->trashfilesfilepath);

			if (remove(trashinfo->trashinfofilepath) < 0)
				die("remove:");
			printf("trashinfofilepath: %s\n", trashinfo->trashinfofilepath);

			printf("\n");
		}

		free_trashinfo(trashinfo);
	}
}
