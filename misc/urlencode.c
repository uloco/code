#include <err.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

bool keep_parens = true;
bool keep_slashes = false;
char *safe_chars = "";
char *unsafe_chars = "";
bool do_quoted_printable = false;

static void process_url(FILE *fp, char *fn) {
	int ch;
	bool safe;
	size_t pos = 0;

	while ((ch = getc(fp)) != EOF && ++pos) {
		switch (ch) {
		case 'A'...'Z':
		case 'a'...'z':
		case '0'...'9':
		case '_':
		case '.':
		case '!':
		case '~':
		case '*':
		case '\'':
		case ',':
		case '=':
		case '-':
			safe = true;
			break;
		case '(':
		case ')':
			safe = keep_parens;
			break;
		case ':':
		case '/':
			safe = keep_slashes;
			break;
		default:
			safe = false;
		}

		if (*unsafe_chars && strchr(unsafe_chars, ch))
			safe = false;
		else if (*safe_chars && strchr(safe_chars, ch))
			safe = true;

		if (safe)
			putchar(ch);
		else
			printf("%%%02X", ch);
	}
}

static void process_qp(FILE *fp, char *fh) {
	int ch;
	size_t line = 0;
	size_t pos = 0;

	while ((ch = getc(fp)) != EOF && ++pos) {
		switch (ch) {
		case '\n':
			putchar(ch);
			line = 0;
			break;
		case '\x20'...'\x3c':
		case '\x3e'...'\x7e':
			if (line > 80-1) {
				printf("=\n");
				line = 0;
			}
			putchar(ch);
			line += 1;
			break;
		default:
			if (line > 80-3) {
				printf("=\n");
				line = 0;
			}
			printf("=%02X", ch);
			line += 3;
		}
	}
}

static void process(FILE *fp, char *fh) {
	if (do_quoted_printable)
		process_qp(fp, fh);
	else
		process_url(fp, fh);
}

static int usage(void) {
	printf("Usage: urlencode [-a text] [-Pp] [files...]\n");
	printf("\n");
	printf("  -a TEXT   use TEXT as input rather than file/stdin\n");
	printf("  -P        treat ( ) as safe\n");
	printf("  -p        encode as path, treating / : as safe\n");
	printf("  -s BYTES  treat provided bytes as safe\n");
	printf("  -u BYTES  treat provided bytes as unsafe\n");
	printf("\n");
	return 2;
}

int main(int argc, char *argv[]) {
	int i, r = 0, opt;
	char *data = NULL;
	char *fn;
	FILE *fp;

	while ((opt = getopt(argc, argv, "a:PpQs:u:")) != -1) {
		switch (opt) {
		case 'a':
			data = optarg;
			break;
		case 'P':
			keep_parens = true;
			break;
		case 'p':
			keep_slashes = true;
			break;
		case 'Q':
			do_quoted_printable = true;
			break;
		case 's':
			safe_chars = optarg;
			break;
		case 'u':
			unsafe_chars = optarg;
			break;
		default:
			return usage();
		}
	}

	argc -= optind-1;
	argv += optind-1;

	if (data) {
		fp = fmemopen(data, strlen(data), "rb");
		process(fp, "stdin");
		fclose(fp);
	}
	else if (argc <= 1) {
		process(stdin, "stdin");
	}
	else {
		for (i = 1; i < argc; i++) {
			fn = argv[i];
			if (!strcmp(fn, "-"))
				fp = stdin, fn = "stdin";
			else
				fp = fopen(fn, "rb");
			if (!fp) {
				warn("failed to open %s", fn);
				r = 1;
				continue;
			}
			process(fp, fn);
			if (fp != stdin)
				fclose(fp);
		}
	}

	return r;
}
