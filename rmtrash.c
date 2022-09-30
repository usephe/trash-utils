#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#include "trash.h"
#include "util.h"

char *arguments = "[-ha] [PATTERN]";

void
show_help(char *program_name)
{
	die("Usage: %s %s", program_name, arguments);
}

int
main(int argc, char *argv[])
{

	if (argc < 2)
		show_help(argv[0]);

	int remove_all = 0;

	int opt;
	while ((opt = getopt(argc, argv, "ah")) != -1) {
		switch (opt) {
		case 'h':
			show_help(argv[0]);
			break;
		case 'a':
				remove_all = 1;
			break;
		case '?':
			show_help(argv[0]);
		}
	}

	if (remove_all && optind < argc) {
		printf("Unknown arguments '%s'\n", argv[optind]);
		show_help(argv[0]);
	}

	Trash *trash = opentrash(NULL);

	if (remove_all)
		trashclean(trash);
	else
		trashremove(trash, argv[1]);

	closetrash(trash);

	return EXIT_SUCCESS;
}
