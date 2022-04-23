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

int
ftrashinfo(FILE *stream, const char *path)
{
	time_t time_now = time(NULL);
	char buf[1024];

	if (!strftime(buf, sizeof(buf), "%Y-%m-%dT%H:%M:%S", localtime(&time_now))) {
		return -1;
	}

	if (fprintf(stream,
			"[Trash Info]\n"
			"Path=%s\n"
			"DeletionDate=%s\n",
			path, buf
	       ) < 0)
		die("Can't write to file:");

	return 0;
}

char *
trashinit()
{
	char buf[PATH_MAX];
	char *trash_home = malloc(PATH_MAX * sizeof(*trash_home));
	char *xdgdata = getenv("XDG_DATA_HOME");
	struct stat statbuf;

	if (!xdgdata) {
		const char *home = getenv("HOME");
		sprintf(xdgdata, "%s/%s", home, ".local/share");
	}

	if (stat(xdgdata, &statbuf) < 0)
		die("%s: doesn't exits", xdgdata);

	sprintf(trash_home, "%s/Trash", xdgdata);
	trash_home = realloc(trash_home, strlen(trash_home) + 1);

	if (stat(trash_home, &statbuf) < 0) {
		if (mkdir(trash_home, 0755) < 0)
			die("cannot make trash home");
	} else if (!S_ISDIR(statbuf.st_mode))
		die("cannot access Trash directory");

	sprintf(buf, "%s/files", trash_home);
	if (stat(buf, &statbuf) < 0) {
		if (mkdir(buf, 0700) < 0)
			die("cannot make trash files");
	} else if (!S_ISDIR(statbuf.st_mode))
		die("cannot access Trash/files directory");

	sprintf(buf, "%s/info", trash_home);

	if (stat(buf, &statbuf) < 0) {
		if (mkdir(buf, 0700) < 0)
			die("cannot make trash info");
	} else if (!S_ISDIR(statbuf.st_mode))
		die("cannot access Trash/info directory");

	return trash_home;
}

int
trash(const char *path)
{
	assert(path != NULL);

	struct stat statbuf;

	// check if the path exists
	if (stat(path, &statbuf) < 0)
		die("%s doesn't exit:", path);

	// get the basename of path
	char pathcpy[strlen(path) + 1];
	strcpy(pathcpy, path);
	char *filename = basename(pathcpy);

	// get the path of the Trash derctory if it doesn't exit create it
	char *trashhome = trashinit();

	// make the new path which path get moved to
	char trashfilepath[PATH_MAX];
	char trashinfopath[PATH_MAX];
	sprintf(trashfilepath, "%s/files/%s", trashhome, filename);
	sprintf(trashinfopath, "%s/info/%s.trashinfo", trashhome, filename);

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

	ftrashinfo(infofile, fullpath);

	// move path into Trash/file directory
	if (rename(path, trashfilepath) < 0)
		die("can't move the trash direcotry:");

	free(trashhome);

	fclose(infofile);

	return 0;
}

void
readtrashinfo(const char *path)
{
	struct stat statbuf;

	if (stat(path, &statbuf) < 0)
		die("stat:");

	if (!S_ISREG(statbuf.st_mode))
		die("%s is not a regular file", path);

	FILE *infofile = fopen(path, "r");
	if (!infofile)
		die("fopen:");

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
		die("invalid info: %s", path);

	if (filepath[strlen(filepath) - 1] == '\n')
		filepath[strlen(filepath) - 1] = '\0';

	if (deletiondate[strlen(deletiondate) - 1] == '\n')
		deletiondate[strlen(deletiondate) - 1] = '\0';

	// Remove the prefix Path= and DeletionDate=
	memmove(filepath, filepath + strlen("Path="), strlen(filepath) - strlen("Path=") + 1);
	memmove(deletiondate,
			deletiondate + strlen("DeletionDate="),
			strlen(deletiondate) - strlen("DeletionDate=") + 1);

	printf("%s %s\n", deletiondate, filepath);

	free(filepath);
	free(deletiondate);
	fclose(infofile);
}

void
listtrash()
{
	char *trash_home = trashinit();
	char infodirpath[PATH_MAX];
	char infofilepath[PATH_MAX];
	struct dirent *dp;

	sprintf(infodirpath, "%s/info", trash_home);

	DIR *infodir = opendir(infodirpath);
	if (!infodir)
		die("opendir:");

	errno = 0;
	while ((dp = readdir(infodir)) != NULL) {
		if (strcmp(dp->d_name, ".") == 0 || strcmp(dp->d_name, "..") == 0)
			continue;

		sprintf(infofilepath, "%s/info/%s", trash_home, dp->d_name);

		readtrashinfo(infofilepath);
	}

	if (errno != 0)
		die("readdir:");


	free(trash_home);
	closedir(infodir);
}

int
main(int argc, char *argv[])
{
	if (argc < 2) {
		fprintf(stderr, "Usage: %s [filename]\n", argv[0]);
		exit(EXIT_FAILURE);
	}

	trash(argv[1]);

	return EXIT_SUCCESS;
}
