/*  nsfid - Play routine/driver identifier for binary rips of computer music
 *  Copyright (C) 2020 karmic <karmic.c64@gmail.com>
 *
 *  This program is free software: you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation, either version 3 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program.  If not, see <https://www.gnu.org/licenses/>.
 */

#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <ctype.h>
#include <limits.h>
#include <errno.h>
#include <dirent.h>
#include <sys/stat.h>

#define END -1
#define WILD -2
#define AND -3
#define RANGEWILD 0x100

#define MAX_SIG_LEN 256

typedef struct
{
	char* name;
	int sigc;
	int** sigv;
	int found;
} driver_t;

driver_t** driverv = NULL;
int driverc = 0;

char** scanv = NULL;
int scanc = 0;

char* cfgname = NULL;

char** filetypev = NULL;
int filetypec = 0;

int verbose = 0;
int allfiles = 0;
int recurse = 1;
int listunident = 0;
int listident = 1;

char** rdriverv = NULL;
int rdriverc = 0;

char** sdriverv = NULL;
int sdriverc = 0;

int scanned = 0;
int ident = 0;
int unident = 0;

int iseq(unsigned char a, int b)
{
	if (b == WILD) return 1;
	return a == b;
}

unsigned char* hunt(unsigned char* haystack, size_t hlen, const int* needle, size_t nlen)
{
	unsigned char nfirst = *needle;
	unsigned char* end = haystack+hlen-nlen;
	for ( ; haystack <= end; haystack++)
	{
		if (iseq(*haystack, nfirst))
		{
			for (int i = 1; i < nlen; i++)
			{
				if (!iseq(haystack[i], needle[i])) goto not_found;
			}
			return haystack;
		}
not_found:
		continue;
	}
	return NULL;
}

void* xmalloc(int size)
{
	void* p = malloc(size);
	if (!p)
	{
		puts("Out of memory");
		exit(EXIT_FAILURE);
	}
	return p;
}

void* xrealloc(void* ptr, int newsize)
{
	void* p = realloc(ptr, newsize);
	if (!p)
	{
		puts("Out of memory");
		exit(EXIT_FAILURE);
	}
	return p;
}

int tohex(const char c)
{
	if (c >= '0' && c <= '9') return c - '0';
	if (c >= 'a' && c <= 'f') return c - 'a' + 0xa;
	if (c >= 'A' && c <= 'F') return c - 'A' + 0xa;
	printf("Can't convert character '%c' to hexadecimal digit\n", c);
	exit(EXIT_FAILURE);
}

int israngewild(const int c)
{
	return c>>8 == RANGEWILD>>8;
}

int iswild(const int c)
{
	if (c == WILD) return 1;
	if (c == AND) return 1;
	if (israngewild(c)) return 1;
	return 0;
}


void scan_file(const char* name)
{
	FILE* f;
	if (filetypec && !allfiles)
	{
		const char* ext = strrchr(name, '.');
		if (!ext++) return;
		for (int i = 0; i < filetypec; i++)
		{
			if (!strcasecmp(ext, filetypev[i]))
				goto correct_extension;
		}
		return;
	}
correct_extension:
	f = fopen(name, "rb");
	if (!f)
	{
		printf("Couldn't open \"%s\" - %s\n", name, strerror(errno));
		return;
	}
	scanned++;
	fseek(f, 0, SEEK_END);
	int size = ftell(f);
	unsigned char* buf = xmalloc(size);
	rewind(f);
	fread(buf, 1, size, f);
	fclose(f);
	
	int foundc = 0;
	int rfoundc = 0; /* found drivers which may be listed */
	
	int foundsig = 0;
	
	unsigned char* end = buf+size;
	
	for (int d = 0; d < driverc; d++)
	{
		unsigned char* firstfound;
		for (int s = 0; s < driverv[d]->sigc; s++)
		{
			unsigned char* b = buf;
			firstfound = NULL;
			unsigned char* prvstrend = buf;
			int* sig = driverv[d]->sigv[s];
			int* str = sig;
			int slen = 0;
			int mindist = 0;
			int nextmindist = 0;
			int maxdist = INT_MAX;
			int nextmaxdist = INT_MAX;
			int newstr = 0;
			while (1)
			{
				int c = *sig++;
				if (c == END)
				{
					newstr = 2;
				}
				else if (c == AND)
				{
					newstr = 1;
					nextmaxdist = INT_MAX;
				}
				else if (israngewild(c))
				{
					newstr = 1;
					if (nextmaxdist == INT_MAX) nextmaxdist = 0;
					while (israngewild(*sig))
					{
						nextmindist += (*sig>>4) & 0xf;
						nextmaxdist += *sig & 0xf;
						sig++;
					}
					nextmindist += (c>>4) & 0xf;
					nextmaxdist += c & 0xf;
				}
				else
				{
					slen++;
				}
				
				if (newstr)
				{
					unsigned char* p = hunt(b, end-b, str, slen);
					if (!p) break;
					if (!firstfound) firstfound = p;
					int dist = p - prvstrend;
					if (dist < mindist || dist > maxdist)
					{   /* go back to start and search again */
						b = firstfound+1;
						firstfound = NULL;
						sig = driverv[d]->sigv[s];
					}
					else
					{
						if (newstr > 1) 
						{
							foundsig = s;
							goto found_driver;
						}
						
						b = p+slen;
						prvstrend = b;
					}
					str = sig;
					slen = 0;
					mindist = nextmindist;
					nextmindist = 0;
					maxdist = nextmaxdist;
					nextmaxdist = INT_MAX;
					newstr = 0;
				}
			}
		}
		/* not found, check next driver */
		continue;
found_driver:
		if (listident)
		{
			int rfound = 0;
			if (rdriverc > 0)
			{
				for (int i = 0; i < rdriverc; i++)
				{
					if (!strcasecmp(rdriverv[i], driverv[d]->name))
					{
						rfound++;
						break;
					}
				}
			}
			else
			{
				rfound++;
			}
			if (rfound)
			{
				if (rfoundc)
					printf("%-58.57s ", "");
				else
					printf("%-58.57s ", name);
				
				printf(driverv[d]->name);
				if (verbose)
					printf("  (id %i at 0x%x)", foundsig, (unsigned int)(firstfound-buf));
				putc('\n', stdout);
				rfoundc++;
			}
		}
		foundc++;
		driverv[d]->found++;
	}
	
	free(buf);
	
	if (foundc)
	{
		ident++;
	}
	else
	{
		unident++;
		if (listunident)
			printf("%-58.57s *Unidentified*\n", name);
	}
}


typedef struct {
	char * name;
	mode_t st_mode;
} nsfid_dirent_t;

int nsfid_dirent_cmp(const void * a, const void * b) {
	const nsfid_dirent_t * aa = (const nsfid_dirent_t *)a;
	const nsfid_dirent_t * bb = (const nsfid_dirent_t *)b;
	
	// directories always first
	int dc = S_ISDIR(bb->st_mode) - S_ISDIR(aa->st_mode);
	if (dc)
		return dc;
	else
		return strcmp(aa->name, bb->name);
}

void scan_dir(const char* name)
{
	DIR* dir = opendir(name ? name : ".");
	if (!dir)
	{
		printf("%s \"%s\"\n", strerror(errno), name);
		return;
	}
	
	int namlen;
	if (name)
		namlen = strlen(name);
		
	// read in directory
	int dir_len = 0;
	int dir_max = 512;
	nsfid_dirent_t * d = xmalloc(dir_max * sizeof(*d));
	
	struct dirent* de;
	while ((de = readdir(dir)))
	{
		char* fnam = de->d_name;
		if (!strcmp(fnam, ".") || !strcmp(fnam, "..")) continue;
		char* fullname;
		if (name)
		{
			int fnamlen = strlen(fnam);
			fullname = xmalloc(namlen+fnamlen+2);
			strcpy(fullname, name);
			fullname[namlen] = '/';
			strcpy(fullname+namlen+1, fnam);
		}
		else
		{
			fullname = strdup(fnam);
		}
		
		struct stat st;
		stat(fullname, &st);
		if ((S_ISDIR(st.st_mode) && recurse) || S_ISREG(st.st_mode))
		{
			if (dir_len == dir_max) {
				dir_max *= 2;
				d = xrealloc(d, dir_max * sizeof(*d));
			}
			
			d[dir_len].name = fullname;
			d[dir_len].st_mode = st.st_mode;
			dir_len++;
		}
		else
		{
			free(fullname);
		}
	}
	closedir(dir);
	
	// sort directory
	qsort(d, dir_len, sizeof(*d), nsfid_dirent_cmp);
	
	// process files/folders
	for (int i = 0; i < dir_len; i++) {
		if (S_ISDIR(d[i].st_mode)) {
			scan_dir(d[i].name);
		} else {
			scan_file(d[i].name);
		}
		
		free(d[i].name);
	}
	
	// done
	free(d);
}

void addfiletype(char* type)
{
	int len = strlen(type);
	for (int i = 0; i < len; i++)
	{
		if (!isalnum(*(type+i)))
		{
			printf("Invalid file type name '%s'\n", type);
			exit(EXIT_FAILURE);
		}
	}
	for (int i = 0; i < len; i++)
		type[i] = tolower(type[i]);
	for (int i = 0; i < filetypec; i++)
	{
		if (!strcmp(filetypev[i], type))
		{
			printf("Duplicate file type '%s'\n", type);
			exit(EXIT_FAILURE);
		}
	}
	filetypev = xrealloc(filetypev, (filetypec+1)*sizeof(*filetypev));
	filetypev[filetypec++] = type;
}

void dohelp(void)
{
	puts(
		"nsfid - Play routine/driver identifier for binary rips of computer music\n"
		"Copyright (C) 2020 karmic <karmic.c64@gmail.com>\n\n"
		"Usage: nsfid [option]... [dir/file]...\n\n"
		"Options:\n"
		" -a                         always scan all file types\n"
		" -c <cfgfile>               specify config file\n"
		" -d                         disable recursively scanning subdirectories\n"
		" -f <type>[,<type>...]      only scan these file types\n"
		" -o                         only report unidentified files\n"
		" -r <driver>[,<driver>...]  only report these drivers\n"
		" -s <driver>[,<driver>...]  only scan these drivers\n"
		" -u                         also report unidentified files\n"
		" -v                         enable verbose mode\n"
		" -?, --help                 display this help message"
		);
	exit(EXIT_SUCCESS);
}

void split_driver_list(char * list, char *** driverv, int * driverc) {
	char* end = strchr(list, '\0');
  char* cur = list;
  while (cur < end)
  {
	  char* next = strchr(cur, ',');
	  if (!next)
	  {
		  next = end;
	  }
	  while (isspace(*cur)) cur++;
	  char* p = next;
	  while (--p >= cur && isspace(*p));
	  *(p+1) = '\0';
	  if (p >= cur)
	  {
		  (*driverv) = xrealloc((*driverv), ((*driverc)+1)*sizeof(**driverv));
		  (*driverv)[(*driverc)++] = cur;
		  while (p >= cur)
		  {
			  if (!isgraph(*(p--)))
			  {
				  printf("Invalid driver name '%s'\n", cur);
				  exit(EXIT_FAILURE);
			  }
		  }
	  }
	  cur = next+1;
  }
}

int main(int argc, char* argv[])
{
	if (argc < 2) dohelp();
	for (int i = 1; i < argc; i++)
		if (!strcmp(argv[i], "--help")) dohelp();
	opterr = 0;
	char c;
	while ((c = getopt(argc, argv, "-:ac:df:or:s:uv")) != -1)
	{
		switch (c)
		{
			case '?':
				if (optopt == '?') dohelp();
				printf("Unknown option -%c\n", optopt);
				exit(EXIT_FAILURE);
			case ':':
				printf("Option -%c expects an argument\n", optopt);
				exit(EXIT_FAILURE);
			case 'a':
				allfiles = 1;
				break;
			case 'c':
				if (cfgname)
				{
					puts("Multiple config files defined");
					exit(EXIT_FAILURE);
				}
				cfgname = optarg;
				break;
			case 'd':
				recurse = 0;
				break;
			case 'f':
				{
					char* end = strchr(optarg, '\0');
					char* cur = optarg;
					while (cur < end)
					{
						char* next = strchr(cur, ',');
						if (!next)
						{
							next = end;
						}
						int len = next-cur;
						while (isspace(*cur))
						{
							cur++;
							len--;
						}
						char* p = next;
						while (--p >= cur && isspace(*p))
						{
							len--;
						}
						addfiletype(cur);
						cur = next + 1;
					}
				}
				break;
			case 'o':
				listunident = 1;
				listident = 0;
				break;
			case 'r':
				split_driver_list(optarg, &rdriverv, &rdriverc);
				break;
			case 's':
				split_driver_list(optarg, &sdriverv, &sdriverc);
				break;
			case 'u':
				listunident = 1;
				listident = 1;
				break;
			case 'v':
				verbose = 1;
				break;
			case '\1':
				scanv = xrealloc(scanv, (scanc+1)*sizeof(*scanv));
				scanv[scanc++] = optarg;
				break;
		}
	}
	
	if (!cfgname)
	{
		puts("No configuration file defined");
		exit(EXIT_FAILURE);
	}
	FILE* cfg = fopen(cfgname, "rb");
	int fail = 0;
	if (!cfg)
	{
		fail++;
		char* oldname = cfgname;
		int oldlen = strlen(oldname);
		cfgname = xmalloc(oldlen + sizeof(".cfg"));
		strcpy(cfgname, oldname);
		strcpy(cfgname+oldlen, ".cfg");
		cfg = fopen(cfgname, "rb");
		if (!cfg)
		{
			printf("No such config file '%s'\n", oldname);
			exit(EXIT_FAILURE);
		}
	}
	printf("Reading configuration file %s\n", cfgname);
	if (fail)
		free(cfgname);
	fseek(cfg, 0, SEEK_END);
	int fsize = ftell(cfg);
	if (fsize < 1)
	{
		if (fsize < 0)
			puts("Could not get size of configuration file");
		else
			puts("Config file is blank");
		exit(EXIT_FAILURE);
	}
	char* cfgbuf = xmalloc(fsize+1);
	rewind(cfg);
	fread(cfgbuf, 1, fsize, cfg);
	fclose(cfg);
	cfgbuf[fsize] = '\0';
	for (char* p = cfgbuf; p < cfgbuf+fsize; p++) /* clean out comments */
	{
		if (*p == '#')
		{
			while (!(isspace(*p) && !isblank(*p)) && p < cfgbuf+fsize)
			{
				*p = ' ';
				p++;
			}
		}
	}
	const char delim[] = " \f\n\r\t\v";
	char* tok = strtok(cfgbuf, delim);
	if (!tok)
	{
		puts("Config file is blank");
		exit(EXIT_FAILURE);
	}
	int found_filetypes = 0;
	driver_t* d = NULL;
	int skip_this_driver = 0;	// due to not being present in -s arg
	int sigbuf[MAX_SIG_LEN];
	int sigpos = 0;
	while (tok)
	{
		/* filetypes */
		if (!strcmp(tok, "FILETYPES"))
		{
			if (found_filetypes)
			{
				puts("Multiple FILETYPES directives");
				exit(EXIT_FAILURE);
			}
			tok = strtok(NULL, delim);
			if (filetypec || allfiles) /* command line overrides */
			{
				while (strcmp(tok, "END"))
				{
					if (!tok)
					{
						puts("FILETYPES directive with no END");
						exit(EXIT_FAILURE);
					}
					tok = strtok(NULL, delim);
				}
			}
			else
			{
				while (strcmp(tok, "END"))
				{
					if (!tok)
					{
						puts("FILETYPES directive with no END");
						exit(EXIT_FAILURE);
					}
					addfiletype(tok);
					tok = strtok(NULL, delim);
				}
			}
			found_filetypes++;
		}
		/* sig tokens */
		else if (!strcmp(tok, "END"))
		{
			if (!d)
			{
				puts("Driver signature with no name");
				exit(EXIT_FAILURE);
			}
			if (!sigpos)
			{
				printf("Blank driver signature in %s\n", d->name);
				exit(EXIT_FAILURE);
			}
			if (iswild(sigbuf[sigpos-1]))
			{
				printf("Driver signature cannot end with wildcard in %s\n", d->name);
				exit(EXIT_FAILURE);
			}
			if (sigpos >= MAX_SIG_LEN)
			{
				printf("Driver signature too long in %s\n", d->name);
				exit(EXIT_FAILURE);
			}
			sigbuf[sigpos++] = END;
			d->sigv = xrealloc(d->sigv, ((d->sigc+1) * sizeof(*d->sigv)));
			int* p = xmalloc(sigpos * sizeof(*sigbuf));
			memcpy(p, sigbuf, sigpos * sizeof(*sigbuf));
			d->sigv[d->sigc++] = p;
			sigpos = 0;
		}
		else if (!strcmp(tok, "AND"))
		{
			if (!sigpos)
			{
				printf("Driver signature cannot begin with wildcard in %s\n", d->name);
				exit(EXIT_FAILURE);
			}
			if (sigpos >= MAX_SIG_LEN)
			{
				printf("Driver signature too long in %s\n", d->name);
				exit(EXIT_FAILURE);
			}
			sigbuf[sigpos++] = AND;
		}
		else if (!strcmp(tok, "??"))
		{
			if (!sigpos)
			{
				printf("Driver signature cannot begin with wildcard in %s\n", d->name);
				exit(EXIT_FAILURE);
			}
			if (sigpos >= MAX_SIG_LEN)
			{
				printf("Driver signature too long in %s\n", d->name);
				exit(EXIT_FAILURE);
			}
			sigbuf[sigpos++] = WILD;
		}
		else if (strlen(tok) == 3 && tok[0] == '?' && isdigit(tok[1]) && isdigit(tok[2]))
		{
			if (!sigpos)
			{
				printf("Driver signature cannot begin with wildcard in %s\n", d->name);
				exit(EXIT_FAILURE);
			}
			if (sigpos >= MAX_SIG_LEN)
			{
				printf("Driver signature too long in %s\n", d->name);
				exit(EXIT_FAILURE);
			}
			sigbuf[sigpos++] = RANGEWILD | (tok[1]-'0')<<4 | (tok[2]-'0');
		}
		else if (strlen(tok) == 2 && tok[0] == '?' && isdigit(tok[1]))
		{
			if (!sigpos)
			{
				printf("Driver signature cannot begin with wildcard in %s\n", d->name);
				exit(EXIT_FAILURE);
			}
			if (sigpos >= MAX_SIG_LEN)
			{
				printf("Driver signature too long in %s\n", d->name);
				exit(EXIT_FAILURE);
			}
			sigbuf[sigpos++] = RANGEWILD | (tok[1]-'0');
		}
		else if (strlen(tok) == 2 && isxdigit(tok[0]) && isxdigit(tok[1]))
		{
			sigbuf[sigpos++] = tohex(tok[0])<<4 | tohex(tok[1]);
		}
		/* else driver name */
		else
		{
			if (sigpos)
			{
				printf("New driver name without ending signature in %s\n", d->name);
				exit(EXIT_FAILURE);
			}
			if (d && !(d->sigc))
				printf("WARNING: Driver \"%s\" has no signatures\n", d->name);
			
			for (int i = 0; i < driverc; i++)
			{
				if (!strcasecmp(tok, driverv[i]->name))
				{
					printf("Duplicate driver name \"%s\"\n", tok);
					exit(EXIT_FAILURE);
				}
			}
			
			if (sdriverc) {
				skip_this_driver = 1;
				for (int i = 0; i < sdriverc; i++) {
					if (!strcasecmp(sdriverv[i], tok)) {
						skip_this_driver = 0;
						break;
					}
				}
			} else {
				skip_this_driver = 0;
			}
			
			d = xmalloc(sizeof(*d));
			d->name = tok;
			d->sigc = 0;
			d->sigv = NULL;
			d->found = 0;
			
			if (!skip_this_driver) {
				driverv = xrealloc(driverv, (driverc+1)*sizeof(*driverv));
				driverv[driverc++] = d;
			}
		}
		tok = strtok(NULL, delim);
	}
	
	if (!driverc)
	{
		if (sdriverc)
			puts("No drivers defined in configuration file, or no driver specified in -s argument exists");
		else
			puts("No drivers defined in configuration file");
		exit(EXIT_FAILURE);
	}
	if (sigpos)
	{
		puts("Unexpected end of configuration file");
		exit(EXIT_FAILURE);
	}
	if (!(d->sigc))
		printf("WARNING: Driver \"%s\" has no signatures\n", d->name);
	
	if (!found_filetypes || filetypec==0)
		allfiles = 1;
	
	
	if (allfiles)
	{
		puts("Scanning all filetypes");
	}
	else
	{
		printf("Scanning filetypes ");
		for (int i = 0; i < filetypec; i++)
		{
			printf("%s", filetypev[i]);
			if (i < filetypec-1)
				fputs(", ", stdout);
		}
		putc('\n', stdout);
	}
	
	if (rdriverc)
	{
		printf("Reporting drivers ");
		for (int i = 0; i < rdriverc; i++)
		{
			printf("%s", rdriverv[i]);
			if (i < rdriverc-1)
				fputs(", ", stdout);
		}
		putc('\n', stdout);
	}
	
	if (sdriverc)
	{
		printf("Scanning drivers ");
		for (int i = 0; i < sdriverc; i++)
		{
			printf("%s", sdriverv[i]);
			if (i < sdriverc-1)
				fputs(", ", stdout);
		}
		putc('\n', stdout);
	}
	
	putc('\n', stdout);
	
	char* initialcwd = getcwd(NULL, 0);
	for (int i = 0; i < scanc; i++)
	{
		chdir(initialcwd);
		/* turn all \ into / */
		for (char *p = scanv[i]; *p != '\0'; p++)
		{
			if (*p == '\\')
				*p = '/';
		}
		/* remove trailing directory separators */
		for (char *p = strchr(scanv[i], '\0')-1; *p == '/'; p--)
		{
			*p = '\0';
		}
		struct stat st;
		int err = stat(scanv[i], &st);
		if (err)
		{
			printf("%s \"%s\"\n", strerror(errno), scanv[i]);
			continue;
		}
		if (S_ISDIR(st.st_mode))
		{
			chdir(scanv[i]);
			scan_dir(NULL);
		}
		else
		{
			char* nam = strrchr(scanv[i], '/');
			if (nam)
			{
				*nam = '\0';
				chdir(scanv[i]);
				scan_file(nam+1);
			}
			else
			{
				scan_file(scanv[i]);
			}
		}
	}
	
	if (ident)
	{
		puts("\nFound drivers:");
		for (int i = 0; i < driverc; i++)
		{
			if (rdriverc)
			{
				int j;
				for (j = 0; j < rdriverc; j++)
					if (!strcasecmp(rdriverv[j], driverv[i]->name)) break;
				if (j == rdriverc) continue;
			}
			if (driverv[i]->found)
				printf("%-28.28s %i\n", driverv[i]->name, driverv[i]->found);
		}
	}
	
	printf(
		"\n"
		"Files examined:  %i\n"
		"Identified:      %i\n"
		"Unidentified:    %i\n",
		scanned, ident, unident);
	
	return EXIT_SUCCESS;
}