/*
 * fortune.c
 * 4/12/90
 *
 * Display a random line from a file.
 * Suitable for automated fortune cookies.
 * Fortunes should be separated by "\n\n".
 */

#include <stdio.h>
#include <sys/timeb.h>

#define RAND_MAX 32767

char *fortfile = "/usr/games/lib/fortunes";

main(argc, argv) int argc; char *argv[];
{
	register int oc, c;
	register FILE *ifp;
	double randomAdj;
	struct timeb tb;

	if (argc > 1)
		fortfile = argv[1];

	if ((ifp = fopen(fortfile, "r")) == NULL) {
		fprintf(stderr, "%s: cannot open \"%s\"\n", argv[0], fortfile);
		exit(1);
	}

	fseek(ifp, 0L, 2);
	randomAdj = (double)ftell(ifp) / ((double)RAND_MAX);

	ftime(&tb);
	srand((unsigned int)(tb.time ^ tb.millitm));
	/*
	 * A user running this several times in quick succession will
	 * be dissapointed if he keeps getting the same result.
	 * Running rand() 100 times distributes the low order bit
	 * changes in the seed to the high half returned by rand().
	 */
	for (c = 0; c < 100; c++)
		rand();

	fseek(ifp, (long)(randomAdj * (double)rand()), 0);
	for (oc = 0; (c = fgetc(ifp)) != EOF; oc = c)
		if (oc == '\n' && c == '\n')
			break;
	if (c == EOF || (c = fgetc(ifp)) == EOF)
		fseek(ifp, 0L, 0);
	else
		ungetc(c, ifp);
	for (oc = 0; (c = fgetc(ifp)) != EOF; oc = c) {
		if (oc == '\n' && c == '\n')
			break;
		else
			putchar(c);
	}
	exit(0);
}
