#include <unistd.h>
#include <stdlib.h>

#include "util.h"
#include "trash.h"


char *arguments = "[-lcr] [file...]";

int
main(int argc, char *argv[])
{
	if (argc < 2)
		die("Usage: %s %s", argv[0], arguments);

	Trash *trash = opentrash();

	int opt;
	while ((opt = getopt(argc, argv, "lcr")) != -1) {
		switch (opt) {
		case 'l':
			if (argc > 2)
				die("Unknown argument: %s", argv[2]);
			trashlist(trash);
			break;
		case 'c':
			if (argc > 2)
				die("Unknown argument: %s", argv[2]);
			trashclean(trash);
			break;
		case 'r':
			if (argc > 3)
				die("Unknown argument: %s", argv[3]);
			trashremove(trash, argv[2]);
			goto end;
			break;
		case '?':
			die("Usage: %s %s", argv[0], arguments);
		}
	}

	for (; optind < argc; optind++)
		trashput(trash, argv[optind]);

end:
	closetrash(trash);
	return EXIT_SUCCESS;
}
