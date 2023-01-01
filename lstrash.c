#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#include "trash.h"
#include "util.h"

char *arguments = "[-h] [TRASHDIR]";

void
show_help(char *program_name)
{
	die("Usage: %s %s", program_name, arguments);
}

int
main(int argc, char *argv[])
{
	int opt;
	while ((opt = getopt(argc, argv, "h")) != -1) {
		switch (opt) {
		case 'h':
			if (argc > 2)
				die("Unknown argument: %s", argv[2]);
			show_help(argv[0]);
			break;
		case '?':
			show_help(argv[0]);
		}
	}

	Trash *trash;
	if (optind >= argc) {
		trash = opentrash(NULL);
		trashlist(trash);
	}

	for (; optind < argc; optind++) {
		printf("Trash: %s\n", argv[optind]);
		trash = opentrash(argv[optind]);
		trashlist(trash);
	}

	closetrash(trash);

	return EXIT_SUCCESS;
}
