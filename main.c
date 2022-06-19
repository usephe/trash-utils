#include <unistd.h>
#include <stdlib.h>

#include "util.h"
#include "trash.h"


char *arguments = "[-l] [file...]";

int
main(int argc, char *argv[])
{
	if (argc < 2)
		die("Usage: %s %s", argv[0], arguments);

	int opt;
	while ((opt = getopt(argc, argv, "lc")) != -1) {
		switch (opt) {
		case 'l':
			if (argc > 2)
				die("Unknown argument: %s", argv[2]);
			listtrash();
			exit(EXIT_SUCCESS);
			break;
		case 'c':
			if (argc > 2)
				die("Unknown argument: %s", argv[2]);
			cleantrash();
			exit(EXIT_SUCCESS);
			break;
		case '?':
			die("Usage: %s %s", argv[0], arguments);
		}
	}

	for (; optind < argc; optind++)
		trash(argv[optind]);

	return EXIT_SUCCESS;
}
