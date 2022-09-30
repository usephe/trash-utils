#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#include "trash.h"
#include "util.h"

char *arguments = "[-h] [file...]";

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

	int opt;
	while ((opt = getopt(argc, argv, "h")) != -1) {
		switch (opt) {
		case 'h':
			show_help(argv[0]);
			break;
		case '?':
			show_help(argv[0]);
		}
	}

	Trash *trash = opentrash(NULL);
	for (; optind < argc; optind++)
		trashput(trash, argv[optind]);
	closetrash(trash);

	return EXIT_SUCCESS;
}
