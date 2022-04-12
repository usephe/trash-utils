#include <assert.h>
#include <libgen.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <time.h>

#include <linux/limits.h>

#include "util.h"
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
