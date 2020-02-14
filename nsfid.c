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


void scanfile(const char* name)
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
    int sfoundc = 0; /* found drivers which may be listed */
    
    int foundsig = 0;
    
    unsigned char* end = buf+size;
    
    for (int d = 0; d < driverc; d++)
    {
        for (int s = 0; s < driverv[d]->sigc; s++)
        {
            unsigned char* b = buf;
            int* sig = driverv[d]->sigv[s];
            int* str = sig;
            int slen = 0;
            int mindist = 0;
            int maxdist = INT_MAX;
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
                    maxdist = INT_MAX;
                }
                else if (israngewild(c))
                {
                    newstr = 1;
                    while (israngewild(*sig))
                    {
                        mindist += (*sig>>4) & 0xf;
                        maxdist += *sig & 0xf;
                        sig++;
                    }
                    mindist += (c>>4) & 0xf;
                    maxdist += c & 0xf;
                }
                else
                {
                    slen++;
                }
                
                if (newstr)
                {
                    unsigned char* p = hunt(b, end-b, str, slen);
                    if (!p) break;
                    if (newstr > 1) 
                    {
                        foundsig = s;
                        goto found_driver;
                    }
                    int dist = p - b;
                    if (dist < mindist || dist > maxdist) break;
                    
                    b = p+slen;
                    str = sig;
                    slen = 0;
                    mindist = 0;
                    maxdist = INT_MAX;
                    newstr = 0;
                }
            }
        }
        /* not found, check next driver */
        continue;
found_driver:
        if (listident)
        {
            int sfound = 0;
            if (sdriverc > 0)
            {
                for (int i = 0; i < sdriverc; i++)
                {
                    if (!strcasecmp(sdriverv[i], driverv[d]->name))
                    {
                        sfound++;
                        break;
                    }
                }
            }
            else
            {
                sfound++;
            }
            if (sfound)
            {
                if (sfoundc)
                    printf("%-58.57s ", "");
                else
                    printf("%-58.57s ", name);
                
                printf(driverv[d]->name);
                if (verbose && driverv[d]->sigc > 1)
                    printf("   (id %i)", foundsig);
                putc('\n', stdout);
                sfoundc++;
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

void scandir(const char* name)
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
            fullname = fnam;
        }
        
        struct stat st;
        stat(fullname, &st);
        if (S_ISDIR(st.st_mode))
        {
            if (recurse)
                scandir(fullname);
        }
        else
        {
            scanfile(fullname);
        }
        
        if (name)
            free(fullname);
    }
    
    closedir(dir);
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
        " -s <driver>[,<driver>...]  only report these drivers\n"
        " -u                         also report unidentified files\n"
        " -v                         enable verbose mode\n"
        " -?, --help                 display this help message"
        );
    exit(EXIT_SUCCESS);
}

int main(int argc, char* argv[])
{
    for (int i = 1; i < argc; i++)
        if (!strcmp(argv[i], "--help")) dohelp();
    opterr = 0;
    char c;
    while ((c = getopt(argc, argv, "-:ac:df:os:uv")) != -1)
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
            case 's':
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
                        while (isspace(*cur)) cur++;
                        char* p = next;
                        while (--p >= cur && isspace(*p));
                        *(p+1) = '\0';
                        if (p >= cur)
                        {
                            sdriverv = xrealloc(sdriverv, (sdriverc+1)*sizeof(*sdriverv));
                            sdriverv[sdriverc++] = cur;
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
            
            d = xmalloc(sizeof(*d));
            d->name = tok;
            d->sigc = 0;
            d->sigv = NULL;
            d->found = 0;
            
            driverv = xrealloc(driverv, (driverc+1)*sizeof(*driverv));
            driverv[driverc++] = d;
        }
        tok = strtok(NULL, delim);
    }
    
    if (!driverc)
    {
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
    
    if (sdriverc)
    {
        printf("Scanning for drivers ");
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
            scandir(NULL);
        }
        else
        {
            char* nam = strrchr(scanv[i], '/');
            if (nam)
            {
                *nam = '\0';
                chdir(scanv[i]);
                scanfile(nam+1);
            }
            else
            {
                scanfile(scanv[i]);
            }
        }
    }
    
    if (ident)
    {
        puts("\nFound drivers:");
        for (int i = 0; i < driverc; i++)
        {
            if (sdriverc)
            {
                int j;
                for (j = 0; j < sdriverc; j++)
                    if (!strcasecmp(sdriverv[j], driverv[i]->name)) break;
                if (j == sdriverc) continue;
            }
            if (driverv[i]->found)
                printf("%-28.28s %i\n", driverv[i]->name, driverv[i]->found);
        }
    }
    
    printf(
        "\nFiles examined:  %i\n"
        "Identified:      %i\n"
        "Unidentified:    %i\n",
        scanned, ident, unident);
    
    return EXIT_SUCCESS;
}