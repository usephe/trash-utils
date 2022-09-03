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
	char *filepath;
	char *original_location;
	char *deletiondate;
	int isvalid;
};

struct trash {
	char *trashpath;
	char *filesdir;
	char *infodir;
};

int
getenv_or_default(char *buf, const char *env, const char *default_val) {
	const char *xdgdata = getenv(env);
	int ret;
	if (!xdgdata || !xdgdata[0])
		ret = sprintf(buf, "%s/%s", getenv("HOME"), default_val);
	else
		ret = sprintf(buf, "%s", xdgdata);

	return ret;
}

void
asserttrash(Trash *trash)
{
	assert(trash != NULL);
	assert(trash->filesdir != NULL && trash->infodir != NULL);
}

Trash *
opentrash()
{
	Trash *trash = malloc(sizeof(*trash));
	trash->trashpath = NULL;

	char buf[PATH_MAX];
	trash->trashpath = malloc(PATH_MAX * sizeof(*trash->trashpath));
	struct stat statbuf;


	char *xdgdata = buf;
	if (getenv_or_default(xdgdata, "XDG_DATA_HOME", "/.local/share") < 0)
		die("getenv_or_default:");

	if (stat(xdgdata, &statbuf) < 0)
		die("%s: doesn't exits", trash->trashpath);

	sprintf(trash->trashpath, "%s/Trash", xdgdata);

	if (stat(trash->trashpath, &statbuf) < 0) {
		if (mkdir(trash->trashpath, 0755) < 0)
			die("cannot make trash home:");
	} else if (!S_ISDIR(statbuf.st_mode))
		die("cannot access Trash directory");

	sprintf(buf, "%s/files", trash->trashpath);
	if (stat(buf, &statbuf) < 0) {
		if (mkdir(buf, 0700) < 0)
			die("cannot make trash files:");
	} else if (!S_ISDIR(statbuf.st_mode))
		die("cannot access Trash/files directory");

	trash->filesdir = malloc((strlen(buf) + 1) * sizeof(*trash->filesdir));
	strcpy(trash->filesdir, buf);

	sprintf(buf, "%s/info", trash->trashpath);
	if (stat(buf, &statbuf) < 0) {
		if (mkdir(buf, 0700) < 0)
			die("cannot make trash info:");
	} else if (!S_ISDIR(statbuf.st_mode))
		die("cannot access Trash/info directory");

	trash->infodir = malloc((strlen(buf) + 1) * sizeof(*trash->infodir));
	strcpy(trash->infodir, buf);

	return trash;
}

void
closetrash(Trash *trash)
{
	asserttrash(trash);

	free(trash->filesdir);
	free(trash->infodir);
	free(trash->trashpath);
	free(trash);
}

struct trashinfo *
create_trashinfo()
{
	struct trashinfo *trashinfo = malloc(sizeof(*trashinfo));
	if (!trashinfo)
		die("malloc:");
	trashinfo->filepath = trashinfo->deletiondate = NULL;
	trashinfo->isvalid = 0;

	return trashinfo;
}

void
free_trashinfo(struct trashinfo *trashinfo)
{
	free(trashinfo->filepath);
	free(trashinfo->deletiondate);

	free(trashinfo);
}

int
writeinfofile(FILE *stream, const char *path)
{
	assert(stream != NULL && path != NULL);
	time_t time_now = time(NULL);
	char buf[1024];
	int result;

	if (!strftime(buf, sizeof(buf), "%Y-%m-%dT%H:%M:%S", localtime(&time_now))) {
		return -1;
	}

	result = fprintf(stream, "[Trash Info]\n"
				"Path=%s\n"
				"DeletionDate=%s\n",
				path, buf);

	return result;
}

int
trashput(Trash *trash, const char *path)
{
	asserttrash(trash);
	assert(path != NULL);

	struct stat statbuf;

	// check if the path exists
	if (stat(path, &statbuf) < 0)
		die("%s doesn't exit:", path);

	// get the basename of path
	char pathcpy[strlen(path) + 1];
	strcpy(pathcpy, path);
	char *filename = basename(pathcpy);

	// make the new path which path get moved to
	char trashfilepath[PATH_MAX];
	char trashinfopath[PATH_MAX];
	sprintf(trashfilepath, "%s/files/%s", trash->trashpath, filename);
	sprintf(trashinfopath, "%s/info/%s.trashinfo", trash->trashpath, filename);

	// if trashfilepath or trashinfopath exits make a new ones
	int i = 1;
	while (stat(trashfilepath, &statbuf) == 0 || stat(trashinfopath, &statbuf) == 0) {
		sprintf(trashfilepath + strlen(trashfilepath), "_%d", i);
		trashinfopath[strlen(trashinfopath) - strlen(".trashinfo")] = '\0';
		sprintf(trashinfopath + strlen(trashinfopath), "_%d.trashinfo", i);
		if (strlen(trashinfopath) >= PATH_MAX)
			die("file name is too long");
		i++;
	}

	FILE *infofile = fopen(trashinfopath, "w");
	if (!infofile) {
		die("can't open file %s:", trashinfopath);
	}

	char fullpath[PATH_MAX];
	if (!realpath(path, fullpath))
		die(":");

	writeinfofile(infofile, fullpath);

	// move path into Trash/file directory
	if (rename(path, trashfilepath) < 0)
		die("can't move the trash direcotry:");

	fclose(infofile);

	return 0;
}

struct trashinfo *
readinfofile(FILE *infofile)
{
	struct trashinfo *trashinfo = create_trashinfo();
	struct stat statbuf;

	int fd = fileno(infofile);
	if (fd < 0)
		return NULL;

	if (fstat(fd, &statbuf) < 0)
		die("stat:");

	if (!S_ISREG(statbuf.st_mode))
		return NULL;


	char *filepath = NULL;
	char *deletiondate = NULL;

	char *line = NULL;
	size_t len = 0;
	ssize_t nread;
	while ((nread = getline(&line, &len, infofile)) != -1) {
		if (strncmp(line, "Path=", strlen("Path=")) == 0) {
			filepath = malloc((nread + 1) * sizeof(*filepath));
			strcpy(filepath, line);
		} else if (strncmp(line, "DeletionDate=", strlen("DeletionDate=")) == 0) {
			deletiondate = malloc((nread + 1) * sizeof(*filepath));
			strcpy(deletiondate, line);
		}
	};
	free(line);

	if (!filepath || !deletiondate)
		return NULL;

	if (filepath[strlen(filepath) - 1] == '\n')
		filepath[strlen(filepath) - 1] = '\0';

	if (deletiondate[strlen(deletiondate) - 1] == '\n')
		deletiondate[strlen(deletiondate) - 1] = '\0';

	// Remove the prefix Path= and DeletionDate=
	memmove(filepath, filepath + strlen("Path="), strlen(filepath) - strlen("Path=") + 1);
	memmove(deletiondate,
			deletiondate + strlen("DeletionDate="),
			strlen(deletiondate) - strlen("DeletionDate=") + 1);

	trashinfo->filepath = filepath;
	trashinfo->deletiondate = deletiondate;

	return trashinfo;
}

int
strendswith(char *str, char *suffix)
{
	assert(str != NULL && suffix != NULL);
	int srclen = strlen(str);
	int suffixlen = strlen(suffix);

	if (suffixlen > srclen)
		return 0;

	return strcmp(str + (srclen - suffixlen), suffix) == 0;
}


void
trashlist(Trash *trash)
{
	asserttrash(trash);

	char infofilepath[PATH_MAX];
	struct dirent *dp;
	struct trashinfo *trashinfo;
	FILE *infofile;

	DIR *infodir = opendir(trash->infodir);
	if (!infodir)
		die("opendir:");

	errno = 0;
	while ((dp = readdir(infodir)) != NULL) {
		if (strcmp(dp->d_name, ".") == 0 || strcmp(dp->d_name, "..") == 0)
			continue;

		if (!strendswith(dp->d_name, ".trashinfo"))
			continue;

		sprintf(infofilepath, "%s/%s", trash->infodir, dp->d_name);

		infofile = fopen(infofilepath, "r");
		if (!infofile)
			die("fopen:");

		errno = 0;
		trashinfo = readinfofile(infofile);
		if (!trashinfo) {
			if (errno != 0)
				die("readinfofile:");
			else
				die("readinfofile");
		}

		printf("%s %s\n", trashinfo->deletiondate, trashinfo->filepath);

		free_trashinfo(trashinfo);
		fclose(infofile);

		errno = 0;
	}

	if (errno != 0)
		die("readdir:");

	closedir(infodir);
}

void
trashclean(Trash *trash)
{
	asserttrash(trash);

	char filepath[PATH_MAX];
	char filename[PATH_MAX];
	struct dirent *dp;

	DIR *infodir = opendir(trash->infodir);
	if (!infodir)
		die("opendir:");

	errno = 0;
	while ((dp = readdir(infodir)) != NULL) {
		if (!strcmp(dp->d_name, ".") || !strcmp(dp->d_name, ".."))
			continue;

		if (!strendswith(dp->d_name, ".trashinfo"))
			continue;

		// get the file name without the .trashinfo extension
		int filenamelen = strlen(dp->d_name) - strlen(".trashinfo");
		strncpy(filename, dp->d_name, filenamelen);
		filename[filenamelen] = '\0';

		sprintf(filepath, "%s/%s.trashinfo", trash->infodir, filename);
		if (remove(filepath) < 0)
			die("remove:");
		printf("%s\n", filepath);

		sprintf(filepath, "%s/%s", trash->filesdir, filename);
		if (remove(filepath) < 0)
			die("remove:");
		printf("%s\n", filepath);

		errno = 0;
	}

	if (errno != 0)
		die("readdir:");

	closedir(infodir);
}

void
trashremove(Trash *trash, char *pattern)
{
	asserttrash(trash);
	assert(pattern != NULL);

	char infofilepath[PATH_MAX];
	char filepath[PATH_MAX];
	char filename[PATH_MAX];

	struct dirent *dp;
	struct trashinfo *trashinfo;
	FILE *infofile;

	DIR *infodir = opendir(trash->infodir);
	if (!infodir)
		die("opendir:");

	errno = 0;
	while ((dp = readdir(infodir)) != NULL) {
		if (strcmp(dp->d_name, ".") == 0 || strcmp(dp->d_name, "..") == 0)
			continue;

		if (!strendswith(dp->d_name, ".trashinfo"))
			continue;

		// get the file name without the .trashinfo extension
		int filenamelen = strlen(dp->d_name) - strlen(".trashinfo");
		strncpy(filename, dp->d_name, filenamelen);
		filename[filenamelen] = '\0';

		sprintf(infofilepath, "%s/%s", trash->infodir, dp->d_name);
		sprintf(filepath, "%s/%s", trash->filesdir, filename);

		infofile = fopen(infofilepath, "r");
		if (!infofile)
			die("fopen:");

		trashinfo = readinfofile(infofile);

		errno = 0;
		if (!trashinfo) {
			if (errno != 0)
				die("readinfofile:");
			else
				die("readinfofile");
		}

		char *filep = strdup(trashinfo->filepath);
		char *original_filename = basename(filep);

		// TODO: support removing none empty directories from Trash
		if (!strcmp(original_filename, pattern)) {
			if (remove(filepath) < 0)
				die("remove:");
			printf("trashfile path: %s\n", filepath);

			if (remove(infofilepath) < 0)
				die("remove:");
			printf("trashinfo path: %s\n", infofilepath);

			printf("\n");
		}

		free(filep);
		free_trashinfo(trashinfo);
		fclose(infofile);

		errno = 0;
	}

	if (errno != 0)
		die("readdir:");

	closedir(infodir);
}
