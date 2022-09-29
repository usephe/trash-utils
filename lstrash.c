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
			die("Usage: %s %s", argv[0], arguments);
			break;
		case '?':
			die("Usage: %s %s", argv[0], arguments);
		}
	}

	if (optind >= argc) {
		Trash *trash = opentrash(NULL);
		trashlist(trash);
		closetrash(trash);
	}

	for (; optind < argc; optind++) {
		printf("Trash: %s\n", argv[optind]);
		Trash *trash = opentrash(argv[optind]);
		trashlist(trash);
		closetrash(trash);
	}

	return EXIT_SUCCESS;
}
