/*
 * file - find type of a file or files - main program.
 *
 * Copyright (c) Ian F. Darwin, 1987.
 * Written by Ian F. Darwin.
 *
 * This software is not subject to any license of the American Telephone
 * and Telegraph Company or of the Regents of the University of California.
 *
 * Permission is granted to anyone to use this software for any purpose on
 * any computer system, and to alter it and redistribute it freely, subject
 * to the following restrictions:
 *
 * 1. The author is not responsible for the consequences of use of this
 *    software, no matter how awful, even if they arise from flaws in it.
 *
 * 2. The origin of this software must not be misrepresented, either by
 *    explicit claim or by omission.  Since few users ever read sources,
 *    credits must appear in the documentation.
 *
 * 3. Altered versions must be plainly marked as such, and must not be
 *    misrepresented as being the original software.  Since few users
 *    ever read sources, credits must appear in the documentation.
 *
 * 4. This notice may not be removed or altered.
 */
#ifndef	lint
static char *moduleid = 
	"@(#)$Id: file.c,v 1.36 1996/10/05 18:13:57 christos Exp $";
#endif	/* lint */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/param.h>	/* for MAXPATHLEN */
#include <sys/stat.h>
#include <fcntl.h>	/* for open() */
#if (__COHERENT__ >= 0x420)
#include <sys/utime.h>
#else
#include <utime.h>
#endif
#include <unistd.h>	/* for read() */

#include <netinet/in.h>		/* for byte swapping */

#include "patchlevel.h"
#include "file.h"

#ifdef S_IFLNK
# define USAGE  "Usage: %s [-vczL] [-f namefile] [-m magicfiles] file...\n"
#else
# define USAGE  "Usage: %s [-vcz] [-f namefile] [-m magicfiles] file...\n"
#endif

#ifndef MAGIC
# define MAGIC "/etc/magic"
#endif

int 			/* Global command-line options 		*/
	debug = 0, 	/* debugging 				*/
	lflag = 0,	/* follow Symlinks (BSD only) 		*/
	zflag = 0;	/* follow (uncompress) compressed files */

int			/* Misc globals				*/
	nmagic = 0;	/* number of valid magic[]s 		*/

struct  magic *magic;	/* array of magic entries		*/

char *magicfile;	/* where magic be found 		*/

char *progname;		/* used throughout 			*/
int lineno;		/* line number in the magic file	*/


static void	unwrap		__P((char *fn));
static int	byteconv4	__P((int, int, int));
static short	byteconv2	__P((int, int, int));

/*
 * main - parse arguments and handle options
 */
int
main(argc, argv)
int argc;
char *argv[];
{
	int c;
	int check = 0, didsomefiles = 0, errflg = 0, ret = 0, app = 0;

	if ((progname = strrchr(argv[0], '/')) != NULL)
		progname++;
	else
		progname = argv[0];

	if (!(magicfile = getenv("MAGIC")))
		magicfile = MAGIC;

	while ((c = getopt(argc, argv, "vcdf:Lm:z")) != EOF)
		switch (c) {
		case 'v':
			(void) fprintf(stdout, "%s-%d.%d\n", progname,
				       FILE_VERSION_MAJOR, patchlevel);
			return 1;
		case 'c':
			++check;
			break;
		case 'd':
			++debug;
			break;
		case 'f':
			if (!app) {
				ret = apprentice(magicfile, check);
				if (check)
					exit(ret);
				app = 1;
			}
			unwrap(optarg);
			++didsomefiles;
			break;
#ifdef S_IFLNK
		case 'L':
			++lflag;
			break;
#endif
		case 'm':
			magicfile = optarg;
			break;
		case 'z':
			zflag++;
			break;
		case '?':
		default:
			errflg++;
			break;
		}

	if (errflg) {
		(void) fprintf(stderr, USAGE, progname);
		exit(2);
	}

	if (!app) {
		ret = apprentice(magicfile, check);
		if (check)
			exit(ret);
		app = 1;
	}

	if (optind == argc) {
		if (!didsomefiles) {
			(void)fprintf(stderr, USAGE, progname);
			exit(2);
		}
	}
	else {
		int i, wid, nw;
		for (wid = 0, i = optind; i < argc; i++) {
			nw = strlen(argv[i]);
			if (nw > wid)
				wid = nw;
		}
		for (; optind < argc; optind++)
			process(argv[optind], wid);
	}

	return 0;
}


/*
 * unwrap -- read a file of filenames, do each one.
 */
static void
unwrap(fn)
char *fn;
{
	char buf[MAXPATHLEN];
	FILE *f;
	int wid = 0, cwid;

	if (strcmp("-", fn) == 0) {
		f = stdin;
		wid = 1;
	} else {
		if ((f = fopen(fn, "r")) == NULL) {
			error("Cannot open `%s' (%s).\n", fn, strerror(errno));
			/*NOTREACHED*/
		}

		while (fgets(buf, MAXPATHLEN, f) != NULL) {
			cwid = strlen(buf) - 1;
			if (cwid > wid)
				wid = cwid;
		}

		rewind(f);
	}

	while (fgets(buf, MAXPATHLEN, f) != NULL) {
		buf[strlen(buf)-1] = '\0';
		process(buf, wid);
	}

	(void) fclose(f);
}


/*
 * byteconv4
 * Input:
 *	from		4 byte quantity to convert
 *	same		whether to perform byte swapping
 *	big_endian	whether we are a big endian host
 */
static int
byteconv4(from, same, big_endian)
    int from;
    int same;
    int big_endian;
{
  if (same)
    return from;
  else if (big_endian)		/* lsb -> msb conversion on msb */
  {
    union {
      int i;
      char c[4];
    } retval, tmpval;

    tmpval.i = from;
    retval.c[0] = tmpval.c[3];
    retval.c[1] = tmpval.c[2];
    retval.c[2] = tmpval.c[1];
    retval.c[3] = tmpval.c[0];

    return retval.i;
  }
  else
    return ntohl(from);		/* msb -> lsb conversion on lsb */
}

/*
 * byteconv2
 * Same as byteconv4, but for shorts
 */
static short
byteconv2(from, same, big_endian)
	int from;
	int same;
	int big_endian;
{
  if (same)
    return from;
  else if (big_endian)		/* lsb -> msb conversion on msb */
  {
    union {
      short s;
      char c[2];
    } retval, tmpval;

    tmpval.s = (short) from;
    retval.c[0] = tmpval.c[1];
    retval.c[1] = tmpval.c[0];

    return retval.s;
  }
  else
    return ntohs(from);		/* msb -> lsb conversion on lsb */
}

/*
 * process - process input file
 */
void
process(inname, wid)
const char	*inname;
int wid;
{
	int	fd = 0;
	static  const char stdname[] = "standard input";
	unsigned char	buf[HOWMANY+1];	/* one extra for terminating '\0' */
	struct utimbuf  utbuf;
	struct stat	sb;
	int nbytes = 0;	/* number of bytes read from a datafile */
	char match = '\0';

	if (strcmp("-", inname) == 0) {
		if (fstat(0, &sb)<0) {
			error("cannot fstat `%s' (%s).\n", stdname,
			      strerror(errno));
			/*NOTREACHED*/
		}
		inname = stdname;
	}

	if (wid > 0)
	     (void) printf("%s:%*s ", inname, 
			   (int) (wid - strlen(inname)), "");

	if (inname != stdname) {
	    /*
	     * first try judging the file based on its filesystem status
	     */
	    if (fsmagic(inname, &sb) != 0) {
		    putchar('\n');
		    return;
	    }

	    if ((fd = open(inname, O_RDONLY)) < 0) {
		    /* We can't open it, but we were able to stat it. */
		    if (sb.st_mode & 0002) ckfputs("writeable, ", stdout);
		    if (sb.st_mode & 0111) ckfputs("executable, ", stdout);
		    ckfprintf(stdout, "can't read `%s' (%s).\n",
			inname, strerror(errno));
		    return;
	    }
	}


	/*
	 * try looking at the first HOWMANY bytes
	 */
	if ((nbytes = read(fd, (char *)buf, HOWMANY)) == -1) {
		error("read failed (%s).\n", strerror(errno));
		/*NOTREACHED*/
	}

	if (nbytes == 0)
		ckfputs("empty", stdout);
	else {
		buf[nbytes++] = '\0';	/* null-terminate it */
		match = tryit(buf, nbytes, zflag);
	}

#ifdef BUILTIN_ELF
	if (match == 's' && nbytes > 5)
		tryelf(fd, buf, nbytes);
#endif

	if (inname != stdname) {
		/*
		 * Try to restore access, modification times if read it.
		 */
		utbuf.actime = sb.st_atime;
		utbuf.modtime = sb.st_mtime;
		(void) utime(inname, &utbuf); /* don't care if loses */
		(void) close(fd);
	}
	(void) putchar('\n');
}


int
tryit(buf, nb, zflag)
unsigned char *buf;
int nb, zflag;
{
	/* try compression stuff */
	if (zflag && zmagic(buf, nb))
		return 'z';

	/* try tests in /etc/magic (or surrogate magic file) */
	if (softmagic(buf, nb))
		return 's';

	/* try known keywords, check whether it is ASCII */
	if (ascmagic(buf, nb))
		return 'a';

	/* abandon hope, all ye who remain here */
	ckfputs("data", stdout);
		return '\0';
}
