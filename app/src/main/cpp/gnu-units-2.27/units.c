#define VERSION "2.27"
/*
 *  units, a program for units conversion
 *  Copyright (C) 1996, 1997, 1999, 2000-2007, 2009, 2011-2020,
 *  2022, 2024, 2026 Free Software Foundation, Inc
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 3 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 51 Franklin Street, Fifth Floor,
 *  Boston, MA 02110-1301 USA
 *
 *  This program was written by Adrian Mariano (adrianm@gnu.org)
 */

#define LICENSE "\
Copyright (C) 2026 Free Software Foundation, Inc.\n\
GNU Units comes with ABSOLUTELY NO WARRANTY.\n\
You may redistribute copies of GNU Units\n\
under the terms of the GNU General Public License."

#define _XOPEN_SOURCE 600

#if defined (_WIN32) && defined (_MSC_VER)
# include <windows.h>
# include <winbase.h>
# include <wincon.h>
# include <winnls.h>
# define SUPPORT_UTF8
# define WINDOWS_TERMINAL "WT_SESSION"  /* indicates if running in Windows Terminal */
#endif

#if defined (_WIN32) && defined (HAVE_MKS_TOOLKIT)
# include <sys/types.h>
#endif
# include <sys/stat.h>

#include <ctype.h>
#include <float.h>
#include <limits.h>

#include <stdio.h>
#include <signal.h>
#include <stdarg.h>
#include <time.h>

#if defined (_WIN32) && defined (_MSC_VER)
# include <io.h>
# define fileno _fileno
# define isatty _isatty
# define stat   _stat
#endif

#ifdef HAVE_IOCTL
#  include <sys/ioctl.h>
#  include <fcntl.h>
#endif

#ifndef NO_SETLOCALE
#  include <locale.h>
#endif

#ifdef SUPPORT_UTF8
/* Apparently this define is needed to get wcswidth() prototype */
#  include <wchar.h>
#  ifndef _WIN32
#    include <langinfo.h>
#  endif
#  define UTF8VERSTR "with UTF-8"
#else
#  define UTF8VERSTR "without UTF-8"
#endif

#ifdef READLINE
#  define RVERSTR "With readline"
#  include <readline/readline.h>
#  include <readline/history.h>
#  define HISTORY_FILE ".units_history"
#else
#  define RVERSTR "Without readline"
#endif

#include "getopt.h"
#include "units.h"

#ifndef UNITSFILE
#  define UNITSFILE "definitions.units"
#endif

#ifndef LOCALEMAP
#  define LOCALEMAP "locale_map.txt"
#endif

#ifndef DATADIR
#  ifdef _WIN32
#    define DATADIR   "..\\share\\units"
#  else
#    define DATADIR   "../share/units"
#  endif
#endif

#if defined (_WIN32) && defined (_MSC_VER)
#  include <direct.h>
#  define getcwd _getcwd
#else
#  include <unistd.h>
#endif

#ifdef _WIN32
#  define EXE_EXT ".exe"
#  define PATHSEP ';'
#  define DIRSEP '\\'
#  define DIRSEPSTR "\\"        /* for building pathnames */
#else
#  define EXE_EXT ""
#  define PATHSEP ':'
#  define DIRSEP '/'
#  define DIRSEPSTR "/"         /* for building pathnames */
#endif

#define PRIMITIVECHAR '!'       /* Character that marks irreducible units */
#define COMMENTCHAR '#'         /* Comments marked by this character */
#define COMMANDCHAR '!'         /* Unit database commands marked with this */
#define UNITSEPCHAR ';'         /* Separator for unit lists.  Include this */
                                /*    char in rl_basic_word_break_characters */
                                /*    and in nonunitchars defined in parse.y */
#define FUNCSEPCHAR ';'         /* Separates forward and inverse definitions */
#define REDEFCHAR '+'           /* Mark unit as redefinition to suppress warning message */
#ifdef _WIN32
#  define DEFAULTPAGER "more"             /* Default pager for Windows */
#else
#  define DEFAULTPAGER "/usr/bin/pager"   /* Default pager for Unix */
#endif
#define DEFAULTLOCALE "en_US"   /* Default locale */
#define MAXINCLUDE 5            /* Max depth of include files */
#define MAXFILES 25             /* Max number of units files on command line */
#define NODIM "!dimensionless"  /* Marks dimensionless primitive units, such */
                                /*    as the radian, which are ignored when */
                                /*    doing unit comparisons */
#define NOPOINT -1              /* suppress display of pointer in processunit*/
#define NOERRMSG -2             /* no error messages in checkunitlist() */
#define ERRMSG -3
#define SHOWFILES -4

#define MAXHISTORYFILE 5000     /* max length of history file for readline */

#define MAXPRODUCTREDUCTIONS 1000  /* If we make this many reductions, declare */
                                   /*    a circular reference */

/* values for output number format */
#define BASE_FORMATS "gGeEf"    /* printf() format types recognized pre-C99 */
#define DEFAULTPRECISION 8      /* default significant digits for printf() */
#define DEFAULTTYPE 'g'         /* default number format type for printf() */
#define MAXPRECISION DBL_DIG    /* maximum number precision for printf() */

#define HOME_UNITS_ENV "MYUNITSFILE"  /* Personal units file environment var */

#define NOERROR_KEYWORD "noerror "   /* The trailing space is important */
#define CO_NOARG -1

#define HELPCOMMAND "help"      /* Command to request help at prompt */
#define INFOCOMMAND "info"      /* Command to show info equivalent to --info option */
#define SEARCHCOMMAND "search"  /* Command to request text search of units */
#define SETCOMMAND "set"        /* Command to display or set command-line options */
#define VERCOMMAND "version"    /* Command to show units version */
#define UNITMATCH "?"           /* Command to request conformable units */
#define RUNTIMEVAR "_"          /* Command arg to show runtime variables */
#define ALLUNITS  "."           /* Command arg to show all units */

/* values for iscommand(), check_yn_choices(), setflag_yn(), and show_yn_setting() */
#define SETFLAG '*'             /* Flag to indicate option set to other than default */
#define YES_STR    "yes"
#define NO_STR     "no"
#define YN_STR     "y|n"                    /* short indication of values for Boolean settings shown with 'set' */
#define YN_CHOICES "\"y[es]\" or \"n[o]\""  /* long indication of values for Boolean settings shown in error message */
#define SETTING_STATUS_LEN 22               /* width of field showing current value of setting */

char *exit_commands[]={"quit","exit",0};
char *all_commands[]={"quit","exit",HELPCOMMAND,INFOCOMMAND,SEARCHCOMMAND,SETCOMMAND,UNITMATCH,VERCOMMAND,0};

/* Key words for function definitions */
char *fnkeywords[]={"units=", "domain=", "range=", NOERROR_KEYWORD, 0};
#define FN_UNITS 0
#define FN_DOMAIN 1
#define FN_RANGE 2
#define FN_NOERROR 3

char *builtins[] = {"sin", "cos", "tan", "csc", "secant", "cot", "ln", "log", "exp",
                    "acos", "atan", "asin", "acsc", "asecant", "acot", "sqrt", "cuberoot", "per",
                    "sinh", "cosh", "tanh", "asinh", "atanh", "acosh",
                    "csch", "sech", "coth", "acsch", "acoth", "asech", 0};

struct {
  char *flags;          /* printf() format flags */
  char *format;         /* printf() format specification for numeric output */
  int width;            /* printf() width from format */
  int precision;        /* printf() precision from format */
  char type;            /* printf() type from format */
} num_format;


struct {    /* Program command line option flags */
 int
   listallunits,  /* send a list of all units to stdout */
   interactive,
   unitlists,     /* Perform unit list output if set */
   oneline,       /* Suppresses the second line of output */
   quiet,         /* Supress prompting (-q option) */
   round,         /* Round the last of unit list output to nearest integer */
   showconformable, /*   */
   showfactor,    /*   */
   strictconvert, /* Strict conversion (disables reciprocals) */
   unitcheck,     /* Enable unit checking: 1 for regular check, 2 for verbose*/
   utf8,          /* Enable UTF-8 support on Windows */
   verbose,       /* Flag for output verbosity */
   readline;      /* Using readline library? */
} flags;

struct query {
   char
       *have,
       *want;
   int
       havewidth,
       wantwidth;
};


#define UTF8MARKER "\xEF\xBB\xBF"   /* UTF-8 marker inserted by some Windows */
                                    /* programs at start of a UTF-8 file */

struct parseflag parserflags;   /* parser options */


char *homeunitsfile = ".units"; /* Units filename in home directory */
char *homedir = NULL;           /* User's home direcotry */
char *homedir_error = NULL;     /* Error message for home directory search */
char *pager;                    /* Actual pager (from PAGER environment var) */
char *mylocale;                 /* Locale in effect (from LC_CTYPE or LANG) */
int utf8mode;                   /* Activate UTF8 support */
char *powerstring = "^";        /* Exponent character used in output */
char *unitsfiles[MAXFILES+1];   /* Null terminated list of units file names */
char *logfilename=NULL;         /* Filename for logging */
FILE *logfile=NULL;             /* File for logging */
char *promptprefix=NULL;        /* Prefix added to prompt */
char *progname;                 /* Used in error messages */
char *fullprogname;             /* Full path of program; printversion() uses */
char *progdir;                  /* Used to find supporting files */
char *datadir;                  /* Used to find supporting files */
char *deftext="        Definition: ";/* Output text when printing definition */
char *digits = "0123456789.,";


#define QUERYHAVE  "You have: "  /* Prompt text for units to convert from */
#define QUERYWANT  "You want: "  /* Prompt text for units to convert to */

#define LOGFROM    "From: "     /* tag for log file */
#define LOGTO      "To:   "     /* tag for log file */


#define  HASHSIZE 101           /* Values from K&R 2nd ed., Sect. 6.6 */
#define  HASHNUMBER 31

#define  SIMPLEHASHSIZE 128
#define  simplehash(str) (*(str) & 127)    /* "hash" value for prefixes */

#define POINTER  "^"            /* pointer to location of a parse error */

#define ERRNUMFMT "%.8g"        /* Numerical format for certain error messages */

char *errormsg[]={
          /*  0 */ "Successful completion",
          /*  1 */ "Parse error",
          /*  2 */ "Product overflow",
          /*  3 */ "Unit reduction error (bad unit definition)",
          /*  4 */ "Circular unit definition",
          /*  5 */ "Invalid sum or difference of non-conformable units",
          /*  6 */ "Unit not dimensionless",
          /*  7 */ "Unit not a root",
          /*  8 */ "Unknown unit",
          /*  9 */ "Bad argument",
          /* 10 */ "Weird nonlinear unit type (bug in program)",
          /* 11 */ "Function argument has wrong dimension",
          /* 12 */ "Argument of function outside domain",
          /* 13 */ "Nonlinear unit definition has unit error",
          /* 14 */ "No inverse defined",
          /* 15 */ "Parser memory overflow (recursive function definition?)",
          /* 16 */ "Argument wrong dimension or bad nonlinear unit definition",
          /* 17 */ "Cannot open units file",
          /* 18 */ "Units file contains errors",
          /* 19 */ "Memory allocation error",
          /* 20 */ "Malformed number",
          /* 21 */ "Unit name ends with a digit other than 0 or 1 without preceding '_'",
          /* 22 */ "No previous result; '_' not set",
          /* 23 */ "Base unit not dimensionless; rational exponent required",
          /* 24 */ "Base unit not a root",
          /* 25 */ "Exponent not dimensionless",
          /* 26 */ "Unknown function name",
          /* 27 */ "Overflow: number too large",
          /* 28 */ "Underflow: number too small",
          /* 29 */ "Too many arguments to function",
          /* 30 */ "Not enough arguments to function",
          /* 31 */ "Multiple arguments without valid function",
          /* 32 */ "Unknown comparison",
          /* 33 */ "Invalid comparison of non-conformable units"
};

char *invalid_utf8 = "invalid/nonprinting UTF-8";

char *irreducible = NULL;            /* Name of last irreducible unit */


/* Hash table for unit definitions. */

struct unitlist {
   char *name;                  /* unit name */
   char *value;                 /* unit value */
   int linenumber;              /* line in units data file where defined */
   char *file;                  /* file where defined */
   struct unitlist *next;       /* next item in list */
} *utab[HASHSIZE];


/* Table for prefix definitions. */

struct prefixlist {
   int len;                     /* length of name string */
   char *name;                  /* prefix name */
   char *value;                 /* prefix value */
   int linenumber;              /* line in units data file where defined */
   char *file;                  /* file where defined */
   struct prefixlist *next;     /* next item in list */
} *ptab[SIMPLEHASHSIZE];


struct wantalias {
  char *name;
  char *definition;
  struct wantalias *next;
  int linenumber;
  char *file;
};

struct wantalias *firstalias = NULL;
struct wantalias **aliaslistend = &firstalias; /* Next list entry goes here */

/* Table for function definitions */

struct func *ftab[SIMPLEHASHSIZE];

/*
   Used for passing parameters to the parser when we are in the process
   of parsing a unit function.  If function_parameter_count is nonzero, then
   whenever the strings in function_parameter appear in a unit expression
   they are replaced by the corresponding unit value stored in parameter_value.
*/

int function_parameter_count = 0;
char **function_parameter = 0;
struct unittype **parameter_value = 0;

/* Stores the last result value for replacement with '_' */

int lastunitset = 0;
struct unittype lastunit;

char *NULLUNIT = "";  /* Used for units that are canceled during reduction */

#define startswith(string, prefix) (!strncmp(string, prefix, strlen(prefix)))
#define lastchar(string) (*((string)+strlen(string)-1))
#define emptystr(string) (*(string) == '\0')
#define nonempty(list)  ((list) && *(list))
#define equal(a, b)  (strcmp(a, b) == 0)
#define equaln(a, b, n)  (strncmp(a, b, n) == 0)
#define equalnv(a, b, min, max)  (strncmp(a, b, max < min ? min : max) == 0)


#ifdef READLINE

char *historyfile;           /* Filename for readline history */
int init_history_length;     /* Length of history read from the history file*/
int init_history_base;

void
save_history(void)
{
  int newentries;
  int err;

  newentries = history_length-init_history_length;
  if (history_max_entries > 0){
    newentries += history_base - init_history_base;
    if (newentries > history_max_entries)
      newentries = history_max_entries;
  }

  err = append_history(newentries,historyfile);
  if (err){
    if (err == ENOENT)
      err = write_history(historyfile);
    if (err) {
      printf("Unable to write history to '%s': %s\n",historyfile,strerror(err));
      return;
    }
  }
  history_truncate_file(historyfile,MAXHISTORYFILE);
}
#endif


/* Increases the buffer by BUFGROW bytes and leaves the new pointer in buf
   and the new buffer size in bufsize. */

#define BUFGROW 100

void
growbuffer(char **buf, int *bufsize)
{
  int usemalloc;

  usemalloc = !*buf || !*bufsize;
  *bufsize += BUFGROW;
  if (usemalloc)
    *buf = malloc(*bufsize);
  else
    *buf = realloc(*buf,*bufsize);
  if (!*buf){
    fprintf(stderr, "%s: memory allocation error (growbuffer)\n",progname);
    exit(EXIT_FAILURE);
  }
}


FILE *
openfile(char *file,char *mode)
{
  FILE *fileptr;

  struct stat statbuf;
  if (stat(file, &statbuf)==0 && statbuf.st_mode & S_IFDIR){
    errno=EISDIR;
    return NULL;
  }
  fileptr = fopen(file,mode);
  return fileptr;
}



void
logprintf(const char *format, ...)
{
  va_list args;

  va_start(args, format);
  vprintf(format, args);
  va_end(args);
  if (logfile) {
    va_start(args, format);
    vfprintf(logfile, format, args);
    va_end(args);
  }
}

void
logputchar(char c)
{
  putchar(c);
  if (logfile) fputc(c, logfile);
}

void
logputs(const char *s)
{
  fputs(s, stdout);
  if (logfile) fputs(s, logfile);
}


/* Look for a subscript in the input string.  A subscript starts with
   '_' and is followed by a sequence of only digits (matching the
   regexp "_[0-9]+").  The function returns 1 if it finds a subscript
   and zero otherwise.  Note that it returns 1 for an input that is
   entirely subscript, with the '_' appearing in the first position. */

int
hassubscript(const char *str)
{
  const char *ptr = &lastchar(str);
  while (ptr>str){
    if (!strchr(digits, *ptr))
      return 0;
    ptr--;
    if (*ptr=='_')
      return 1;
  }
  return 0;
}


/* replace various Unicode operator symbols with stanard ASCII equivalents */
void
replace_operators(char *input)
{
  struct{
    char *unicode;
    char replacement;
  } repl_table[]={
                   {"\xE2\x80\x92", '-'}, /* U+2012: figure dash */
                   {"\xE2\x80\x93", '-'}, /* U+2013: en dash */
                   {"\xE2\x88\x92", '-'}, /* U+2212: minus */
                   {"\xC3\x97", '*'},     /* U+00D7: times */
                   {"\xE2\xA8\x89" ,'*'}, /* U+2A09: N-ary times operator */
                   {"\xC2\xB7", '*'},     /* U+00B7: middle dot */
                   {"\xE2\x8B\x85", '*'}, /* U+22C5: dot operator */
                   {"\xC3\xB7", '/'},     /* U+00F7: division sign */
                   {"\xE2\x88\x95", '/'}, /* U+2215: division slash */
                   {"\xE2\x81\x84", '|'}, /* U+2044: fraction slash */
                   /* convert specialized spaces to ordinary spaces */
                   {"\xC2\xA0", ' '},     /* U+00A0, no-break space*/
                   {"\xE1\x9A\x80", ' '}, /* U+1680, ogham space mark */
                   {"\xE2\x80\x80", ' '}, /* U+2000, en quad */
                   {"\xE2\x80\x81", ' '}, /* U+2001, em quad */
                   {"\xE2\x80\x82", ' '}, /* U+2002, en space */
                   {"\xE2\x80\x83", ' '}, /* U+2003, em space */
                   {"\xE2\x80\x84", ' '}, /* U+2004, three-per-em space*/
                   {"\xE2\x80\x85", ' '}, /* U+2005, four-per-em space */
                   {"\xE2\x80\x86", ' '}, /* U+2006, six-per-em space */
                   {"\xE2\x80\x87", ' '}, /* U+2007, figure space */
                   {"\xE2\x80\x88", ' '}, /* U+2008, punctuation space */
                   {"\xE2\x80\x89", ' '}, /* U+2009, thin space*/
                   {"\xE2\x80\x8A", ' '}, /* U+200A, hair space */
                   {"\xE2\x80\xAF", ' '}, /* U+202F, narrow non-break space */
                   {"\xE2\x81\x9F", ' '}, /* U+205F, medium mathematical space */
                   {"\xE3\x80\x80", ' '}, /* U+3000, ideographic space */
                   /* treat zero-width characters as nothing? */
                   {"\xE2\x80\x8B", '\0'},  /* U+200B, zero-width space */
                   {"\xE2\x80\x8C", '\0'},  /* U+200D, zero-width non-joiner */
                   {0,0}
  };
  char *inptr, *outptr, *ptr;

  for (int i=0; repl_table[i].unicode; i++) {
    inptr = outptr = input;
    do {
      ptr = strstr(inptr, repl_table[i].unicode);    /* find next unicode symbol */
      if (ptr) {
        while (inptr < ptr)                          /* copy the input up to the unicode */
          *outptr++ = *inptr++;
        inptr = ptr + strlen(repl_table[i].unicode); /* skip over unicode */
        if (repl_table[i].replacement)
            *outptr++ = repl_table[i].replacement;   /* Output replacement */
      }
    } while (ptr);

    /* If replacement were made, copy remaining input to end of string */

    if (inptr > input) {
      while (*inptr)
        *outptr++ = *inptr++;
      *outptr = '\0';
    }
  }
}



/* Replace all control chars with a space */

void
replacectrlchars(char *string)
{
  for(;*string;string++)
    if (iscntrl(*string))
      *string = ' ';
}

/*
   Fetch a line of data with backslash for continuation.  The
   parameter count is incremented to report the number of newlines
   that are read so that line numbers can be accurately reported.
*/

char *
fgetscont(char *buf, int size, FILE *file, int *count)
{
  if (!fgets(buf,size,file))
    return NULL;
  (*count)++;
  while(strlen(buf)>=2 && 0==strcmp(buf+strlen(buf)-2,"\\\n")){
    (*count)++;
    buf[strlen(buf)-2] = '\0'; /* delete trailing \n and \ char */
    if (strlen(buf)>=size-1) /* return if the buffer is full */
      return buf;
    if (!fgets(buf+strlen(buf), size - strlen(buf), file))
      return buf;  /* already read some data so return success */
  }
  if (lastchar(buf) == '\\') {   /* If last char of buffer is \ then   */
    ungetc('\\', file);           /* we don't know if it is followed by */
    lastchar(buf) = '\0';         /* a \n, so put it back and try again */
  }
  return buf;
}


/*
   Gets arbitrarily long input data into a buffer using growbuffer().
   Returns 0 if no data is read.  Increments count by the number of
   newlines read unless it points to NULL.

   Replaces tabs and newlines with spaces before returning the result.
*/

char *
fgetslong(char **buf, int *bufsize, FILE *file, int *count)
{
  int dummy;
  if (!count)
    count = &dummy;
  if (!*bufsize) growbuffer(buf,bufsize);
  if (!fgetscont(*buf, *bufsize, file, count))
    return NULL;
  while (lastchar(*buf) != '\n' && !feof(file)){
    growbuffer(buf, bufsize);
    fgetscont(*buf+strlen(*buf), *bufsize-strlen(*buf), file, count);
    (*count)--;
  }
  /* These nonprinting characters must be removed so that the test
     for UTF-8 validity will work. */
  replacectrlchars(*buf);
  return *buf;
}

/* Allocates memory and aborts if malloc fails. */

void *
mymalloc(size_t bytes,const char *mesg)
{
   void *pointer;

   pointer = malloc(bytes);
   if (!pointer){
     fprintf(stderr, "%s: memory allocation error %s\n", progname, mesg);
     exit(EXIT_FAILURE);
   }
   return pointer;
}

/* Reallocates memory and aborts if realloc fails. */

void *
myrealloc(void *pointer,size_t bytes,const char *mesg)
{
   pointer = realloc(pointer, bytes);
   if (!pointer){
     fprintf(stderr, "%s: memory allocation error %s\n", progname, mesg);
     exit(EXIT_FAILURE);
   }
   return pointer;
}

/* Duplicates a string */

char *
dupstr(const char *str, const char *msg)
{
   char *ret;

  if (msg)
      ret = mymalloc(strlen(str) + 1,msg);
  else
      ret = mymalloc(strlen(str) + 1,"(dupstr)");
  strcpy(ret, str);
  return ret;
}

/* Duplicates a string that is not null-terminated,
   adding the null to the copy */

char *
dupnstr(const char *string, size_t length, const char *msg)
{
  char *newstr;
  if (msg)
      newstr = mymalloc(length+1,msg);
  else
      newstr = mymalloc(length+1,"(dupnstr)");
  strncpy(newstr, string, length);
  newstr[length] = '\0';
  return newstr;
}


#ifdef SUPPORT_UTF8

/*
   The strwidth function gives the printed width of a UTF-8 byte sequence.
   It will return -1 if the sequence is an invalid UTF-8 sequence or
   if the sequence contains "nonprinting" characters.  Note that \n and \t are
   "nonprinting" characters.
*/

int
strwidth(const char *str)
{
  wchar_t *widestr;
  int len;

  if (!utf8mode)
    return strlen(str);
  len = strlen(str)+1;
  widestr = mymalloc(sizeof(wchar_t)*len, "(strwidth)");
  len = mbsrtowcs(widestr, &str, len, NULL);
  if (len==-1){
    free(widestr);
    return -1; /* invalid multibyte sequence */
  }
#  if defined _WIN32 && defined _MSC_VER
  len=wcslen(widestr);
#  else
  len=wcswidth(widestr, len);
#  endif
  free(widestr);
  return len;
}
#else
#  define strwidth strlen
#endif



/* hashing algorithm for units */

unsigned
uhash(const char *str)
{
   unsigned hashval;

   for (hashval = 0; *str; str++)
      hashval = *str + HASHNUMBER * hashval;
   return (hashval % HASHSIZE);
}


/* Lookup a unit in the units table.  Returns the definition, or NULL
   if the unit isn't found in the table. */

struct unitlist *
ulookup(const char *str)
{
   struct unitlist *uptr;

   for (uptr = utab[uhash(str)]; uptr; uptr = uptr->next)
      if (strcmp(str, uptr->name) == 0)
         return uptr;
   return NULL;
}

/* Lookup a prefix in the prefix table.  Finds the longest prefix that
   matches the beginning of the input string.  Returns NULL if no
   prefixes match. */

struct prefixlist *
plookup(const char *str)
{
   struct prefixlist *prefix;
   struct prefixlist *bestprefix=NULL;
   int bestlength=0;

   for (prefix = ptab[simplehash(str)]; prefix; prefix = prefix->next) {
     if (prefix->len > bestlength && !strncmp(str, prefix->name, prefix->len)){
       bestlength = prefix->len;
       bestprefix = prefix;
     }
   }
   return bestprefix;
}

/* Look up function in the function linked list */

struct func *
fnlookup(const char *str)
{
  struct func *funcptr;

  for(funcptr=ftab[simplehash(str)];funcptr;funcptr = funcptr->next)
    if (!strcmp(funcptr->name, str))
      return funcptr;
  return NULL;
}

struct wantalias *
aliaslookup(const char *str)
{
  struct wantalias *aliasptr;
  for(aliasptr = firstalias; aliasptr; aliasptr=aliasptr->next)
    if (!strcmp(aliasptr->name, str))
      return aliasptr;
  return NULL;
}


/* Insert a new function into the linked list of functions */

void
addfunction(struct func *newfunc)
{
  int val;

  val = simplehash(newfunc->name);
  newfunc->next = ftab[val];
  ftab[val] = newfunc;
}


void
free_functype(struct functype *func)
{
  int count, i;

  count = func->param_count;

  if (func->param){
    for(i=0;i<count;i++)
      if (func->param[i]) free(func->param[i]);
  }
  if (func->dimen){
    for(i=0;i<count;i++)
      if (func->dimen[i]) free(func->dimen[i]);
  }
  if (func->def) free(func->def);
  if (func->domain) free(func->domain);
}


/* Free the fields in the function except for the name so that it
   can be redefined.  It remains in position in the linked list. */
void
freefunction(struct func *funcentry)
{
  if (funcentry->table){
    free(funcentry->table);
    free(funcentry->tableunit);
  } else {
    free_functype(&funcentry->forward);
    free_functype(&funcentry->inverse);
  }
}

/* Remove leading and trailing spaces from the input */

void
removespaces(char *in)
{
  char *ptr;
  if (*in) {
    for(ptr = &lastchar(in); *ptr==' '; ptr--); /* Last non-space */
    *(ptr+1) = '\0';
    if (*in==' '){
      ptr = in + strspn(in," ");
      memmove(in, ptr, strlen(ptr)+1);
    }
  }
}


/*
   Looks up an inverse function given as a ~ character followed by
   spaces and then the function name.  The spaces will be deleted as a
   side effect.  If an inverse function is found returns the function
   pointer, otherwise returns null.
*/

struct func *
invfnlookup(char *str)
{
  if (*str != '~')
    return NULL;
  removespaces(str+1);
  return fnlookup(str+1);
}


char *
strip_comment(char *line)
{
  char *comment = NULL;

  if ((line = strchr(line,COMMENTCHAR))) {
    comment = line+1;
    *line = '\0';
  }
  return comment;
}


/* Print string but replace two consecutive spaces with one space. */

void
tightprint(FILE *outfile, char *string)
{
  while(*string){
    fputc(*string, outfile);
    if (*string != ' ') string++;
    else while(*string==' ') string++;
  }
}

/*
    Copy string to buf, replacing two or more consecutive spaces with
    one space.
*/
void
tightbufprint(char *buf, char *string)
{
  while(*string) {
    *buf++ = *string;
    if (*string != ' ')
        string++;
    else {
        while(*string==' ')
            string++;
    }
  }
  *buf = '\0';
}


#define readerror (goterr=1) && errfile && fprintf

#define VAGUE_ERR "%s: error in units file '%s' line %d\n", \
                       progname, file, linenum

/* Print out error message encountered while reading the units file. */


/*
  Splits the line into two parts.  The first part is space delimited.
  The second part is everything else.  Removes trailing spaces from
  the second part.  Returned items are null if no parameter was found.
*/

void
splitline(char *line, char **first, char **second)
{
  *second = NULL;
  *first = strtok(line, " ");
  if (*first){
    *second = strtok(NULL, "\n");
    if (*second){
      removespaces(*second);
      if (emptystr(*second))
        *second = NULL;
    }
  }
}


/* see if character is part of a valid decimal number */

int
isdecimal(char c)
{
  return strchr(digits, c) != NULL;
}


/*
   Check for some invalid unit names.  Print error message.  Returns 1 if
   unit name is bad, zero otherwise.
*/
int
checkunitname(char *name, int linenum, char *file, FILE *errfile)
{
#if 0
  char nonunitchars[] = ",~;+-*/|^)<>";  /* Also defined in parse.y with a few more characters */
#endif
  char nonunitchars[] = ",~;+-*/|^)";    /* Also defined in parse.y with a few more characters */
  char **ptr;
  char *cptr;

  if ((cptr=strpbrk(name, nonunitchars))){
    if (errfile) fprintf(errfile,
         "%s: unit '%s' in units file '%s' on line %d ignored.  It contains invalid character '%c'\n",
              progname, name, file, linenum, *cptr);
    return 1;
  }
  if (strchr(digits, name[0])){
    if (errfile) fprintf(errfile,
         "%s: unit '%s' in units file '%s' on line %d ignored.  It starts with a digit\n",
              progname, name, file, linenum);
    return 1;
  }
  for(ptr=builtins;*ptr;ptr++)
    if (!strcmp(name, *ptr)){
      if (errfile) fprintf(errfile,
           "%s: redefinition of built-in function '%s' in file '%s' on line %d ignored.\n",
                           progname, name, file, linenum);
      return 1;
    }
  for(ptr=all_commands;*ptr;ptr++)
    if (!strcmp(name, *ptr)){
      if (errfile) fprintf(errfile,
           "%s: unit name '%s' in file '%s' on line %d may be hidden by command with the same name.\n",
                           progname, name, file, linenum);
    }
  return 0;
}

int
newunit(char *unitname, char *unitdef, int *count, int linenum,
        char *file,FILE *errfile, int redefine, int userunit)
{
  struct unitlist *uptr;
  unsigned hashval;

  /* units ending with '_' create ambiguity for exponents */

  if ((unitname[0]=='_' && !userunit) || lastchar(unitname)=='_'){
    if (errfile) fprintf(errfile,
       "%s: unit '%s' on line %d of '%s' ignored.  It starts or ends with '_'\n",
              progname, unitname, linenum, file);
    return E_BADFILE;
  }

  /* Units that end in [2-9] can never be accessed */
  if (strchr(".,23456789", lastchar(unitname)) && !hassubscript(unitname)){
    if (errfile) fprintf(errfile,
       "%s: unit '%s' on line %d of '%s' ignored.  %s\n",
        progname, unitname, linenum, file,errormsg[E_UNITEND]);
    return E_BADFILE;
  }

  if (checkunitname(unitname, linenum, file, errfile))
    return E_BADFILE;

  if ((uptr=ulookup(unitname))) {    /* Is it a redefinition? */
    if (flags.unitcheck && errfile && !redefine)
      fprintf(errfile,
      "%s: unit '%s' defined on line %d of '%s' is redefined on line %d of '%s'.\n",
              progname, unitname, uptr->linenumber,uptr->file,
              linenum, file);
    free(uptr->value);
  } else {
    /* make new units table entry */

    uptr = (struct unitlist *) mymalloc(sizeof(*uptr),"(newunit)");
    uptr->name = dupstr(unitname, "(newunit)");

    /* install unit name/value pair in list */

    hashval = uhash(uptr->name);
    uptr->next = utab[hashval];
    utab[hashval] = uptr;
    (*count)++;
  }
  uptr->value = dupstr(unitdef, "(newunit)");
  uptr->linenumber = linenum;
  uptr->file = file;
  return 0;
}



int
newprefix(char *unitname, char *unitdef, int *count, int linenum,
          char *file,FILE *errfile, int redefine)
{
  struct prefixlist *pfxptr;
  unsigned pval;

  lastchar(unitname) = 0;
  if (checkunitname(unitname,linenum,file,errfile))
    return E_BADFILE;
  if ((pfxptr = plookup(unitname))     /* already there: redefinition */
      && !strcmp(pfxptr->name, unitname)){
    if (flags.unitcheck && errfile && !redefine)
      fprintf(errfile,
             "%s: prefix '%s-' defined on line %d of '%s' is redefined on line %d of '%s'.\n",
              progname, unitname, pfxptr->linenumber,pfxptr->file,
              linenum, file);
    free(pfxptr->value);
  } else {
    pfxptr = (struct prefixlist *) mymalloc(sizeof(*pfxptr),"(newprefix)");
    pfxptr->name = dupstr(unitname, "(newprefix)");
    pfxptr->len = strlen(unitname);
    pval = simplehash(unitname);
    pfxptr->next = ptab[pval];
    ptab[pval] = pfxptr;
    (*count)++;
  }
  pfxptr->value = dupstr(unitdef, "(newprefix)");
  pfxptr->linenumber = linenum;
  pfxptr->file = file;
  return 0;
}


/*
   parseinterval() looks for an interval that is either closed or open at either
   end.  Values are allowed to be missing, in which case they are set to infinity.
   The function returns the first character after the closing bracket if no errors
   occur or the NULL pointer on error.
*/

char *
parseinterval(char *input, struct interval *ival,
              char *unitname, int linenum, char *file,FILE *errfile, int *errors, char *keyword)
{
  char *start, *end, *numend, *middle, *first, *second;
  int firsterr;
  int seconderr;

  start = strpbrk(input, "[(");
  if (!start){
    if (errfile) fprintf(errfile,
             "%s: expecting '[' or '(' after '%s' in definition of '%s' in '%s' line %d\n",
             progname, keyword, unitname, file, linenum);
    (*errors)++;
    return input;
  }
  if (*start=='(') ival->min_open=1;
  else ival->min_open=0;
  *start++ = '\0';
  removespaces(input);
  if (!emptystr(input)){
    if (errfile) fprintf(errfile,
        "%s: unexpected characters (%s) before '%c' in definition of '%s' in '%s' line %d\n",
        progname, input, ival->min_open?'(':'[',unitname, file, linenum);
    (*errors)++;
  }
  end = strpbrk(start, "])");
  if (!end){
    if (errfile) fprintf(errfile,
             "%s: expecting ']' or ')' after '%s' in definition of '%s' in '%s' line %d\n",
             progname, keyword, unitname, file, linenum);
    (*errors)++;
    return start;
  }
  if (*end==')') ival->max_open = 1;
  else ival->max_open=0;
  *end++ = '\0';

  middle = strchr(start,',');
  if (middle){
    *middle++ = '\0';
    removespaces(middle);
    second = middle;
  } else {
    if (errfile) fprintf(errfile,
             "%s: interval missing its comma in definition of '%s' in '%s' line %d\n",
             progname, unitname, file, linenum);
    (*errors)++;
    return end;
  }

  removespaces(start);
  first = start;

  firsterr=seconderr=0;
  if (emptystr(first))
    ival->min = -HUGE_VAL;
  else {
    ival->min = strtod(first, &numend);
    if (*numend) firsterr=1;
  }
  if (emptystr(second))
    ival->max = HUGE_VAL;
  else {
    ival->max = strtod(second, &numend);
    if (*numend) seconderr=1;
  }
  if (firsterr || seconderr){
    if (errfile) {
      if (firsterr)
        fprintf(errfile,
                "%s: malformed number '%s' in interval after '%s' in definition of '%s' in '%s' line %d\n",
                progname, first, keyword, unitname, file, linenum);
      if (seconderr)
        fprintf(errfile,
                "%s: malformed number '%s' in interval after '%s' in definition of '%s' in '%s' line %d\n",
                progname, second, keyword, unitname, file, linenum);
    }
    (*errors)++;
  }
  if (!*errors && ival->max <= ival->min){
    if (errfile) fprintf(errfile,
               "%s: second endpoint for interval must be greater than the first\n"
               "       in definition of '%s' in '%s' line %d\n",
                progname, unitname, file, linenum);
    (*errors)++;
  }
  return end;
}



void
copyfunctype(struct functype *dest, struct functype *src)
{
  int count,i;
  count = src->param_count;
  dest->param_count = src->param_count;
  dest->def = dupstr(src->def, "(copyfunctype)");
  dest->param = (char **) mymalloc(count*sizeof(char *),  "(copyfunctype)");
  dest->dimen = (char **) mymalloc(count*sizeof(char *),  "(copyfunctype)");
  for(i=0;i<count;i++){
    dest->param[i] = dupstr(src->param[i], "(copyfunctype)");
    if (src->dimen[i])
      dest->dimen[i] = dupstr(src->dimen[i], "(copyfunctype)");
    else
      dest->dimen[i] = 0;
  }
  dest->domain = (struct interval *) mymalloc(count*sizeof(struct interval), "(copyfunctype)");
  memcpy(dest->domain, src->domain, count*sizeof(struct interval));
}


int
copyfunction(char *unitname, char *funcname, int *count, int linenum,
             char *file, FILE *errfile)
{
  struct func *source, *funcentry;
  int i;
  if (checkunitname(unitname, linenum, file, errfile))
    return E_BADFILE;
  removespaces(funcname);
  i = strlen(funcname)-2;                /* strip trailing () if present */
  if (i>0 && !strcmp(funcname+i,"()"))
    funcname[i] = '\0';
  source = fnlookup(funcname);
  if (!source) {
    if (errfile){
      if (!strpbrk(funcname," ;][()+*/-^"))
        fprintf(errfile,"%s: bad definition for '%s' in '%s' line %d, function '%s' not defined\n",
                progname, unitname, file, linenum, funcname);
      else
        fprintf(errfile,"%s: bad function definition of '%s' in '%s' line %d\n",
                progname,unitname,file,linenum);
    }
    return E_BADFILE;
  }
  if ((funcentry=fnlookup(unitname))){
    if (flags.unitcheck && errfile)
      fprintf(errfile,
             "%s: function '%s' defined on line %d of '%s' is redefined on line %d of '%s'.\n",
              progname, unitname, funcentry->linenumber,funcentry->file,
              linenum, file);
    freefunction(funcentry);
  } else {
    funcentry = (struct func*)mymalloc(sizeof(struct func),"(newfunction)");
    funcentry->name = dupstr(unitname, "(copyfunction)");
    addfunction(funcentry);
    (*count)++;
  }
  funcentry->linenumber = linenum;
  funcentry->file = file;
  funcentry->skip_error_check = source->skip_error_check;
  if (source->table){
    funcentry->tablelen = source->tablelen;
    funcentry->tableunit = dupstr(source->tableunit, "(copyfunction)");
    funcentry->table = (struct pair *)
        mymalloc(sizeof(struct pair)*funcentry->tablelen, "(copyfunction)");
    for(i=0;i<funcentry->tablelen;i++){
      funcentry->table[i].location = source->table[i].location;
      funcentry->table[i].value = source->table[i].value;
    }
  } else {
    funcentry->table = 0;
      copyfunctype(&funcentry->forward, &source->forward);
      copyfunctype(&funcentry->inverse, &source->inverse);
  }
  return 0;
}


void
init_interval(struct interval *data)
{
  data->min=-HUGE_VAL;
  data->max=HUGE_VAL;
  data->min_open=1;
  data->max_open=1;
}


char *
parse_func_units(int param_count,
                 char *input, char **forward_dim, char **inverse_dim,
                 char *unitname, int linenum, char *file,FILE *errfile)
{
  char *inverse, *start, *end;
  int i;

  start = strchr(input, '[');
  if (!start){
    if (errfile) fprintf(errfile,
             "%s: expecting '[' in definition of '%s' in '%s' line %d\n",
             progname, unitname, file, linenum);
    return NULL;
  }
  *start++ = '\0';
  removespaces(input);
  if (!emptystr(input)){
    if (errfile) fprintf(errfile,
        "%s: unexpected characters (%s) before [ in definition of '%s' in '%s' line %d\n",
        progname, input, unitname, file, linenum);
    return NULL;
  }
  end = strchr(start, ']');
  if (!end){
    if (errfile) fprintf(errfile,
             "%s: expecting ']' in definition of '%s' in '%s' line %d\n",
             progname, unitname, file, linenum);
    return NULL;
  }
  *end = '\0';

  inverse = strchr(start, ';');
  if (inverse){
    *inverse++ = '\0';
    if (strchr(inverse,',')){
      if (errfile) fprintf(errfile,
                       "%s: definition of units for output of function '%s' has too many entries in '%s' line %d\n",
                        progname,unitname,file,linenum);
      return NULL;
    }
    if (0==strcmp(inverse, "*"))
      *inverse_dim = NULL;
    else
      *inverse_dim=inverse;
  }

  forward_dim[0]=start;
  start = strchr(start,',');
  for(i=1;start;i++){
    *start++ = '\0';
    if (strlen(start)==0){
      if (errfile) fprintf(errfile,
             "%s: bad function definition of '%s' in '%s' line %d\n",
              progname,unitname,file,linenum);
      return NULL;
    }
    if (i>=param_count){
      if (errfile) fprintf(errfile,
             "%s: definition of function '%s' has too many units in '%s' line %d\n",
              progname,unitname,file,linenum);
      return NULL;
    }
    forward_dim[i]=start;
    start = strchr(start,',');
  }
  for(i=0;i<param_count;i++)
    if (forward_dim[i]) {
      removespaces(forward_dim[i]);
      if (0==strcmp(forward_dim[i],"*"))
        forward_dim[i]=NULL;
    }
  return end+1;
}

#define BADFILE {if (parambuf) free(parambuf); return E_BADFILE;}

int
newfunction(char *unitname, char *unitdef, int *count,
            int linenum, char *file,FILE *errfile, int redefine)
{
  struct func *funcentry;
  char *start, *end, *inv, *inverse_dim, *parambuf;
  char *params[MAX_FUNC_PARAMS];
  char *forward_dim[MAX_FUNC_PARAMS];
  int param_count, extrachars;
  int looking_for_keywords,i,param_index;
  struct interval domain[MAX_FUNC_PARAMS];
  struct interval range;
  int noerror = 0;
  int didthis[sizeof(fnkeywords)/sizeof(fnkeywords[0])];
  int errors = 0;       /* set by parseinterval() */

  parambuf = 0;

  for(i=0;i<MAX_FUNC_PARAMS;i++){
    forward_dim[i]=NULL;
    init_interval(domain+i);
    params[i] = '\0';
  }
  inverse_dim = NULL;
  init_interval(&range);
  for(i=0;fnkeywords[i];i++) didthis[i]=0;

  if (*unitname=='('){
    if (errfile) fprintf(errfile,
         "%s: unit '%s' on line %d of '%s' ignored.  It starts with a '('\n",
         progname, unitname, linenum, file);
    BADFILE;
  }
                                   /* coverity[returned_null] */
  start = strchr(unitname,'(');
  *start++ = '\0';
  if (checkunitname(unitname,linenum,file,errfile))
    BADFILE;

  parambuf = NULL;
  extrachars=0;
  end = strchr(start,')');
  if (end){
    *end = '\0';
    if (end[1])
      extrachars=1;
  }
  else {
    end = strchr(unitdef,')');
    if (!end){
      if (errfile) fprintf(errfile,
              "%s: no ')' character after parameters for function definition of '%s' in '%s' line %d\n",
              progname,unitname,file,linenum);
      BADFILE;
    }
    if ((end[1])!=' ')
      extrachars=1;
    else {
      *end = '\0';
      parambuf = (char *) mymalloc((strlen(start)+strlen(end)+2), "(newfunction)");
      strcpy(parambuf, start);
      strcat(parambuf," ");
      strcat(parambuf, unitdef);
      unitdef=end+1;
      start=parambuf;
      removespaces(start);
    }
  }
  if (extrachars){
    if (errfile) fprintf(errfile,
              "%s: extra characters '%s' after parameter list in function definition of '%s' in '%s' line %d\n",
                         progname,end+1,unitname,file,linenum);
    BADFILE;
  }

  if (emptystr(start)){  /* no argument: function() so make a function copy */
    if (parambuf) free(parambuf);
    return copyfunction(unitname, unitdef, count, linenum, file, errfile);
  }
  *end = '\0';

  params[0]=start;
  start = strchr(start,',');
  for(param_count=1;start;param_count++){
    *start++ = '\0';
    if (strlen(start)==0){
      if (errfile) fprintf(errfile,
             "%s: bad function definition of '%s' in '%s' line %d\n",
              progname,unitname,file,linenum);
      BADFILE;
    }
    if (param_count>=MAX_FUNC_PARAMS){
      if (errfile) fprintf(errfile,
             "%s: definition of function '%s' has too many parameters in '%s' line %d\n",
              progname,unitname,file,linenum);
      BADFILE;
    }
    params[param_count]=start;
    start = strchr(start,',');
  }
  for(i=0;i<param_count;i++){
    removespaces(params[i]);
    if (strchr(params[i],' ')){
      if (errfile) fprintf(errfile,
             "%s: function '%s' has a parameter '%s' whose name contains white space in '%s' line %d\n",
             progname,unitname,params[i],file,linenum);
      BADFILE;
    }
    if (strlen(params[i])==0){
      if (errfile) fprintf(errfile,
             "%s: missing parameter in function definition of '%s' in '%s' line %d\n",
             progname,unitname,file,linenum);
      BADFILE;
    }
  }
  looking_for_keywords=1;
  while (looking_for_keywords) {
    looking_for_keywords = 0;
    for(i=0;fnkeywords[i];i++){
      if (startswith(unitdef, fnkeywords[i])){
        looking_for_keywords = 1;  /* found keyword so keep looking */
        unitdef+=strlen(fnkeywords[i]);
        if (i==FN_NOERROR){
          noerror=1;
          while(* unitdef==' ') unitdef++;
          continue;
        }
        if (didthis[i]){
          if (errfile) fprintf(errfile, \
                      "%s: keyword '%s' repeated in definition of '%s' on line %d of '%s'.\n",\
                      progname,fnkeywords[i],unitname, linenum, file);
            BADFILE;
        }
        didthis[i]=1;
        if (i==FN_UNITS){

          unitdef = parse_func_units(param_count, unitdef, forward_dim, &inverse_dim,
                                     unitname, linenum, file, errfile);
          if (!unitdef) BADFILE;
          if (inverse_dim && param_count>1){
            if (errfile) fprintf(errfile, \
                                 "%s: 'units=' specification for multivariate function '%s' has inverse dimensions on line %d of '%s'.\n",
                                 progname, unitname, linenum, file);
            BADFILE;
          }
        }
        if (i==FN_RANGE){
          if (param_count>1){
            if (errfile) fprintf(errfile, \
                                 "%s: range given in definition of multivariate function '%s' on line %d of '%s'.\n",
                                 progname, unitname, linenum, file);
            BADFILE;
          }
          unitdef = parseinterval(unitdef, &range, unitname, linenum, file, errfile, &errors, "range=");
        }
        if (i==FN_DOMAIN){
          char *x=unitdef;
          unitdef = parseinterval(unitdef, domain, unitname, linenum, file, errfile, &errors, "domain=");
          param_index=1;
          while(*unitdef && strchr(",[(",*unitdef)){
            if (*unitdef==',') unitdef++;
            if (param_index>=param_count){
              if (errfile) fprintf(errfile,
                     "%s: definition of function '%s' has too many intervals in domain in '%s' line %d\n",
                     progname,unitname,file,linenum);
              BADFILE;
            }
            unitdef = parseinterval(unitdef, domain+param_index, unitname, linenum, file, errfile, &errors, "domain=");
            param_index++;
          }
        }
        if (*unitdef!=' '){
          if (errfile) fprintf(errfile, \
                      "%s: unexpected character '%c' after '%s' specification given in\n"
                      "       definition of '%s' on line %d of '%s'.\n",
                               progname,*unitdef,fnkeywords[i],unitname, linenum, file);
            BADFILE;
        }

        while(*unitdef==' ') unitdef++;
      }
    }
  }
  /* errors from parseinterval() */
  if (errors) BADFILE;

  if (emptystr(unitdef)){
    if (errfile) fprintf(errfile,
                 "%s: function '%s' lacks a definition at line %d of '%s'\n",
                 progname, unitname, linenum, file);
    BADFILE;
  }

  if (*unitdef=='['){
    if (errfile) fprintf(errfile,
         "%s: function '%s' missing keyword before '[' on line %d of '%s'\n",
         progname, unitname, linenum, file);
    BADFILE;
  }

  /* Check that if domain and range are specified and nonzero then the
     units are given.  Otherwise these are meaningless.  */
  for(i=0;i<param_count;i++){
    if (!forward_dim[i] &&
        ((isfinite(domain[i].min) && domain[i].min!=0) ||  (isfinite(domain[i].max) && domain[i].max!=0))){
      if (errfile)
        fprintf(errfile,"%s: function '%s' defined on line %d of '%s' has domain interval without corresponding units.\n",
                progname, unitname, linenum, file);
      BADFILE;
    }
  }
  if (!inverse_dim &&
      ((isfinite(range.min) && range.min!=0) || (isfinite(range.max) && range.max!=0))){
    if (errfile)
      fprintf(errfile,"%s: function '%s' defined on line %d of '%s' has range with no units.\n",
              progname, unitname, linenum, file);
    BADFILE;
  }
  inv = strchr(unitdef,FUNCSEPCHAR);
  if (inv){
    if (param_count>1){
      if (errfile)
        fprintf(errfile,"%s: multivariate function '%s' has an inverse on line %d of '%s'.\n",
                progname, unitname, linenum, file);
      BADFILE;
    }
    *inv++ = '\0';
  }
  if ((funcentry=fnlookup(unitname))){
    if (flags.unitcheck && errfile && !redefine)
      fprintf(errfile,
             "%s: function '%s' defined on line %d of '%s' is redefined on line %d of '%s'.\n",
              progname, unitname, funcentry->linenumber,funcentry->file,
              linenum, file);
    freefunction(funcentry);
  } else {
    funcentry = (struct func*)mymalloc(sizeof(struct func),"(newfunction)");
    funcentry->name = dupstr(unitname, "(newfunction)");
    addfunction(funcentry);
    (*count)++;
  }
  funcentry->table = 0;
  funcentry->skip_error_check = noerror;

  funcentry->inverse.dimen = (char **) mymalloc(sizeof(char *), "(newfunction)");
  if (inverse_dim)
    funcentry->inverse.dimen[0] = dupstr(inverse_dim, "(newfunction)");
  else
    funcentry->inverse.dimen[0] = NULL;

  funcentry->forward.dimen = (char **) mymalloc(param_count*sizeof(char *), "(newfunction)");
  for(i=0;i<param_count;i++){
    if (forward_dim[i])
      funcentry->forward.dimen[i] = dupstr(forward_dim[i], "(newfunction)");
    else
      funcentry->forward.dimen[i] = NULL;
  }

  funcentry->forward.param_count = param_count;
  funcentry->forward.domain = (struct interval *)mymalloc(param_count*sizeof(struct interval), "(newfunction)");
  memcpy(funcentry->forward.domain, domain, param_count*sizeof(struct interval));

  funcentry->inverse.param_count = 1;
  funcentry->inverse.domain = (struct interval *)mymalloc(sizeof(struct interval), "(newfunction)");
  memcpy(funcentry->inverse.domain, &range, sizeof(struct interval));

  funcentry->forward.param = (char **) mymalloc(param_count*sizeof(char*), "(newfunction)");
  for(i=0;i<param_count;i++)
    funcentry->forward.param[i] = dupstr(params[i], "(newfunction)");
  removespaces(unitdef);
  funcentry->forward.def = dupstr(unitdef, "(newfunction)");

  if (inv){
    removespaces(inv);
    funcentry->inverse.def = dupstr(inv, "(newfunction)");
    funcentry->inverse.param = (char **) mymalloc(sizeof(char *), "(newfunction)");
    funcentry->inverse.param[0] = dupstr(unitname, "(newfunction)");
  }
  else {
    funcentry->inverse.def = 0;
    funcentry->inverse.param = 0;
  }
  funcentry->linenumber = linenum;
  funcentry->file = file;
  if (parambuf) free(parambuf);
  return 0;
}


int
newtable(char *unitname,char *unitdef, int *count,
         int linenum, char *file,FILE *errfile, int redefine)
{
  char *start, *end;
  char *tableunit;
  int tablealloc, tabpt;
  struct pair *tab;
  struct func *funcentry;
  int noerror = 0;

                                      /* coverity[returned_null] */
  tableunit = strchr(unitname,'[');
  end = strchr(unitname,']');
  *tableunit++ = '\0';
  if (checkunitname(unitname, linenum, file, errfile))
    return E_BADFILE;
  if (!end){
    if (errfile) fprintf(errfile,"%s: missing ']' in units file '%s' line %d\n",
              progname,file,linenum);
    return E_BADFILE;
  }
  if (strlen(end)>1){
    if (errfile) fprintf(errfile,
              "%s: unexpected characters after ']' in units file '%s' line %d\n",
              progname,file,linenum);
    return E_BADFILE;
  }
  *end = '\0';
  tab = (struct pair *)mymalloc(sizeof(struct pair)*20, "(newtable)");
  tablealloc=20;
  tabpt = 0;
  start = unitdef;
  if (startswith(start, NOERROR_KEYWORD)) {
    noerror = 1;
    start += strlen(NOERROR_KEYWORD);
    removespaces(start);
  }
  while (1) {
    if (tabpt>=tablealloc){
      tablealloc+=20;
      tab = (struct pair *)realloc(tab,sizeof(struct pair)*tablealloc);
      if (!tab){
        if (errfile) fprintf(errfile, "%s: memory allocation error (newtable)\n",
                  progname);
        return E_MEMORY;
      }
    }
    tab[tabpt].location = strtod(start,&end);
    if (start==end || (!emptystr(end) && *end !=' ')){
      if (!emptystr(start)) {
        if (strlen(start)>15) start[15] = '\0';  /* Truncate for error msg display */
        if (errfile) fprintf(errfile,
             "%s: cannot parse table definition %s at '%s' on line %d of '%s'\n",
             progname, unitname, start, linenum, file);
        free(tab);
        return E_BADFILE;
      }
      break;
    }
    if (tabpt>0 && tab[tabpt].location<=tab[tabpt-1].location){
      if (errfile)
        fprintf(errfile,"%s: points don't increase (" ERRNUMFMT " to " ERRNUMFMT
                        ") in units file '%s' line %d\n",
                progname, tab[tabpt-1].location, tab[tabpt].location,
                file, linenum);
      free(tab);
      return E_BADFILE;
    }
    start=end+strspn(end," ");
    tab[tabpt].value = strtod(start,&end);
    if (start==end){
      if (errfile)
        fprintf(errfile,"%s: missing value after " ERRNUMFMT " in units file '%s' line %d\n",
                progname, tab[tabpt].location, file, linenum);
      free(tab);
      return E_BADFILE;
    }
    tabpt++;
    start=end+strspn(end," ,");
  }
  if ((funcentry=fnlookup(unitname))){
    if (flags.unitcheck && errfile && !redefine)
      fprintf(errfile,
      "%s: unit '%s' defined on line %d of '%s' is redefined on line %d of '%s'.\n",
                    progname, unitname, funcentry->linenumber,funcentry->file,
                                         linenum, file);
    freefunction(funcentry);
  } else {
    funcentry = (struct func *)mymalloc(sizeof(struct func),"(newtable)");
    funcentry->name = dupstr(unitname, "(newtable)");
    addfunction(funcentry);
    (*count)++;
  }
  funcentry->tableunit = dupstr(tableunit, "(newtable)");
  funcentry->tablelen = tabpt;
  funcentry->table = tab;
  funcentry->skip_error_check = noerror;
  funcentry->linenumber = linenum;
  funcentry->file = file;
  return 0;
}


int
newalias(char *unitname, char *unitdef,int linenum, char *file,FILE *errfile)
{
  struct wantalias *aliasentry;

  if (!strchr(unitdef, UNITSEPCHAR)){
    if (errfile) fprintf(errfile,
            "%s: unit list missing '%c' on line %d of '%s'\n",
            progname, UNITSEPCHAR, linenum, file);
    return E_BADFILE;
  }
  if ((aliasentry=aliaslookup(unitname))){   /* duplicate alias */
    if (flags.unitcheck && errfile)
      fprintf(errfile,
              "%s: unit list '%s' defined on line %d of '%s' is redefined on line %d of '%s'.\n",
              progname, unitname, aliasentry->linenumber,
              aliasentry->file, linenum, file);
    free(aliasentry->definition);
  } else {
    aliasentry = (struct wantalias *)
      mymalloc(sizeof(struct wantalias),"(newalias)");
    aliasentry->name = dupstr(unitname, "(newalias)");
    aliasentry->next = 0;
    *aliaslistend = aliasentry;
    aliaslistend = &aliasentry->next;
  }
  aliasentry->definition = dupstr(unitdef, "(newalias)");
  aliasentry->linenumber = linenum;
  aliasentry->file = file;
  return 0;
}


/*
   Check environment variable name to see if its value appears on the
   space delimited text string pointed to by list.  Returns 2 if the
   environment variable is not set, return 1 if its value appears on
   the list and zero otherwise.
*/

int
checkvar(char *name, char *list)
{
  char *listitem;
  name = getenv(name);
  if (!name)
    return 2;
  listitem = strtok(list," ");
  while (listitem){
    if (!strcmp(name, listitem))
      return 1;
    listitem = strtok(0," ");
  }
  return 0;
}


#ifdef NO_SETENV
int
setenv(const char *name, const char *val, int overwrite)
{
  char *environment;

  if (!overwrite && getenv(name) != NULL)
    return 0;
  environment = (char *) malloc(strlen(name) + strlen(val) + 2);
  if (!environment)
    return 1;
  strcpy(environment, name);
  strcat(environment, "=");
  strcat(environment, val);

  /* putenv() doesn't copy its argument, so don't free environment */

#if defined (_WIN32) && defined (_MSC_VER)
  return _putenv(environment);
#else
  return putenv(environment);
#endif
}
#endif

#ifdef _WIN32
#  define  isdirsep(c)  ((c) == '/' || (c) == '\\')
#  define  hasdirsep(s) strpbrk((s),"/\\")
#else
#  define  isdirsep(c)  ((c) == '/')
#  define  hasdirsep(s) strchr((s),'/')
#endif
#define  isexe(s) ((strlen(s) == 4) && (tolower(s[1]) == 'e') \
                 && (tolower(s[2]) == 'x') && (tolower(s[3]) == 'e'))

/* Returns a pointer to the end of the pathname part of the
   specified filename */

char *
pathend(char *filename)
{
  char *pointer;

  for(pointer=filename+strlen(filename);pointer>filename;pointer--){
    if (isdirsep(*pointer)) {
      pointer++;
      break;
    }
  }
  return pointer;
}

int
isfullpath(char *path)
{
#ifdef _WIN32
  /* handle Windows drive specifier */
  if (isalpha(*path) && *(path + 1) == ':')
      path += 2;
#endif
  return isdirsep(*path);
}

/*
   Read in units data.

   file - Filename to load
   errfile - File to receive messages about errors in the units database.
             Set it to 0 to suppress errors.
   unitcount, prefixcount, funccount - Return statistics to the caller.
                                       Must initialize to zero before calling.
   depth - Used to prevent recursive includes.  Call with it set to zero.

   The global variable progname is used in error messages.
*/

int
readunits(char *file, FILE *errfile,
          int *unitcount, int *prefixcount, int *funccount, int depth)
{
   FILE *unitfile;
   char *line = NULL, *lineptr, *unitdef, *unitname, *permfile;
   int linenum, linebufsize, goterr, retcode;
   int locunitcount, locprefixcount, locfunccount, redefinition;
   int wronglocale = 0;   /* If set then we are currently reading data */
   int inlocale = 0;      /* for the wrong locale so we should skip it */
   int in_utf8 = 0;       /* If set we are reading utf8 data */
   int invar = 0;         /* If set we are in data for an env variable.*/
   int wrongvar = 0;      /* If set then we are not processing */

   locunitcount = 0;
   locprefixcount = 0;
   locfunccount  = 0;
   linenum = 0;
   linebufsize = 0;
   goterr = 0;

   unitfile = openfile(file, "rt");

   if (!unitfile){
     if (errfile)
       fprintf(errfile, "%s: Unable to read units file '%s': %s\n", progname, file, strerror(errno));
     return E_FILE;
   }
   growbuffer(&line,&linebufsize);
                                            /* coverity[alloc_fn] */
   permfile = dupstr(file, "(readunits)");    /* This is a permanent copy to reference in */
                               /* the database. It is never freed. */
   while (!feof(unitfile)) {
      if (!fgetslong(&line, &linebufsize, unitfile, &linenum))
        break;
      if (linenum==1 && startswith(line, UTF8MARKER)){
        lineptr = line + strlen(UTF8MARKER);
        memmove(line, lineptr, strlen(lineptr) + 1);
      }
      strip_comment(line);
      if (-1 == strwidth(line)){
        readerror(errfile, "%s: %s on line %d of '%s'\n",
                      progname, invalid_utf8, linenum, file);
        continue;
      }
      replace_operators(line);

      if (*line == COMMANDCHAR) {         /* Process units file commands */
        unitname = strtok(line+1, " ");
        if (!unitname){
          readerror(errfile, VAGUE_ERR);
          continue;
        }

        /* Check for locale and utf8 declarations.  Any other commands
           most likely belong after these tests. */
        if (!strcmp(unitname,"var") || !strcmp(unitname,"varnot")){
          int not = 0;
          if (unitname[3]=='n')
            not = 1;
          unitname=strtok(0," ");
          unitdef=strtok(0,"");  /* Get rest of the line */
          if (!unitname)
            readerror(errfile,
                      "%s: no variable name specified on line %d of '%s'\n",
                      progname, linenum, file);
          else if (!unitdef)
            readerror(errfile,
                      "%s: no value specified on line %d of '%s'\n",
                      progname, linenum, file);
          else if (invar)
            readerror(errfile,
                 "%s: nested var statements not allowed, line %d of '%s'\n",
                      progname, linenum, file);
          else {
            int check;
            invar = 1;
            check = checkvar(unitname, unitdef);
            if (check==2){
              readerror(errfile,
                 "%s: environment variable %s not set at line %d of '%s'\n",
                         progname, unitname, linenum, file);
              wrongvar = 1;
            }
            else if (!(not^check))
              wrongvar = 1;
          }
          continue;
        }
        else if (!strcmp(unitname, "endvar")){
          if (!invar)
            readerror(errfile,
                      "%s: unmatched !endvar on line %d of '%s'\n",
                      progname, linenum, file);
          wrongvar = 0;
          invar = 0;
          continue;
        }
        else if (!strcmp(unitname,"locale")){
          unitname = strtok(0, " ");
          if (!unitname)
            readerror(errfile,
                      "%s: no locale specified on line %d of '%s'\n",
                      progname, linenum, file);
          else if (inlocale)
            readerror(errfile,
                      "%s: nested locales not allowed, line %d of '%s'\n",
                      progname, linenum, file);
          else {
            inlocale = 1;
            if (strcmp(unitname,mylocale))  /* locales don't match           */
              wronglocale = 1;
          }
          continue;
        }
        else if (!strcmp(unitname, "endlocale")){
          if (!inlocale)
            readerror(errfile,
                      "%s: unmatched !endlocale on line %d of '%s'\n",
                      progname, linenum, file);
          wronglocale = 0;
          inlocale = 0;
          continue;
        }
        else if (!strcmp(unitname, "utf8")){
          if (in_utf8)
            readerror(errfile,"%s: nested utf8 not allowed, line %d of '%s'\n",
                              progname, linenum, file);
          else in_utf8 = 1;
          continue;
        }
        else if (!strcmp(unitname, "endutf8")){
          if (!in_utf8)
            readerror(errfile,"%s: unmatched !endutf8 on line %d of '%s'\n",
                      progname, linenum, file);
          in_utf8 = 0;
          continue;
        }
        if (in_utf8 && !utf8mode) continue;
        if (wronglocale || wrongvar) continue;

        if (!strcmp(unitname,"prompt")){
          unitname = strtok(0,"");     /* Rest of the line */
          if (promptprefix)
            free(promptprefix);
          if (!unitname)
            promptprefix = NULL;
          else
            promptprefix = dupstr(unitname, "(readunits)");
          continue;
        }
        if (!strcmp(unitname,"message")){
          unitname = strtok(0,"");     /* Rest of the line */
          if (!flags.quiet){
            if (unitname) logputs(unitname);
            logputchar('\n');
          }
          continue;
        }
        else if (!strcmp(unitname,"set")) {
          unitname = strtok(0," ");
          unitdef = strtok(0," ");
          if (!unitname)
            readerror(errfile,
                      "%s: no variable name specified on line %d of '%s'\n",
                      progname, linenum, file);
          else if (!unitdef)
            readerror(errfile,
                      "%s: no value specified on line %d of '%s'\n",
                      progname, linenum, file);
          else
            setenv(unitname, unitdef, 0);
          continue;
        }
        else if (!strcmp(unitname,"unitlist")){
          splitline(0,&unitname, &unitdef); /* 0 continues strtok call */
          if (!unitname || !unitdef)
            readerror(errfile,VAGUE_ERR);
          else {
            if (newalias(unitname, unitdef, linenum, permfile, errfile))
              goterr = 1;
          }
          continue;
        }
        else if (!strcmp(unitname, "include")){
          if (depth>MAXINCLUDE){
            readerror(errfile,
                "%s: max include depth of %d exceeded in file '%s' line %d\n",
                progname, MAXINCLUDE, file, linenum);
          } else {
            int readerr;
            char *includefile;

            unitname = strtok(0, " ");
            if (!unitname){
              readerror(errfile,
                        "%s: missing include filename on line %d of '%s'\n",
                        progname, linenum, file);
              continue;
            }
            includefile = mymalloc(strlen(file)+strlen(unitname)+1, "(readunits)");
            if (isfullpath(unitname))
              strcpy(includefile,unitname);
            else {
              strcpy(includefile,file);
              strcpy(pathend(includefile), unitname);
            }

            readerr = readunits(includefile, errfile, unitcount, prefixcount,
                                funccount, depth+1);
            if (readerr == E_MEMORY){
              fclose(unitfile);
              free(line);
              free(includefile);
              return readerr;
            }
            if (readerr == E_FILE) {
              readerror(errfile, "%s:   file was included at line %d of file '%s'\n", progname,linenum, file);
            }

            if (readerr)
              goterr = 1;
            free(includefile);
          }
        } else                             /* not a valid command */
          readerror(errfile,VAGUE_ERR);
        continue;
      }
      if (in_utf8 && !utf8mode) continue;
      if (wronglocale || wrongvar) continue;
      splitline(line, &unitname, &unitdef);
      if (!unitname) continue;

      if (*unitname == REDEFCHAR){
        unitname++;
        redefinition=1;
        if (strlen(unitname)==0){
          readerror(errfile,
               "%s: expecting name of unit to redefine after '+' at line %d of '%s'\n",
               progname, linenum,file);
          continue;
        }
      } else
        redefinition=0;
      if (!strcmp(unitname,"-")) {
          readerror(errfile,
               "%s: expecting prefix name before '-' at line %d of '%s'\n",
               progname, linenum,file);
          continue;
      }
      if (!unitdef){
        readerror(errfile,
              "%s: unit '%s' lacks a definition at line %d of '%s'\n",
                  progname, unitname, linenum, file);
        continue;
      }

      if (lastchar(unitname) == '-'){      /* it's a prefix definition */
        if (newprefix(unitname,unitdef,&locprefixcount,linenum,
                      permfile,errfile,redefinition))
          goterr=1;
      }
      else if (strchr(unitname,'[')){     /* table definition  */
        retcode=newtable(unitname,unitdef,&locfunccount,linenum,
                         permfile,errfile,redefinition);
        if (retcode){
          if (retcode != E_BADFILE){
            fclose(unitfile);
            free(line);
            return retcode;
          }
          goterr=1;
        }
      }
      else if (strchr(unitname,'(')){     /* function definition */
        if (newfunction(unitname,unitdef,&locfunccount,linenum,
                        permfile,errfile,redefinition))
          goterr = 1;
      }
      else {                              /* ordinary unit definition */
        if (newunit(unitname,unitdef,&locunitcount,linenum,permfile,errfile,redefinition,0))
          goterr = 1;
      }
   }
   fclose(unitfile);
   free(line);
   if (unitcount)
     *unitcount+=locunitcount;
   if (prefixcount)
     *prefixcount+=locprefixcount;
   if (funccount)
     *funccount+=locfunccount;
   if (goterr)
     return E_BADFILE;
   else return 0;
}

/* Initialize a unit to be equal to 1. */

void
initializeunit(struct unittype *theunit)
{
   theunit->factor = 1.0;
   theunit->numerator[0] = theunit->denominator[0] = NULL;
}


/* Free a unit: frees all the strings used in the unit structure.
   Does not free the unit structure itself.  */

void
freeunit(struct unittype *theunit)
{
   char **ptr;

   for(ptr = theunit->numerator; *ptr; ptr++)
     if (*ptr != NULLUNIT) free(*ptr);
   for(ptr = theunit->denominator; *ptr; ptr++)
     if (*ptr != NULLUNIT) free(*ptr);

   /* protect against repeated calls to freeunit() */

   theunit->numerator[0] = 0;
   theunit->denominator[0] = 0;
}


/* Print out a unit  */

void
showunit(struct unittype *theunit)
{
   char **ptr;
   int printedslash;
   int counter = 1;

   logprintf(num_format.format, theunit->factor);

   for (ptr = theunit->numerator; *ptr; ptr++) {
      if (ptr > theunit->numerator && **ptr &&
          !strcmp(*ptr, *(ptr - 1)))
         counter++;
      else {
         if (counter > 1)
           logprintf("%s%d", powerstring, counter);
         if (**ptr)
           logprintf(" %s", *ptr);
         counter = 1;
      }
   }
   if (counter > 1)
     logprintf("%s%d", powerstring, counter);
   counter = 1;
   printedslash = 0;
   for (ptr = theunit->denominator; *ptr; ptr++) {
      if (ptr > theunit->denominator && **ptr &&
          !strcmp(*ptr, *(ptr - 1)))
         counter++;
      else {
         if (counter > 1)
           logprintf("%s%d", powerstring, counter);
         if (**ptr) {
            if (!printedslash)
              logprintf(" /");
            printedslash = 1;
            logprintf(" %s", *ptr);
         }
         counter = 1;
      }
   }
   if (counter > 1)
     logprintf("%s%d", powerstring, counter);
}


/* qsort comparison function */

int
compare(const void *item1, const void *item2)
{
   return strcmp(*(char **) item1, *(char **) item2);
}

/* Sort numerator and denominator of a unit so we can compare different
   units */

void
sortunit(struct unittype *theunit)
{
   char **ptr;
   int count;

   for (count = 0, ptr = theunit->numerator; *ptr; ptr++, count++);
   qsort(theunit->numerator, count, sizeof(char *), compare);
   for (count = 0, ptr = theunit->denominator; *ptr; ptr++, count++);
   qsort(theunit->denominator, count, sizeof(char *), compare);
}


/* Cancels duplicate units that appear in the numerator and
   denominator.  The input unit must be sorted. */

void
cancelunit(struct unittype *theunit)
{
   char **den, **num;
   int comp;

   den = theunit->denominator;
   num = theunit->numerator;

   while (*num && *den) {
      comp = strcmp(*den, *num);
      if (!comp) { /* units match, so cancel them */
         if (*den!=NULLUNIT) free(*den);
         if (*num!=NULLUNIT) free(*num);
         *den++ = NULLUNIT;
         *num++ = NULLUNIT;
      } else if (comp < 0) /* Move up whichever pointer is alphabetically */
         den++;            /* behind to look for future matches */
      else
         num++;
   }
}


/*
   Looks up the definition for the specified unit including prefix processing
   and plural removal.

   Returns a pointer to the definition or a null pointer
   if the specified unit does not appear in the units table.

   Sometimes the returned pointer will be a pointer to the special
   buffer created to hold the data.  This buffer grows as needed during
   program execution.

   Note that if you pass the output of lookupunit() back into the function
   again you will get correct results, but the data you passed in may get
   clobbered if it happened to be the internal buffer.
*/

static int bufsize=0;
static char *buffer;  /* buffer for lookupunit answers with prefixes */


/*
  Plural rules for english: add -s
  after x, sh, ch, ss   add -es
  -y becomes -ies except after a vowel when you just add -s as usual
*/


char *
lookupunit(char *unit,int prefixok)
{
   char *copy;
   struct prefixlist *pfxptr;
   struct unitlist *uptr;

   if ((uptr = ulookup(unit)))
      return uptr->value;

   if (strwidth(unit)>2 && lastchar(unit) == 's') {
      copy = dupstr(unit, "(lookupunit)");
      lastchar(copy) = 0;
      if (lookupunit(copy,prefixok)){
         while(strlen(copy)+1 > bufsize) {
            growbuffer(&buffer, &bufsize);
         }
         strcpy(buffer, copy);  /* Note: returning looked up result seems   */
         free(copy);            /*   better but it causes problems when it  */
         return buffer;         /*   contains PRIMITIVECHAR.                */
      }
      if (strlen(copy)>2 && lastchar(copy) == 'e') {
         lastchar(copy) = 0;
         if (lookupunit(copy,prefixok)){
            while (strlen(copy)+1 > bufsize) {
               growbuffer(&buffer,&bufsize);
            }
            strcpy(buffer,copy);
            free(copy);
            return buffer;
         }
      }
      if (strlen(copy)>2 && lastchar(copy) == 'i') {
         lastchar(copy) = 'y';
         if (lookupunit(copy,prefixok)){
            while (strlen(copy)+1 > bufsize) {
               growbuffer(&buffer,&bufsize);
            }
            strcpy(buffer,copy);
            free(copy);
            return buffer;
         }
      }
      free(copy);
   }
   if (prefixok && (pfxptr = plookup(unit))) {
      copy = unit + pfxptr->len;
      if (emptystr(copy) || lookupunit(copy,0)) {
         char *tempbuf;
         while (strlen(pfxptr->value)+strlen(copy)+2 > bufsize){
            growbuffer(&buffer, &bufsize);
         }
         tempbuf = dupstr(copy, "(lookupunit)");   /* copy might point into buffer */
         strcpy(buffer, pfxptr->value);
         strcat(buffer, " ");
         strcat(buffer, tempbuf);
         free(tempbuf);
         return buffer;
      }
   }
   return NULL;
}


/* Points entries of product[] to the strings stored in tomove[].
   Leaves tomove pointing to a list of NULLUNITS.  */

int
moveproduct(char *product[], char *tomove[])
{
   char **dest, **src;

   dest=product;
   for(src = tomove; *src; src++){
     if (*src == NULLUNIT) continue;
     for(; *dest && *dest != NULLUNIT; dest++);
     if (dest - product >= MAXSUBUNITS - 1) {
       return E_PRODOVERFLOW;
     }
     if (!*dest)
        *(dest + 1) = '\0';
     *dest = *src;
     *src=NULLUNIT;
   }
   return 0;
}

/*
   Make a copy of a product list.  Note that no error checking is done
   for overflowing the product list because it is assumed that the
   source list doesn't overflow, so the destination list shouldn't
   overflow either.  (This assumption could be false if the
   destination is not actually at the start of a product buffer.)
*/

void
copyproduct(char **dest, char **source)
{
   for(;*source;source++,dest++) {
     if (*source==NULLUNIT)
       *dest = NULLUNIT;
     else
       *dest=dupstr(*source, "(copyproduct)");
   }
   *dest = NULL;
}

/*
    Make a copy of a unit
   The destination will have freeunit() run on it so make sure
   it has been initialized.
*/

void
unitcopy(struct unittype *dest, struct unittype *source)
{
  freeunit(dest);
  dest->factor = source->factor;
  copyproduct(dest->numerator, source->numerator);
  copyproduct(dest->denominator, source->denominator);
}


/* Multiply left by right.  In the process, all of the units are
   deleted from right (but it is not freed) */

int
multunit(struct unittype *left, struct unittype *right)
{
  int myerr;
  left->factor *= right->factor;
  myerr = moveproduct(left->numerator, right->numerator);
  if (!myerr)
    myerr = moveproduct(left->denominator, right->denominator);
  return myerr;
}

int
divunit(struct unittype *left, struct unittype *right)
{
  int myerr;
  left->factor /= right->factor;
  myerr = moveproduct(left->numerator, right->denominator);
  if (!myerr)
    myerr = moveproduct(left->denominator, right->numerator);
  return myerr;
}


/*
   reduces a product of symbolic units to primitive units.
   The three low bits are used to return flags:

   bit 0 set if reductions were performed without error.
   bit 1 set if no reductions are performed.
   bit 2 set if an unknown unit is discovered.

   Return values from multiple calls will be ORed together later.
 */

#define DIDREDUCTION    (1<<0)
#define NOREDUCTION     (1<<1)
#define REDUCTIONERROR  (1<<2)
#define CIRCULARDEF     (1<<3)

int
reduceproduct(struct unittype *theunit, int flip)
{

   char *toadd;
   char **product;
   int didsomething = NOREDUCTION;
   struct unittype newunit;
   int ret;
   int itcount=0;    /* Count iterations to catch infinite loops */

   if (flip)
      product = theunit->denominator;
   else
      product = theunit->numerator;

   for (; *product; product++) {
      for (;;) {
         if (!strlen(*product))
            break;

         /* check for infinite loop */
         itcount++;
         if (itcount>MAXPRODUCTREDUCTIONS)
           return CIRCULARDEF;

         toadd = lookupunit(*product,1);
         if (!toadd) {
            if (!irreducible)
              irreducible = dupstr(*product, "(reduceproduct)");
            return REDUCTIONERROR;
         }
         if (strchr(toadd, PRIMITIVECHAR))
            break;
         didsomething = DIDREDUCTION;
         if (*product != NULLUNIT) {
            free(*product);
            *product = NULLUNIT;
         }
         if (parseunit(&newunit, toadd, 0, 0))
            return REDUCTIONERROR;
         if (flip) ret=divunit(theunit,&newunit);
         else ret=multunit(theunit,&newunit);
         freeunit(&newunit);
         if (ret)
            return REDUCTIONERROR;
      }
   }
   return didsomething;
}


#if 0
void showunitdetail(struct unittype *foo)
{
  char **ptr;

  printf("%.17g ", foo->factor);

  for(ptr=foo->numerator;*ptr;ptr++)
    if (*ptr==NULLUNIT) printf("NULL ");
    else printf("`%s' ", *ptr);
  printf(" / ");
  for(ptr=foo->denominator;*ptr;ptr++)
    if (*ptr==NULLUNIT) printf("NULL ");
    else printf("`%s' ", *ptr);
  putchar('\n');
}
#endif


/*
   Reduces numerator and denominator of the specified unit.
   Returns 0 on success, or 1 on unknown unit error.
 */

int
reduceunit(struct unittype *theunit)
{
   int ret;

   if (irreducible)
     free(irreducible);
   irreducible = NULL;
   ret = DIDREDUCTION;

   /* Keep calling reduceproduct until it doesn't do anything */

   while (ret & DIDREDUCTION) {
      ret = reduceproduct(theunit, 0);
      if (!(ret & REDUCTIONERROR))
        ret |= reduceproduct(theunit, 1);
      if (ret & REDUCTIONERROR){
         if (irreducible)
           return E_UNKNOWNUNIT;
         else
           return E_REDUCE;
      }
      else if (ret & CIRCULARDEF)
        return E_CIRCULARDEF;
   }
   return 0;
}

/*
   Returns one if the argument unit is defined in the data file as a
  dimensionless unit.  This is determined by comparing its definition to
  the string NODIM.
*/

int
ignore_dimless(char *name)
{
  struct unitlist *ul;
  if (!name)
    return 0;
  ul = ulookup(name);
  if (ul && !strcmp(ul->value, NODIM))
    return 1;
  return 0;
}

int
ignore_nothing(char *name)
{
  return 0;
}


int
ignore_primitive(char *name)
{
  struct unitlist *ul;
  if (!name)
    return 0;
  ul = ulookup(name);
  if (ul && strchr(ul->value, PRIMITIVECHAR))
    return 1;
  return 0;
}


/*
   Compare two product lists, return zero if they match and one if
   they do not match.  They may contain embedded NULLUNITs which are
   ignored in the comparison. Units defined as NODIM are also ignored
   in the comparison.
*/

int
compareproducts(char **one, char **two, int (*isdimless)(char *name))
{
   int oneblank, twoblank;
   while (*one || *two) {
      oneblank = (*one==NULLUNIT) || isdimless(*one);
      twoblank = (*two==NULLUNIT) || isdimless(*two);
      if (!*one && !twoblank)
         return 1;
      if (!*two && !oneblank)
         return 1;
      if (oneblank)
         one++;
      else if (twoblank)
         two++;
      else if (strcmp(*one, *two))
         return 1;
      else
         one++, two++;
   }
   return 0;
}


/* Return zero if units are compatible, nonzero otherwise.  The units
   must be reduced, sorted and canceled for this to work.  */

int
compareunits(struct unittype *first, struct unittype *second,
             int (*isdimless)(char *name))
{
   return
      compareproducts(first->numerator, second->numerator, isdimless) ||
      compareproducts(first->denominator, second->denominator, isdimless);
}


/* Reduce a unit as much as possible */

int
completereduce(struct unittype *unit)
{
   int err;

   if ((err=reduceunit(unit)))
     return err;

   sortunit(unit);
   cancelunit(unit);
   return 0;
}


/* Raise theunit to the specified power.  This function does not fill
   in NULLUNIT gaps, which could be considered a deficiency. */

int
expunit(struct unittype *theunit, int  power)
{
  char **numptr, **denptr;
  double thefactor;
  int i, uind, denlen, numlen;

  if (power==0){
    freeunit(theunit);
    initializeunit(theunit);
    return 0;
  }
  numlen=0;
  for(numptr=theunit->numerator;*numptr;numptr++) numlen++;
  denlen=0;
  for(denptr=theunit->denominator;*denptr;denptr++) denlen++;
  thefactor=theunit->factor;
  for(i=1;i<power;i++){
    theunit->factor *= thefactor;
    for(uind=0;uind<numlen;uind++){
      if (theunit->numerator[uind]!=NULLUNIT){
        if (numptr-theunit->numerator>=MAXSUBUNITS-1) {
           *numptr = *denptr = NULL;
           return E_PRODOVERFLOW;
        }
        *numptr++=dupstr(theunit->numerator[uind], "(expunit)");
      }
    }
    for(uind=0;uind<denlen;uind++){
      if (theunit->denominator[uind]!=NULLUNIT){
        *denptr++=dupstr(theunit->denominator[uind], "(expunit)");
        if (denptr-theunit->denominator>=MAXSUBUNITS-1) {
          *numptr = *denptr = NULL;
          return E_PRODOVERFLOW;
        }
      }
    }
  }
  *numptr = NULL;
  *denptr = NULL;
  return 0;
}


int
unit2num(struct unittype *input)
{
  struct unittype one;
  int err;

  initializeunit(&one);
  if ((err=completereduce(input)))
    return err;
  if (compareunits(input,&one,ignore_nothing))
    return E_NOTANUMBER;
  freeunit(input);
  return 0;
}


int
unitdimless(struct unittype *input)
{
  struct unittype one;
  initializeunit(&one);
  if (compareunits(input, &one, ignore_dimless))
    return 0;
  freeunit(input);  /* Eliminate dimensionless units from list */
  return 1;
}


/*
   The unitroot function takes the nth root of an input unit which has
   been completely reduced.  Returns 1 if the unit is not a power of n.
   Input data can contain NULLUNITs.
*/

int
subunitroot(int n,char *current[], char *out[])
{
  char **ptr;
  int count=0;

  while(*current==NULLUNIT) current++;  /* skip past NULLUNIT entries */
  ptr=current;
  while(*ptr){
    while(*ptr){
      if (*ptr!=NULLUNIT){
        if (strcmp(*current,*ptr)) break;
        count++;
      }
      ptr++;
    }
    if (count % n != 0){  /* If not dimensionless generate error, otherwise */
      if (!ignore_dimless(*current))    /* just skip over it */
        return E_NOTROOT;
    } else {
      for(count /= n;count>0;count--) *(out++) = dupstr(*current, "(subunitroot)");
    }
    current=ptr;
  }
  *out = '\0';
  return 0;
}


int
rootunit(struct unittype *inunit,int n)
{
   struct unittype outunit;
   int err;

   initializeunit(&outunit);
   if ((err=completereduce(inunit)))
     return err;
   /* Roots of negative numbers fail in pow(), even odd roots */
   if (inunit->factor < 0)
     return E_NOTROOT;
   outunit.factor = pow(inunit->factor,1.0/(double)n);
   if ((err = subunitroot(n, inunit->numerator, outunit.numerator)))
     return err;
   if ((err = subunitroot(n, inunit->denominator, outunit.denominator)))
     return err;
   freeunit(inunit);
   initializeunit(inunit);
   return multunit(inunit,&outunit);
}


/* Compute the inverse of a unit (1/theunit) */

void
invertunit(struct unittype *theunit)
{
  char **ptr, *swap;
  int numlen, length, ind;

  theunit->factor = 1.0/theunit->factor;
  length=numlen=0;
  for(ptr=theunit->denominator;*ptr;ptr++,length++);
  for(ptr=theunit->numerator;*ptr;ptr++,numlen++);
  if (numlen>length)
    length=numlen;
  for(ind=0;ind<=length;ind++){
    swap = theunit->numerator[ind];
    theunit->numerator[ind] = theunit->denominator[ind];
    theunit->denominator[ind] = swap;
  }
}


int
float2rat(double y, int *p, int *q)
{
  int coef[20];           /* How long does this buffer need to be? */
  int i,termcount,saveq;
  double fracpart,x;

  /* Compute continued fraction approximation */

  x=y;
  termcount=0;
  while(1){
    double coefficient = floor(x);
    if (!isfinite(coefficient) || coefficient < INT_MIN || coefficient > INT_MAX)
      return 0;
    coef[termcount] = (int) coefficient;
    fracpart = x-coef[termcount];
    if (fracpart < .001 || termcount==19) break;
    x = 1/fracpart;
    termcount++;
  }

  /* Compress continued fraction into rational p/q */

  *p=0;
  *q=1;
  for(i=termcount;i>=1;i--) {
    long long nextq = (long long)coef[i] * *q + *p;
    if (nextq < INT_MIN || nextq > INT_MAX) return 0;
    saveq=*q;
    *q = (int)nextq;
    *p = saveq;
  }
  long long numerator = (long long)*p + (long long)*q * coef[0];
  if (numerator < INT_MIN || numerator > INT_MAX || *q == 0) return 0;
  *p = (int)numerator;
  return *q<MAXSUBUNITS && fabs((double)*p / (double)*q - y) < DBL_EPSILON;
}


/* Raise a unit to a power */

int
unitpower(struct unittype *base, struct unittype *exponent)
{
  int errcode, p, q;

  errcode = unit2num(exponent);
  if (errcode == E_NOTANUMBER)
    return E_DIMEXPONENT;
  if (errcode)
    return errcode;
  errcode = unit2num(base);
  if (!errcode){                     /* Exponent base is dimensionless */
    base->factor = pow(base->factor,exponent->factor);
    if (errno)
      return E_FUNC;
  }
  else if (errcode==E_NOTANUMBER) {          /* Base not dimensionless */
    if (!float2rat(exponent->factor,&p,&q)){ /* Exponent must be rational */
      if (unitdimless(base)){
        base->factor = pow(base->factor,exponent->factor);
        if (errno)
          return E_FUNC;
      }
      else
        return E_IRRATIONAL_EXPONENT;
    } else {
      if (q!=1) {
        errcode = rootunit(base, q);
        if (errcode == E_NOTROOT)
          return E_BASE_NOTROOT;
        if (errcode)
          return errcode;
      }
      errcode = expunit(base, abs(p));
      if (errcode)
        return errcode;
      if (p<0)
        invertunit(base);
    }
  }
  else return errcode;
  return 0;
}


/* Old units program would give message about what each operand
   reduced to, showing that they weren't conformable.  Can this
   be achieved here?  */

int
addunit(struct unittype *unita, struct unittype *unitb)
{
  int err;

  if ((err=completereduce(unita)))
    return err;
  if ((err=completereduce(unitb)))
    return err;
  if (compareunits(unita,unitb,ignore_nothing))
    return E_BADSUM;
  unita->factor += unitb->factor;
  freeunit(unitb);
  return 0;
}


int
gtlt_unit(int comp,struct unittype *unita, struct unittype *unitb)
{
  int err;
  float result;

  if ((err=completereduce(unita)))
    return err;
  if ((err=completereduce(unitb)))
    return err;
  if (compareunits(unita,unitb,ignore_nothing))
    return E_BADCOMP;
  if (comp==COMP_LT) { if (unita->factor < unitb->factor) result=1; else result=0;}
  else if (comp==COMP_GT) { if (unita->factor > unitb->factor) result=1; else result=0;}
  else if (comp==COMP_EQ) { if (unita->factor == unitb->factor) result=1; else result=0;}
  else if (comp==COMP_GTE) { if (unita->factor >= unitb->factor) result=1; else result=0;}
  else if (comp==COMP_LTE) { if (unita->factor <= unitb->factor) result=1; else result=0;}
  else return E_BADCOMPOPERATOR;
  freeunit(unita);
  freeunit(unitb);
  unita->factor = result;
  return 0;
}



double
linearinterp(double  a, double b, double aval, double bval, double c)
{
  double lambda;

  lambda = (b-c)/(b-a);
  return lambda*aval + (1-lambda)*bval;
}


/* evaluate a user function */

#define INVERSE 1
#define FUNCTION 0
#define ALLERR 1
#define NORMALERR 0



int
evalfunc(int param_count, struct unittype **theunitlist, struct func *infunc, int inverse,
         int allerrors)
{
   struct unittype result;
   struct functype *thefunc;
   int err, i;
   double value, direction;
   int foundit, count;
   struct unittype **save_value;
   char **save_function;
   int save_count;
   if (infunc->table) {  /* Tables are short, so use dumb search algorithm */
     if (param_count>1)
       return E_TOOMANYARGS;
     err = parseunit(&result, infunc->tableunit, 0, 0);
     if (err)
       return E_BADFUNCDIMEN;
     if (inverse){
       err = divunit(theunitlist[0], &result);
       if (err)
         return err;
       err = unit2num(theunitlist[0]);
       if (err==E_NOTANUMBER)
         return E_BADFUNCARG;
       if (err)
         return err;
       value = theunitlist[0]->factor;
       foundit=0;
       if (infunc->table[infunc->tablelen-1].value > infunc->table[0].value)
         direction = 1;
       else
         direction = -1;
       if (direction*value >= direction*(infunc->table[infunc->tablelen-1].value)
           && direction*value < direction*(infunc->table[infunc->tablelen-1].value) + DBL_EPSILON)
         {
           value = infunc->table[infunc->tablelen-1].location;
           foundit=1;
         }
       if (direction*value <= direction*infunc->table[0].value
           && direction*value > direction*(infunc->table[0].value) - DBL_EPSILON)
         {
           value = infunc->table[0].location;
           foundit=1;
         }
       if (!foundit)
         for(count=0;count<infunc->tablelen-1;count++)
           if (direction*infunc->table[count].value <= direction*value &&
               direction*value <= direction * infunc->table[count+1].value){
             foundit=1;
             value  = linearinterp(infunc->table[count].value,
                                   infunc->table[count+1].value,
                                   infunc->table[count].location,
                                   infunc->table[count+1].location,
                                   value);
             break;
           }
       if (!foundit)
         return E_NOTINDOMAIN;
       freeunit(&result);
       freeunit(theunitlist[0]);
       theunitlist[0]->factor = value;
       return 0;
     } else {
       err=unit2num(theunitlist[0]);
       if (err)
         return err;
       value=theunitlist[0]->factor;
       foundit=0;
       if (value >= infunc->table[infunc->tablelen-1].location
           && value < infunc->table[infunc->tablelen-1].location+DBL_EPSILON)
         {
           value = infunc->table[infunc->tablelen-1].value;
           foundit=1;
         }
       if (value <= infunc->table[0].location
           && value > infunc->table[0].location-DBL_EPSILON)
         {
           value = infunc->table[0].value;
           foundit=1;
         }
       if (!foundit)
         for(count=0;count<infunc->tablelen-1;count++)
           if (infunc->table[count].location<=value &&
               value<=infunc->table[count+1].location){
             foundit=1;
             value =  linearinterp(infunc->table[count].location,
                                   infunc->table[count+1].location,
                                   infunc->table[count].value,
                                   infunc->table[count+1].value,
                                   value);
             break;
           }
       if (!foundit)
         return E_NOTINDOMAIN;
       result.factor *= value;
     }
   } else {  /* it's a function */
     if (inverse){
       thefunc=&(infunc->inverse);
       if (!thefunc->def)
         return E_NOINVERSE;
     }
     else
       thefunc=&(infunc->forward);
     if (thefunc->param_count > param_count)
       return E_TOOFEWARGS;
     if (thefunc->param_count < param_count)
       return E_TOOMANYARGS;
     for(i=0;i<param_count;i++){
       err = completereduce(theunitlist[i]);
       if (err)
         return err;
     }
     for(i=0;i<param_count;i++){
       if (thefunc->dimen[i]){
         err = parseunit(&result, thefunc->dimen[i], 0, 0);
         if (err)
           return E_BADFUNCDIMEN;
         err = completereduce(&result);
         if (err)
           return E_BADFUNCDIMEN;
         if (compareunits(&result, theunitlist[i], ignore_nothing))
           return E_BADFUNCARG;
         value = theunitlist[i]->factor/result.factor;
       } else
         value = theunitlist[i]->factor;
       if (value > thefunc->domain[i].max
           || (thefunc->domain[i].max_open && value == thefunc->domain[i].max))
         return E_NOTINDOMAIN;
       if (value < thefunc->domain[i].min
           || (thefunc->domain[i].min_open && value == thefunc->domain[i].min))
         return E_NOTINDOMAIN;
     }
     /* Save global function parameters in case we have been called from another function */
     save_value = parameter_value;
     save_function = function_parameter;
     save_count = function_parameter_count;
     parameter_value = theunitlist;
     function_parameter = thefunc->param;
     function_parameter_count = param_count;
     err = parseunit(&result, thefunc->def, 0,0);
     /* restore global function parameters */
     function_parameter = save_function;
     function_parameter_count = save_count;
     parameter_value = save_value;
     if (err && (allerrors == ALLERR || err==E_PARSEMEM || err==E_PRODOVERFLOW
                 || err==E_NOTROOT || err==E_BADFUNCTYPE))
       return err;
     if (err)
       return E_FUNARGDEF;
   }
   freeunit(theunitlist[0]);
   initializeunit(theunitlist[0]);
   multunit(theunitlist[0], &result);
   return 0;
}


/*
    append a formatted string to a buffer; first character of buffer
    should be '\0' on first call
*/
void
vbufprintf(char **buf, size_t *bufsize, char *fmt, ...)
{
    va_list args;
    size_t
        oldlen,                 /* length of current buffer contents */
        newlen,                 /* length of string to be added */
        buflen;                 /* length of new buffer */
    double growfactor = 1.5;
    static char *newbuf = NULL;
    char *oldbuf;

    oldlen = strlen(*buf);
    oldbuf = dupstr(*buf, "(vbufprintf)");

    /* get length of formatted string to be appended to buffer */
    va_start(args, fmt);
    newlen = vsnprintf(NULL, 0, fmt, args);
    va_end(args);

    buflen = newlen + 1;

    /* allocate a buffer for the new string */
    newbuf = (char *) mymalloc(buflen, "(vbufprintf)");

    /* expand main buffer if necessary */
    if (*bufsize < oldlen + newlen + 1) {
        *bufsize = (size_t) ((oldlen + newlen) * growfactor + 1);

        *buf = (char *) myrealloc(*buf, *bufsize, "(vbufprintf)");
    }

    /* generate the new formatted string */
    va_start(args, fmt);
    newlen = vsnprintf(newbuf, buflen, fmt, args);
    va_end(args);

    /* copy old and new strings to buffer */
    strcpy(*buf, oldbuf);
    strcat(*buf, newbuf);

    free(oldbuf);
    free(newbuf);
}


/*
    similar to showunit(), but it saves the string in a buffer rather
    than sending it to an output stream
*/
char *
buildunitstr(struct unittype *theunit)
{
    char **ptr;
    char *buf;
    int printedslash;
    int counter = 1;
    size_t bufsize;

    bufsize = 256;
    buf = (char *) mymalloc(bufsize, "(buildunitstr)");
    *buf = '\0';

    /* factor */
    vbufprintf(&buf, &bufsize, num_format.format, theunit->factor);

    /* numerator */
    for (ptr = theunit->numerator; *ptr; ptr++) {
        if (ptr > theunit->numerator && **ptr &&
          !strcmp(*ptr, *(ptr - 1)))
            counter++;
        else {
            if (counter > 1)
                vbufprintf(&buf, &bufsize, "%s%d", powerstring, counter);
            if (**ptr)
                vbufprintf(&buf, &bufsize, " %s", *ptr);
            counter = 1;
        }
    }
    if (counter > 1)
        vbufprintf(&buf, &bufsize, "%s%d", powerstring, counter);
    counter = 1;
    printedslash = 0;

    /* denominator */
    for (ptr = theunit->denominator; *ptr; ptr++) {
        if (ptr > theunit->denominator && **ptr &&
          !strcmp(*ptr, *(ptr - 1)))
            counter++;
        else {
            if (counter > 1)
                vbufprintf(&buf, &bufsize, "%s%d", powerstring, counter);
            if (**ptr) {
                if (!printedslash)
                    vbufprintf(&buf, &bufsize, " /");
                printedslash = 1;
                vbufprintf(&buf, &bufsize, " %s", *ptr);
            }
            counter = 1;
        }
    }
    if (counter > 1)
        vbufprintf(&buf, &bufsize, "%s%d", powerstring, counter);

   return buf;
}

/* compare strings, ignoring whitespace */
int
strwcmp(char *str1, char *str2)
{
    while (*str1 && *str2) {
        while (isspace(*str1))
            str1++;
        while (isspace(*str2))
            str2++;
        if (*str1 != *str2 || ! *str1 || ! *str2)
            break;
        str1++, str2++;
    }
    return *str1 - *str2;
}

/*
   If the given character string has only one unit name in it, then print out
   the rule for that unit.  In any case, print out the reduced form for
   the unit.
*/

void
showdefinition(char *unitstr, struct unittype *theunit)
{
  size_t bufsize = 256;
  char *buf, *showstr;

  logputs(deftext);
  showstr = buildunitstr(theunit);
  buf = (char *) mymalloc(bufsize, "(showdefinition)");
  *buf = '\0';

  while((unitstr = lookupunit(unitstr,1))
        && strspn(unitstr,digits) != strlen(unitstr)
        && !strchr(unitstr,PRIMITIVECHAR)) {
    if (strlen(unitstr) > bufsize - 1) {
        bufsize = strlen(unitstr) + 1;
        buf = realloc(buf, bufsize);
    }
    tightbufprint(buf, unitstr);
    logputs(buf);
    if (strwcmp(buf, showstr))
        logputs(" = ");
  }

  /* don't repeat a definition that differs only in whitespace */
  if (strwcmp(buf, showstr))
      logputs(showstr);

  logputchar('\n');
  free(buf);
  free(showstr);
}


void
showfunction(struct functype *func)
{
  struct unittype unit;
  int not_dimensionless, i, j;

  if (!func->def) {
    logputs(" is undefined");
    return;
  }
  logputchar('(');
  for(i=0;i<func->param_count;i++){
    logputs(func->param[i]);
    if (i<func->param_count-1)
      logputchar(',');
  }
  logprintf(") = %s", func->def);

  for(j=0;j<func->param_count;j++){
    if (func->dimen[j]){                       /* coverity[check_return] */
      parseunit(&unit,func->dimen[j],0,0);     /* unit2num returns 0 for */
      not_dimensionless = unit2num(&unit);     /* dimensionless units    */
    }
    if (isfinite(func->domain[j].min) || isfinite(func->domain[j].max)){
      logputchar('\n');
      for(i=strwidth(deftext);i;i--) logputchar(' ');
      logputs("defined for ");
      if (isfinite(func->domain[j].min) && isfinite(func->domain[j].max)) {
        logprintf(num_format.format, func->domain[j].min);
        if (func->dimen[j] && (not_dimensionless || unit.factor != 1)){
          if (isdecimal(*func->dimen[j]))
            logputs(" *");
          logprintf(" %s",func->dimen[j]);
        }
        logputs(func->domain[j].min_open?" < ":" <= ");
      }
      logputs(func->param[j]);
      if (isfinite(func->domain[j].max)){
        logputs(func->domain[j].max_open?" < ":" <= ");
        logprintf(num_format.format, func->domain[j].max);
      }
      else {
        logputs(func->domain[j].min_open?" > ":" >= ");
        logprintf(num_format.format, func->domain[j].min);
      }
      if (func->dimen[j] && (not_dimensionless || unit.factor != 1)){
        if (isdecimal(*func->dimen[j]))
          logputs(" *");
        logprintf(" %s",func->dimen[j]);
      }
      if (!func->dimen[j]) logputs(" (any units)");
    } else if (func->dimen[j]){
      logputchar('\n');
      for(i=strwidth(deftext);i;i--) logputchar(' ');
      if (not_dimensionless)
        logprintf("%s has units %s",func->param[j], func->dimen[j]);
      else
        logprintf("%s is dimensionless",func->param[j]);
    }
  }
  logputchar('\n');
}

void
showtable(struct func *fun, int inverse)
{
  int i;

  logprintf("%sinterpolated table with points\n",deftext);
  if (inverse){
    int reverse, j;
    reverse = (fun->table[0].value > fun->table[fun->tablelen-1].value);
    for(i=0;i<fun->tablelen;i++){
      if (reverse) j = fun->tablelen-i-1;
      else j=i;
      if (flags.verbose>0)
        logputs("\t\t    ");
      logprintf("~%s(", fun->name);
      logprintf(num_format.format, fun->table[j].value);
      if (isdecimal(fun->tableunit[0]))
        logputs(" *");
      logprintf(" %s",fun->tableunit);
      logputs(") = ");
      logprintf(num_format.format, fun->table[j].location);
      logputchar('\n');
    }
  } else {
    for(i=0;i<fun->tablelen;i++){
      if (flags.verbose>0)
        logputs("\t\t    ");
      logprintf("%s(", fun->name);
      logprintf(num_format.format, fun->table[i].location);
      logputs(") = ");
      logprintf(num_format.format, fun->table[i].value);
      if (isdecimal(fun->tableunit[0]))
        logputs(" *");
      logprintf(" %s\n",fun->tableunit);
    }
  }
}


void
showfuncdefinition(struct func *fun, int inverse)
{
  if (fun->table)  /* It's a table */
    showtable(fun, inverse);
  else {
    logprintf("%s%s%s", deftext,inverse?"~":"", fun->name);
    if (inverse)
      showfunction(&fun->inverse);
    else
      showfunction(&fun->forward);
  }
}


void
showunitlistdef(struct wantalias *alias)
{
  logprintf("%sunit list, ",deftext);
  tightprint(stdout,alias->definition);
  if (logfile) tightprint(logfile,alias->definition);
  logputchar('\n');
}


/* Show conversion to a function.  Input unit 'have' is replaced by the
   function inverse and completely reduced. */

int
showfunc(char *havestr, struct unittype *have, struct func *fun)
{
   int err;
   char *dimen;

   err = evalfunc(1, &have, fun, INVERSE, NORMALERR);
   if (!err)
     err = completereduce(have);
   if (err) {
     if (err==E_BADFUNCARG){
       logputs("conformability error");
       if (fun->table)
         dimen = fun->tableunit;
       else
         dimen=fun->inverse.dimen[0];
       if (!dimen)
         logputchar('\n');
       else {
         struct unittype want;

         if (emptystr(dimen))
           dimen = "1";
         logprintf(": conversion requires dimensions of '%s'\n",dimen);
         if (flags.verbose==2) logprintf("\t%s = ",havestr);
         else if (flags.verbose==1) logputchar('\t');
         showunit(have);
         if (flags.verbose==2) logprintf("\n\t%s = ",dimen);
         else if (flags.verbose==1) logprintf("\n\t");
         else logputchar('\n');            /* coverity[check_return] */
         parseunit(&want, dimen, 0, 0);    /* coverity[check_return] */
         completereduce(&want);         /* dimen was already checked for */
         showunit(&want);               /* errors so no need to check here */
         logputchar('\n');
         }
     } else if (err==E_NOTINDOMAIN)
       logprintf("Value '%s' is not in the function's range\n",havestr);
     else if (err==E_NOINVERSE)
       logprintf("Inverse of the function '%s' is not defined\n",fun->name);
     else
       logputs("Function evaluation error (bad function definition)\n");
     return 1;
   }
   if (flags.verbose==2)
     logprintf("\t%s = %s(", havestr, fun->name);
   else if (flags.verbose==1)
     logputchar('\t');
   showunit(have);
   if (flags.verbose==2)
     logputchar(')');
   logputchar('\n');
   return 0;
}

/* Print the conformability error message */

void
showconformabilityerr(char *havestr,struct unittype *have,
                      char *wantstr,struct unittype *want)
{
  logputs("conformability error\n");
  if (flags.verbose==2)
    logprintf("\t%s = ",havestr);
  else if (flags.verbose==1)
    logputchar('\t');
  showunit(have);
  if (flags.verbose==2)
    logprintf("\n\t%s = ",wantstr);
  else if (flags.verbose==1)
    logputs("\n\t");
  else
    logputchar('\n');
  showunit(want);
  logputchar('\n');
} /* end showconformabilityerr */



/*
  determine whether a unit string begins with a fraction; assume it
  does if it starts with an integer, '|', and another integer
*/
int
isfract(const char *unitstr)
{
  char *enddouble = NULL, *endlong = NULL;

  while (isdigit(*unitstr))
    unitstr++;
  if (*unitstr++ == '|') {
    (void)strtod(unitstr, &enddouble);
    (void)strtol(unitstr, &endlong, 10);
    if (enddouble == endlong)
      return 1;
  }
  return 0;
}

int
checksigdigits(char *arg)
{
  int errors, ival;
  char *nonum;

  errors = 0;

  if (!strcmp(arg, "max"))
    num_format.precision = MAXPRECISION;
  else {
    ival = (int) strtol(arg, &nonum, 10);
    if (!emptystr(nonum)) {
      fprintf(stderr,
      "%s: invalid significant digits (%s)--integer value or 'max' required\n",
              progname, arg);
      errors++;
    }
    else if (ival <= 0) {
      fprintf(stderr, "%s: number of significant digits must be positive\n",
              progname);
      errors++;
    }
    else if (strchr("EeGg", num_format.type) && ival > MAXPRECISION) {
      fprintf(stderr,
           "%s: too many significant digits (%d) for '%c' format--using maximum value (%d)\n",
           progname, ival, num_format.type, MAXPRECISION);
      num_format.precision = MAXPRECISION;
    }
    else
      num_format.precision = ival;
  }
  if (errors)
    return -1;
  else
    return 0;
}

/*
    Set output number format specification from flags, width, precision,
    and type; here, "precision" is the same as for printf(3).
*/

int
setnumformat()
{
    size_t buflen;

    if (num_format.format)
        free(num_format.format);
    if (num_format.width > 0)
        buflen = snprintf(NULL, 0, "%%%s%d.%d%c", num_format.flags,
                       num_format.width, num_format.precision, num_format.type) + 1;
    else
        buflen = snprintf(NULL, 0, "%%%s.%d%c",
                       num_format.flags, num_format.precision, num_format.type) + 1;
    num_format.format = (char *) mymalloc(buflen, "(setnumformat)");
    if (num_format.width > 0)
        snprintf(num_format.format, buflen, "%%%s%d.%d%c", num_format.flags,
                num_format.width, num_format.precision, num_format.type);
    else
        snprintf(num_format.format, buflen, "%%%s.%d%c", num_format.flags,
                num_format.precision, num_format.type);

    return 0;
}

/*
    parse and validate the output number format specification and
    extract its type and precision into the num_format structure.
    Returns nonzero for invalid format.
*/

int
parsenumformat()
{
  static char *format_types = NULL;
  static char *format_flags = "+-# 0'";
  static char badflag;
  char *two = "0x1p+1";
  char *valid="ABCDEFGHIJKLMNOPQRSTUVWXYXabcdefghijklmnopqrstuvwxyx.01234567890";
  char *dotptr, *lptr, *nonum, *p0, *p;
  char testbuf[80];
  int errors, ndx;

  if (format_types == NULL){
    format_types = (char *) mymalloc(strlen(BASE_FORMATS)+4, "(parsenumformat)");
    strcpy(format_types,BASE_FORMATS);

    /* check for support of type 'F' */
    sprintf(testbuf, "%.1F", 1.2);
    if (strlen(testbuf) == 3 && testbuf[0] == '1' && testbuf[2] == '2')
      strcat(format_types,"F");

    /* check for support of hexadecimal floating point */
    sprintf(testbuf, "%.0a", 2.0);
    if (!strcmp(testbuf,two))
      strcat(format_types, "aA");

    /* check for support of digit-grouping (') flag */
    sprintf(testbuf, "%'.0f", 1234.0);
    if (strlen(testbuf) > 2 && testbuf[0] == '1' && testbuf[2] == '2')
      badflag = '\0'; /* supported */
    else
      badflag = '\''; /* not supported */
  }

  errors = 0;

  p = num_format.format;

  if (*p != '%') {
    fprintf(stderr, "%s: number format specification must start with '%%'\n",
            progname);
    errors++;
  }
  else if (strrchr(num_format.format, '%') != num_format.format) {
    fprintf(stderr, "%s: only one '%%' allowed in number format specification\n",
            progname);
    errors++;
    p++;
  }
  else
    p++;

  dotptr = strchr(num_format.format, '.');
  if (dotptr && strrchr(num_format.format, '.') != dotptr) {
    fprintf(stderr, "%s: only one '.' allowed in number format specification\n",
            progname);
    errors++;
  }

  p0 = p = num_format.format + 1;

  /* skip over flags */
  while (*p && strchr(format_flags, *p)) {
    if (*p == badflag) {   /* only if digit-grouping flag (') not supported */
      fprintf(stderr, "%s: digit-grouping flag (') not supported\n", progname);
      errors++;
    }
    p++;
  }
  /* save any flags */
  if (num_format.flags != NULL)
      free(num_format.flags);
  num_format.flags = dupnstr(p0, p - p0, "(parsenumformat)");

  /* check for type length modifiers, which aren't allowed */
  if ((lptr = strstr(num_format.format, "hh"))
     || (lptr = strstr(num_format.format, "ll"))) {
    fprintf(stderr, "%s: type length modifier (%.2s) not supported\n", progname, lptr);
    errors++;
  }
  else if ((lptr = strpbrk(num_format.format, "hjLltz"))) {
    fprintf(stderr, "%s: type length modifier (%c) not supported\n", progname, lptr[0]);
    errors++;
  }

  /* check for other invalid characters */
  ndx = strspn(p, valid);
  if (ndx < strlen(p)) {
    fprintf(stderr, "%s: invalid character (%c) in width, precision, or type\n",
    progname, p[ndx]);
    errors++;
  }

  if (errors) { /* results of any other checks are likely to be misleading */
    fprintf(stderr, "%s: invalid number format specification (%s)\n",
                    progname, num_format.format);
    fprintf(stderr, "%s: valid specification is %%[flags][width][.precision]type\n",
            progname);
    return -1;
  }

  /* get width and precision if specified; check precision */
  num_format.width = (int) strtol(p, &nonum, 10);

  if (*nonum == '.'){
    if (isdigit(nonum[1]))
      num_format.precision = (int) strtol(nonum+1, &nonum, 10);
    else {
      num_format.precision = 0;
      nonum++;
    }
  }
  else  /* precision not given--use printf() default */
    num_format.precision = 6;

  /* check for valid format type */
  if (emptystr(nonum)) {
    fprintf(stderr, "%s: missing format type\n", progname);
    errors++;
  }
  else {
    if (strchr(format_types, *nonum)) {
      if (nonum[1]) {
        fprintf(stderr, "%s: invalid character(s) (%s) after format type\n",
                progname, nonum + 1);
        errors++;
      }
      else
        num_format.type = *nonum;
    }
    else {
      fprintf(stderr,
              "%s: invalid format type (%c)--valid types are [%s]\n",
              progname, *nonum, format_types);
      errors++;
    }
  }

  if (num_format.precision == 0 &&
      (num_format.type == 'G' || num_format.type == 'g'))
    num_format.precision = 1;

  if (errors) {
    fprintf(stderr, "%s: invalid number format specification (%s)\n",
                    progname, num_format.format);
    fprintf(stderr, "%s: valid specification is %%[flags][width][.precision]type\n",
            progname);
    return -1;
  }
  else
    return 0;
}

#if 0
/*
   round a number to the lesser of the displayed precision or the
   remaining significant digits; indicate in hasnondigits if a number
   will contain any character other than the digits 0-9 in the current
   display format.
*/

double
round_output(double value, int sigdigits, int *hasnondigits)
{
  size_t buflen;
  char *buf;
  double rounded;
  double mult_factor, rdigits;
  int fmt_digits;       /* decimal significant digits in format */

  if (!isfinite(value)){
    if (hasnondigits)
      *hasnondigits = 1;
    return value;
  }

  fmt_digits = num_format.precision;
  switch (num_format.type) {
    case 'A':
    case 'a':
      sigdigits = (int) round(sigdigits * log2(10) / 4);
      fmt_digits++;
      break;
    case 'E':
    case 'e':
      fmt_digits++;
      break;
    case 'F':
    case 'f':
      if (fabs(value) > 0)
        fmt_digits += (int) ceil(log10(fabs(value)));
      break;
  }

  if (sigdigits < fmt_digits)
    rdigits = sigdigits;
  else
    rdigits = fmt_digits;

  /* place all significant digits to the right of the radix */
  if (value != 0)
    rdigits -= ceil(log10(fabs(value)));
  /* then handle like rounding to specified decimal places */
  mult_factor = pow(10.0, rdigits);
  rounded = round(value * mult_factor) / mult_factor;

  buflen = snprintf(NULL, 0, num_format.format, value) + 1;
  buf = (char *) mymalloc(buflen, "(round_output)");
  snprintf(buf, buflen, num_format.format, value);

  if (hasnondigits){
    if (strspn(buf, "1234567890") != strlen(buf))
      *hasnondigits = 1;
    else
      *hasnondigits = 0;
  }

  free(buf);
  return rounded;
}
#endif

/*
   Determine significant digits in remainder relative to an original
   value which is assumed to have full double precision.  Returns
   the number of binary or decimal digits depending on the value
   of base, which must be 2 or 10.
*/

int
getsigdigits(double original, double remainder, int base)
{
  int sigdigits;
  double maxdigits;
  double (*logfunc)(double);

  if (base == 2) {
    maxdigits = DBL_MANT_DIG;
    logfunc = log2;
  }
  else {
    maxdigits = DBL_MANT_DIG * log10(2.0);
    logfunc = log10;
  }

  if (original == 0)
    return (int) floor(maxdigits);
  else if (remainder == 0)
    return 0;

  sigdigits = (int) floor(maxdigits - logfunc(fabs(original/remainder)));

  if (sigdigits < 0)
    sigdigits = 0;

  return sigdigits;
}

/* Rounds a double to the specified number of binary or decimal
   digits.  The base argument must be 2 or 10.  */

double
round_digits(double value, int digits, int base)
{
  double mult_factor;
  double (*logfunc)(double);

  if (digits == 0)
    return 0.0;

  if (base == 2)
    logfunc = log2;
  else
    logfunc = log10;

  if (value != 0)
    digits -= (int) ceil(logfunc(fabs(value)));

  mult_factor = pow((double) base, digits);

  return round(value*mult_factor)/mult_factor;
}


/*
  Returns true if the value will display as equal to the
  reference.  If hasnondigits is non-null, set it to true if the
  displayed output contains any character in "0123456789".
*/
int
displays_as(double reference, double value, int *hasnondigits)
{
  size_t buflen;
  char *buf;
  double rounded;

  if (!isfinite(value)){
    if (hasnondigits)
      *hasnondigits = 1;
    return 0;
  }

  buflen = snprintf(NULL, 0, num_format.format, value) + 1;
  buf = (char *) mymalloc(buflen, "(displays_as)");
  snprintf(buf, buflen, num_format.format, value);

  if (hasnondigits){
    if (strspn(buf, "1234567890") != strlen(buf))
      *hasnondigits = 1;
    else
      *hasnondigits = 0;
  }
  rounded = strtod(buf, NULL);
  free(buf);

  return rounded==reference;
}



/* Print the unit in 'unitstr' along with any necessary punctuation.
   The 'value' is the multiplier for the unit.  If printnum is set
   to PRINTNUM then this value is printed, or set it to NOPRINTNUM
   to prevent the value from being printed.
*/

#define PRINTNUM 1
#define NOPRINTNUM 0

void
showunitname(double value, char *unitstr, int printnum)
{
  int hasnondigits;     /* flag to indicate nondigits in displayed value */
  int is_one;           /* Does the value display as 1? */

  is_one = displays_as(1, value, &hasnondigits);

  if (printnum && !(is_one && isdecimal(*unitstr)))
    logprintf(num_format.format, value);

  if (strpbrk(unitstr, "+-"))   /* show sums and differences of units */
    logprintf(" (%s)", unitstr);   /* in parens */
     /* fractional unit 1|x and multiplier is all digits and not one--   */
     /* no space or asterisk or numerator (3|8 in instead of 3 * 1|8 in) */
  else if (printnum && !flags.showfactor
           && startswith(unitstr,"1|") && isfract(unitstr)
           && !is_one && !hasnondigits)
    logputs(unitstr+1);
     /* multiplier is unity and unit begins with a number--no space or       */
     /* asterisk (multiplier was not shown, and the space was already output)*/
  else if (is_one && isdecimal(*unitstr))
    logputs(unitstr);
     /* unitstr begins with a non-fraction number and multiplier was */
     /* shown--prefix a spaced asterisk */
  else if (isdecimal(unitstr[0]))
    logprintf(" * %s", unitstr);
  else
    logprintf(" %s", unitstr);
}


/* Show the conversion factors or print the conformability error message */

int
showanswer(char *havestr,struct unittype *have,
           char *wantstr,struct unittype *want)
{
   struct unittype invhave;
   int doingrec;  /* reciprocal conversion? */
   char *right = NULL, *left = NULL;

   doingrec=0;
   if (compareunits(have, want, ignore_dimless)) {
        char **src,**dest;

        invhave.factor=1/have->factor;
        for(src=have->numerator,dest=invhave.denominator;*src;src++,dest++)
           *dest=*src;
        *dest = NULL;
        for(src=have->denominator,dest=invhave.numerator;*src;src++,dest++)
           *dest=*src;
        *dest = NULL;
        if (flags.strictconvert || compareunits(&invhave, want, ignore_dimless)){
          showconformabilityerr(havestr, have, wantstr, want);
          return -1;
        }
        if (flags.verbose>0)
          logputchar('\t');
        logputs("reciprocal conversion\n");
        have=&invhave;
        doingrec=1;
   }
   if (flags.verbose==2) {
     if (!doingrec)
       left=right="";
     else if (strchr(havestr,'/')) {
       left="1 / (";
       right=")";
     } else {
       left="1 / ";
       right="";
     }
   }

   /* Print the first line of output. */

   if (flags.verbose==2)
     logprintf("\t%s%s%s = ",left,havestr,right);
   else if (flags.verbose==1)
     logputs("\t* ");
   if (flags.verbose==2)
     showunitname(have->factor / want->factor, wantstr, PRINTNUM);
   else
     logprintf(num_format.format, have->factor / want->factor);

   /* Print the second line of output. */

   if (!flags.oneline){
     if (flags.verbose==2)
       logprintf("\n\t%s%s%s = (1 / ",left,havestr,right);
     else if (flags.verbose==1)
       logputs("\n\t/ ");
     else
       logputchar('\n');
     logprintf(num_format.format, want->factor / have->factor);
     if (flags.verbose==2) {
       logputchar(')');
       showunitname(0,wantstr, NOPRINTNUM);
     }
   }
   logputchar('\n');
   return 0;
}


/* Checks that the function definition has a valid inverse
   Prints a message to stdout if function has bad definition or
   invalid inverse.
*/

#define SIGN(x) ( (x) > 0.0 ?   1 :   \
                ( (x) < 0.0 ? (-1) :  \
                                0 ))


void
print_string_array(int len, char **str)
{
  int i;
  for(i=0;i<len;i++){
    if (i>0) putchar(',');
    printf("%s",str[i]);
  }
}



#define FREE_UNITARRAY for(j=0;j<count;j++) if(unitarray[j]) {freeunit(unitarray[j]);free(unitarray[j]);}

void
checkfunc(struct func *infunc, int verbose)
{
  struct unittype *unitarray[MAX_FUNC_PARAMS], testunit, theunit;
  struct prefixlist *prefix;
  int err, i, j, dimensionless, count;
  double direction;

  if (infunc->skip_error_check){
    if (verbose)
      printf("skipped function '%s'\n", infunc->name);
    return;
  }
  if (verbose)
    printf("doing function '%s'\n", infunc->name);
  if ((prefix=plookup(infunc->name))
      && strlen(prefix->name)==strlen(infunc->name))
    printf("Warning: '%s' defined as prefix and function\n",infunc->name);

  /* Check tables */

  if (infunc->table){
    /* Check for valid unit for the table */
    if (parseunit(&theunit, infunc->tableunit, 0, 0) ||
        completereduce(&theunit))
      printf("Table '%s' has invalid units '%s'\n",
             infunc->name, infunc->tableunit);
    freeunit(&theunit);

    /* Check for monotonicity which is needed for unique inverses */
    if (infunc->tablelen<=1){
      printf("Table '%s' has only one data point\n", infunc->name);
      return;
    }
    direction = SIGN(infunc->table[1].value -  infunc->table[0].value);
    for(i=2;i<infunc->tablelen;i++)
      if (SIGN(infunc->table[i].value-infunc->table[i-1].value) != direction){
        printf("Table '%s' lacks unique inverse around entry " ERRNUMFMT "\n",
               infunc->name, infunc->table[i].location);
        return;
      }
    return;
  }

  /* Check functions */

  count = infunc->forward.param_count;

  for(j=0;j<count;j++)
    unitarray[j] = (struct unittype *) mymalloc(sizeof(struct unittype),"(checkfunc)");

  dimensionless = 1;
  for(j=0;j<count;j++){
    /* Initialize the parameter with the specified dimensions if given, unity otherwise */
    struct interval *ival;
    if (infunc->forward.dimen[j]){
      dimensionless = 0;
      if (parseunit(unitarray[j], infunc->forward.dimen[j], 0, 0) ||
          completereduce(unitarray[j])){
        printf("Function '%s' has invalid units '%s'\n",
               infunc->name, infunc->forward.dimen[j]);
        FREE_UNITARRAY;
        return;
      }
    }
    else initializeunit(unitarray[j]);

    /* Compute a value that is inside the domain */
    ival = infunc->forward.domain+j;
    if (isfinite(ival->max) && isfinite(ival->min))
      unitarray[j]->factor *= (ival->max + ival->min)/2;
    else if (isfinite(ival->max))
      unitarray[j]->factor *= ival->max-1;
    else if (isfinite(ival->min))
      unitarray[j]->factor *= ival->min+1;
    else
      unitarray[j]->factor *= 7;   /* Arbitrary choice where we evaluate */
  }
  initializeunit(&testunit);  /* Will hold first parameter of test point */
  if (!dimensionless){  /* In this case evaluate at the test point */
    unitcopy(&testunit, unitarray[0]);  /* Save first parameter to check inverse */
    err = evalfunc(count, unitarray, infunc, FUNCTION, ALLERR);
    if (err) {
      printf("input: ");
      showunit(&testunit);
      printf("Error in definition %s() as '%s':\n",
             infunc->name, infunc->forward.def);
      printf("      %s\n",errormsg[err]);
      FREE_UNITARRAY;
      freeunit(&testunit);
      return;
    }        /* All input parameters can have any dimension */
  } else {   /* In this case evaluate a few powers of (k kg) to confirm */
#   define MAXPOWERTOCHECK 4
    struct unittype resultunit, arbunit;
    int errors[MAXPOWERTOCHECK], errcount=0;
    char *indent;
    char *testunitstr = "3 kg K";

    initializeunit(&resultunit);
    err = parseunit(&arbunit, testunitstr, 0, 0);
    if (err){
      printf("Unable to create test unit 'kg K'.  Something is wrong!\n");
      FREE_UNITARRAY;
      return;
    }
    for(i=0;i<MAXPOWERTOCHECK;i++){
      unitcopy(unitarray[0],&arbunit);
      expunit(unitarray[0], i);
      for(j=1;j<count;j++)
        unitcopy(unitarray[j],unitarray[0]);
      errors[i] = evalfunc(count, unitarray, infunc, FUNCTION, ALLERR);
      if (errors[i]) errcount++;
      else {
        unitcopy(&testunit, &arbunit);
        expunit(&testunit, i);
        unitcopy(&resultunit, unitarray[0]);
      }
    }
    freeunit(&arbunit);
    unitcopy(unitarray[0],&resultunit);   /* Result of calculation stored into unitarray */
    freeunit(&resultunit);             /* which matches behavior in the other case */

    if (!errors[0] && errcount==3) {
      printf("Warning: function '%s(",infunc->name);
      print_string_array(count,infunc->forward.param);
      printf(")' defined as '%s'\n", infunc->forward.def);
      printf("         appears to require dimensionless arguments; 'units' keyword not given\n");
      indent = "         ";
    }
    else if (errcount==MAXPOWERTOCHECK) {
      printf("Error or missing 'units' keyword in definion %s(",infunc->name);
      print_string_array(count,infunc->forward.param);
      printf(") as '%s'\n",infunc->forward.def);
      indent="      ";
    }
    else if (errcount){
      printf("Warning: function '%s(",infunc->name);
      print_string_array(count, infunc->forward.param);
      printf(")' defined as '%s'\n",
             infunc->forward.def);
      printf("         failed for some test inputs:\n");
      indent = "         ";
    }
    for(i=0;i<MAXPOWERTOCHECK;i++)
      if (errors[i]) {
        printf("%s%s(",indent,infunc->name);
          for(j=0;j<count;j++){
            if (j>0) putchar(',');
            printf("(%s)^%d",testunitstr,i);
          }
        printf("): %s\n", errormsg[errors[i]]);
      }
  }

  /* At this point unitarray[0] holds the result of evaluating the
     function at the test point.  The test point is stored in testunit */

  if (completereduce(unitarray[0])){
    printf("Definition %s(",infunc->name);
    print_string_array(count,infunc->forward.param);
    printf(") as '%s' is irreducible\n", infunc->forward.def);
    FREE_UNITARRAY;
    freeunit(&testunit);
    return;
  }
  if (!(infunc->inverse.def)){
    if (count==1)
      printf("Warning: no inverse for function '%s'\n", infunc->name);
    FREE_UNITARRAY;
    freeunit(&testunit);
    return;
  }
  err = evalfunc(1, unitarray, infunc, INVERSE, ALLERR);
  if (err){
    printf("Error in inverse ~%s(%s) as '%s':\n",
           infunc->name,infunc->inverse.param[0], infunc->inverse.def);
    printf("      %s\n",errormsg[err]);
    FREE_UNITARRAY;
    freeunit(&testunit);
    return;
  }
  divunit(unitarray[0], &testunit);
  if (unit2num(unitarray[0]) || fabs(unitarray[0]->factor-1)>1e-12)
    printf("Inverse is not the inverse for function '%s'\n", infunc->name);
  FREE_UNITARRAY;
  freeunit(&testunit);
}


struct namedef {
  char *name;
  char *def;
};

#define CONFORMABLE 1
#define TEXTMATCH 2
#define SHOWRUNTIME 3
#define SHOWALL 4

void
addtolist(struct unittype *have, char *searchstring, char *rname, char *name,
               char *def, struct namedef **list, int *listsize,
               int *maxnamelen, int *count, int searchtype)
{
  struct unittype want;
  int len = 0;
  int keepit = 0;

  if (searchtype==CONFORMABLE){
    if (!name)
      return;
    initializeunit(&want);
    if (!parseunit(&want, name,0,0) && !completereduce(&want))
      keepit = !compareunits(have,&want,ignore_dimless);
  } else if (searchtype == TEXTMATCH)
    keepit = (strstr(rname,searchstring) != NULL);
  else if (searchtype == SHOWRUNTIME)
      keepit = startswith(rname, "_");
  else if (searchtype == SHOWALL)
      keepit = 1;
  if (keepit){
    if (*count==*listsize){
      *listsize += 100;
      *list = (struct namedef *)
        realloc(*list,*listsize*sizeof(struct namedef));
      if (!*list){
        fprintf(stderr, "%s: memory allocation error (addtolist)\n",
                progname);
        exit(EXIT_FAILURE);
      }
    }
    (*list)[*count].name = rname;
    if (strchr(def, PRIMITIVECHAR))
      (*list)[*count].def = "<primitive unit>";
    else
      (*list)[*count].def = def;
    (*count)++;
    len = strwidth(rname);
    if (len>*maxnamelen)
          *maxnamelen = len;
  }
  if (searchtype == CONFORMABLE)
    freeunit(&want);
}


int
compnd(const void *a, const void *b)
{
  return strcmp(((struct namedef *)a)->name, ((struct namedef *)b)->name);
}


int
screensize()
{
   int lines = 20;      /* arbitrary but probably safe value */

#if defined (_WIN32) && defined (_MSC_VER)
   CONSOLE_SCREEN_BUFFER_INFO csbi;

   if (GetConsoleScreenBufferInfo(GetStdHandle(STD_OUTPUT_HANDLE), &csbi))
       lines = csbi.srWindow.Bottom - csbi.srWindow.Top + 1;
#endif

#ifdef HAVE_IOCTL
   struct winsize ws;
   int fd;
   fd = open("/dev/tty", O_RDWR);
   if (fd>=0 && ioctl(fd, TIOCGWINSZ, &ws)==0)
     lines = ws.ws_row;
#endif

   return lines;
}

/*
  determines whether the output will fit on the screen, and opens a
  pager if it won't
*/
FILE *
get_output_fp(int lines)
{
  FILE *fp = NULL;

  if (isatty(fileno(stdout)) && screensize() < lines) {
    if ((fp = popen(pager, "w")) == NULL) {
      fprintf(stderr, "%s: can't run pager '%s--'", progname, pager);
      perror((char*) NULL);
    }
  }
  if (!fp)
    fp = stdout;

  return fp;
}

int
countlines(char *msg)
{
  int nlines = 0;
  char *p;

  for (p = msg; *p; p++)
    if (*p == '\n')
      nlines++;

  return nlines;
}

/*
   If have is non-NULL then search through all units and print the ones
   which are conformable with have.  Otherwise search through all the
   units for ones whose names contain the second argument as a substring.
*/

void
tryallunits(struct unittype *have, char *searchstring)
{
  struct unitlist *uptr;
  struct namedef *list;
  int listsize, maxnamelen, count;
  struct func *funcptr;
  struct wantalias *aliasptr;
  int i, j, searchtype;
  FILE *outfile;
  char *seploc, *firstunit;

  list = (struct namedef *) mymalloc( 100 * sizeof(struct namedef),
                                       "(tryallunits)");
  listsize = 100;
  maxnamelen = 0;
  count = 0;

  if (have)
    searchtype = CONFORMABLE;
  else if (flags.listallunits)        /* command line only */
    searchtype = SHOWALL;
  else if (equal(searchstring, ALLUNITS))
    searchtype = SHOWALL;
  else if (equal(searchstring, RUNTIMEVAR))
    searchtype = SHOWRUNTIME;
  else {
    if (!searchstring)
      searchstring="";
    searchtype = TEXTMATCH;
  }

  for(i=0;i<HASHSIZE;i++)
    for (uptr = utab[i]; uptr; uptr = uptr->next)
      addtolist(have, searchstring, uptr->name, uptr->name, uptr->value,
                &list, &listsize, &maxnamelen, &count, searchtype);
  for(i=0;i<SIMPLEHASHSIZE;i++)
    for(funcptr=ftab[i];funcptr;funcptr=funcptr->next){
      if (funcptr->table)
        addtolist(have, searchstring, funcptr->name, funcptr->tableunit,
                "<piecewise linear>", &list, &listsize, &maxnamelen, &count,
                searchtype);
      else
        addtolist(have, searchstring, funcptr->name, funcptr->inverse.dimen[0],
                "<nonlinear>", &list, &listsize, &maxnamelen, &count,
                searchtype);
  }
  for(aliasptr=firstalias;aliasptr;aliasptr=aliasptr->next){
    firstunit = dupstr(aliasptr->definition, "(tryallunits)");/* coverity[var_assigned] */
    seploc = strchr(firstunit,UNITSEPCHAR);  /* Alias definitions allowed in */
    *seploc = '\0';                          /* database contain UNITSEPCHAR */
    addtolist(have, searchstring, aliasptr->name, firstunit,
              aliasptr->definition, &list, &listsize, &maxnamelen, &count,
              searchtype);
    free(firstunit);
  }

  qsort(list, count, sizeof(struct namedef), compnd);

  if (count==0) {
    if (searchtype == SHOWRUNTIME)
      puts("No runtime variables assigned.");
    else
      puts("No matching units found.");
  }
#ifdef SIGPIPE
  signal(SIGPIPE, SIG_IGN);
#endif
  /* assume a pager isn't wanted with --list-units option */
  if (flags.listallunits)
      outfile = stdout;
  /* see if we need a pager */
  else
      outfile = get_output_fp(count);
  for(i=0;i<count;i++){
    fputs(list[i].name,outfile);
    if (flags.verbose > 0 || flags.interactive) {
        for(j=strwidth(list[i].name);j<=maxnamelen;j++)
          putc(' ',outfile);
        tightprint(outfile,list[i].def);
    }
    fputc('\n',outfile);
  }
  if (outfile != stdout)
    pclose(outfile);
#ifdef SIGPIPE
  signal(SIGPIPE, SIG_DFL);
#endif
}


/* If quiet is false then prompt user with the query.

   Fetch one line of input and return it in *buffer.

   The bufsize argument is a dummy argument if we are using readline.
   The readline version frees buffer if it is non-null.  The other
   version keeps the same buffer and grows it as needed.

   If no data is read, then this function exits the program.
*/


void
getuser_noreadline(char **buffer, int *bufsize, const char *query)
{
#ifdef SUPPORT_UTF8
  int valid = 0;
  while(!valid){
    fputs(query, stdout);
    if (!fgetslong(buffer, bufsize, stdin,0)){
      if (!flags.quiet)
        putchar('\n');
      exit(EXIT_SUCCESS);
    }
    valid = strwidth(*buffer)>=0;
    if (!valid)
      printf("Error: %s\n",invalid_utf8);
  }
#else
  fputs(query, stdout);
  if (!fgetslong(buffer, bufsize, stdin,0)){
    if (!flags.quiet)
      putchar('\n');
    exit(EXIT_SUCCESS);
  }
#endif
}


#ifndef READLINE
#  define getuser getuser_noreadline
#else
           /* we DO have readline */
void
getuser_readline(char **buffer, int *bufsize, const char *query)
{
#ifdef SUPPORT_UTF8
  int valid = 0;
  while (!valid){
    if (*buffer) free(*buffer);
    *buffer = readline(query);
    if (*buffer)
      replacectrlchars(*buffer);
    if (!*buffer || strwidth(*buffer)>=0)
        valid=1;
    else
      printf("Error: %s\n",invalid_utf8);
  }
#else
    if (*buffer) free(*buffer);
    *buffer = readline(query);
    if (*buffer)
      replacectrlchars(*buffer);
#endif
  if (nonempty(*buffer)) add_history(*buffer);
  if (!*buffer){
    if (!flags.quiet)
       putchar('\n');
    exit(EXIT_SUCCESS);
  }
}


void
getuser(char **buffer, int *bufsize, const char *query)
{
  if (flags.readline)
    getuser_readline(buffer,bufsize,query);
  else
    getuser_noreadline(buffer,bufsize,query);
}


/* Unit name completion for readline.

   Complete function names or alias names or builtin functions.

   Complete to the end of a prefix or complete to the end of a unit.
   If the text includes a full prefix plus part of a unit and if the
   prefix is longer than one character then complete that compound.
   Don't complete a prefix fragment into prefix plus anything.
*/

#define CU_ALIAS 0
#define CU_BUILTIN 1
#define CU_FUNC 2
#define CU_PREFIX 3
#define CU_UNITS 4
#define CU_DONE 5

char *
completeunits(char *text, int state)
{
  static int uhash, fhash, phash, checktype;
  static struct prefixlist *curprefix, *unitprefix;
  static struct unitlist *curunit;
  static struct func *curfunc;
  static struct wantalias *curalias;
  static char **curbuiltin;
  char *output = NULL;

#ifndef NO_SUPPRESS_APPEND
  rl_completion_suppress_append = 1;
#endif

  if (!state){     /* state == 0 means this is the first call, so initialize */
    checktype = 0; /* start at first type */
    fhash = uhash = phash = 0;
    unitprefix = NULL; /* search for unit continuations starting with this prefix */
    curfunc=ftab[fhash];
    curunit=utab[uhash];
    curprefix=ptab[phash];
    curbuiltin = builtins;
    curalias = firstalias;
  }
  while (checktype != CU_DONE){
    if (checktype == CU_ALIAS){
      while(curalias){
        if (startswith(curalias->name,text))
          output = dupstr(curalias->name, "(completeunits)");
        curalias = curalias->next;
        if (output) return output;
      }
      checktype++;
    }
    if (checktype == CU_BUILTIN){
      while(*curbuiltin){
        if (startswith(*curbuiltin,text))
          output = dupstr(*curbuiltin, "(completeunits)");
        curbuiltin++;
        if (output) return output;
      }
      checktype++;
    }
    while (checktype == CU_FUNC){
      while (!curfunc && fhash<SIMPLEHASHSIZE-1){
        fhash++;
        curfunc = ftab[fhash];
      }
      if (!curfunc)
        checktype++;
      else {
        if (startswith(curfunc->name,text))
          output = dupstr(curfunc->name, "(completeunits)");
        curfunc = curfunc->next;
        if (output) return output;
      }
    }
    while (checktype == CU_PREFIX){
      while (!curprefix && phash<SIMPLEHASHSIZE-1){
        phash++;
        curprefix = ptab[phash];
      }
      if (!curprefix)
        checktype++;
      else {
        if (startswith(curprefix->name,text))
          output = dupstr(curprefix->name, "(completeunits)");
        curprefix = curprefix->next;
        if (output) return output;
      }
    }
    while (checktype == CU_UNITS){
      while (!curunit && uhash<HASHSIZE-1){
        uhash++;
        curunit = utab[uhash];
      }
      /* If we're done with the units go through them again with */
      /* the largest possible prefix stripped off */
      if (!curunit && !unitprefix
            && (unitprefix = plookup(text)) && strlen(unitprefix->name)>1){
        uhash = 0;
        curunit = utab[uhash];
      }
      if (!curunit) {
        checktype++;
        break;
      }
      if (unitprefix){
        if (startswith(curunit->name, text+unitprefix->len)){
          output = (char *)mymalloc(1+strlen(curunit->name)+unitprefix->len,
                                    "(completeunits)");
          strcpy(output, unitprefix->name);
          strcat(output, curunit->name);
        }
      }
      else if (startswith(curunit->name,text))
        output = dupstr(curunit->name, "(completeunits)");
      curunit=curunit->next;
      if (output)
        return output;
    }
  }
  return NULL;
}

#endif /* READLINE */



/* see if current directory contains an executable file */
int
checkcwd (char *file)
{
  FILE *fp;
  char *p;

  fp = openfile(file, "r");
  if (fp){
    fclose(fp);
    return 1;
  }
#ifdef _WIN32
  else if (!((p = strrchr(file, '.')) && isexe(p))) {
    char *pathname;

    pathname = mymalloc(strlen(file) + strlen(EXE_EXT) + 1,
                        "(checkcwd)");
    strcpy(pathname, file);
    strcat(pathname, EXE_EXT);
    fp = openfile(pathname, "r");
    free(pathname);
    if (fp) {
      fclose(fp);
      return 1;
    }
  }
#endif

  return 0;
}

/*
   return the last component of a pathname, and remove a .exe extension
   if one exists.
*/

char *
getprogramname(char *path)
{
  size_t proglen;
  char *p;

  path = pathend(path);

  /* get rid of filename extensions in Windows */
  proglen = strlen(path);

  if ((p = strrchr(path, '.')) && isexe(p))
    proglen -= 4;
  return dupnstr(path, proglen, "(getprogramname)");
}

/*
   Find the directory that contains the invoked executable.
*/

char *
getprogdir(char *progname, char **fullprogname)
{
  char *progdir = NULL;
  char *p;

#if defined (_WIN32) && defined (_MSC_VER)
  char buf[FILENAME_MAX + 1];

  /* get the full pathname of the current executable and be done with it */
  /* TODO: is there way to do this with gcc? */

  if (GetModuleFileName(NULL, buf, FILENAME_MAX + 1))
    progdir = dupstr(buf, "(getprogdir)");
#endif

  /* If path name is absolute or includes more than one component use it */

  if (!progdir && (isfullpath(progname) || hasdirsep(progname)))
    progdir = dupstr(progname, "(getprogdir)");


  /*
  command.com and cmd.exe under Windows always run a program that's in the
  current directory whether or not the current directory is in PATH, so we need
  to check the current directory.

  This could return a false positive if units is run from a Unix-like command
  interpreter under Windows if the current directory is not in PATH but
  contains 'units' or 'units.exe'
  */
#if defined (_WIN32) && !defined (_MSC_VER)
  if (!progdir && checkcwd(progname))
      progdir = dupstr(progname, "(getprogdir)");
#endif

  /* search PATH to find the executable */
  if (!progdir) {
    char *env;
    env = getenv("PATH");
    if (env) {
      /* search PATH */
      char *direc, *direc_end, *pathname;
      int len;
      FILE *fp;

      pathname = mymalloc(strlen(env)+strlen(progname)+strlen(EXE_EXT)+2,
                                                         "(getprogdir)");
      direc = env;
      while (direc) {
        direc_end = strchr(direc,PATHSEP);
        if (!direc_end)
          len = strlen(direc);
        else
          len = direc_end-direc;
        strncpy(pathname, direc, len);
        if (len>0)
          pathname[len++]=DIRSEP;
        strcpy(pathname+len, progname);
        fp = openfile(pathname, "r");
        if (fp){
          progdir = dupstr(pathname, "(getprogdir)");
          break;
        }
#ifdef _WIN32
        /*
          executable may or may not have '.exe' suffix, so we need to
          look for both
        */
        if (!((p = strrchr(pathname, '.')) && isexe(p))) {
          strcat(pathname, EXE_EXT);
          fp = openfile(pathname, "r");
          if (fp){
            progdir = dupstr(pathname, "(getprogdir)");
            break;
          }
        }
#endif
        direc = direc_end;
        if (direc) direc++;
      }
      free(pathname);
      if (fp)
        fclose(fp);
    }
  }

  if (!progdir) {
    fprintf(stderr, "%s: cannot find program directory\n", progname);
    exit(EXIT_FAILURE);
  }

  *fullprogname = dupstr(progdir, "(getprogdir)");      /* used by printversion() */
  p = pathend(progdir);
  *p = '\0';

  return progdir;
}

/*
  find a possible data directory relative to a 'bin' directory that
  contains the executable
*/

char *
getdatadir()
{
    int progdirlen;
    char *p;

    if (isfullpath(DATADIR))
      return DATADIR;
    if (!progdir || emptystr(DATADIR))
      return NULL;
    progdirlen = strlen(progdir);
    datadir = (char *) mymalloc(progdirlen + strlen(DATADIR) + 2,
                                "(getdatadir)");
    strcpy(datadir, progdir);
    if (isdirsep(progdir[progdirlen - 1]))
      datadir[progdirlen - 1] = '\0';   /* so pathend() can work */
    p = pathend(datadir);
    if ((strlen(p) == 3) && (tolower(p[0]) == 'b') \
       && (tolower(p[1]) == 'i') && (tolower(p[2]) == 'n')) {
      p = DATADIR;
      while (*p == '.')         /* ignore "./", "../" */
        p++;
      if (isdirsep(*p))
        p++;
      strcpy(pathend(datadir), p);

      return datadir;
    }
    else
      return NULL;
}

void
showfilecheck(int errnum, char *filename)
{
  if (errnum==ENOENT)
    printf("  Checking %s\n", filename);
  else
    printf("  Checking %s: %s\n", filename, strerror(errnum));
}

/*
  On Windows, find the locale map

  If print is set to ERRMSG then display error message if the file is
  not valid.  If print is set to SHOWFILES then display files that are
  checked (when the filename is not a fully specified path).  If print
  is set to NOERRMSG then do not display anything.

  Returns filename of valid local map file or NULL if no file was found.
*/
#ifdef _WIN32
char *
findlocalemap(int print)
{
  FILE *map = NULL;
  char *filename = NULL;
  char *file;

  /*
      Try the environment variable UNITSLOCALEMAP, then the #defined
      value LOCALEMAP, then the directory containing the units
      executable, then the directory given by datadir, and finally
      the directory containing the units data file.
  */

  /* check the environment variable UNITSLOCALEMAP */
  file = getenv("UNITSLOCALEMAP");
  if (nonempty(file)) {
    map = openfile(file,"rt");
    if (!map) {
      if (print == ERRMSG) {
        fprintf(stderr,
                "%s: cannot open locale map '%s'\n"
                "        specified in UNITSLOCALEMAP environment variable. ",
                progname, file);
        perror((char *) NULL);
      }
      return NULL;
    }
    else
      filename = dupstr(file, "(findlocalemap)");
  }

  /* check the #defined value LOCALEMAP */
  if (!map) {
    file = LOCALEMAP;
    map = openfile(file,"rt");
    if (map)
      filename = dupstr(file, "(findlocalemap)");
  }

  if (!map && !progdir) {
    if (print == ERRMSG) {
      fprintf(stderr,
              "%s: cannot find locale map--program directory not set\n",
              progname);
      exit(EXIT_FAILURE);
    }
    else
      return NULL;
  }

  /* check the directory with the units executable */
  if (!map && progdir) {
    filename = (char *) mymalloc(strlen(progdir) + strlen(file) + 2,
               "(findlocalemap)");
    strcpy(filename, progdir);
    strcat(filename, file);
    map = openfile(filename,"rt");
    if (print==SHOWFILES && !map)
      showfilecheck(errno, filename);
  }

  /* check data directory */
  if (!map && datadir) {
    if (filename)
      free(filename);
    filename = (char *) mymalloc(strlen(datadir) + strlen(file) + 3,
                                 "(findlocalemap)");
    strcpy(filename, datadir);
    strcat(filename, DIRSEPSTR);
    strcat(filename, file);
    map = openfile(filename, "rt");
    if (print==SHOWFILES && !map)
      showfilecheck(errno, filename);
  }

  /* check the directory with the units data file  */
  if (!map && unitsfiles[0]) {
    char *lastfilename = NULL;

    if (filename) {
      if (datadir)
        lastfilename = dupstr(filename, "(findlocalemap)");
      free(filename);
    }
    filename = (char *) mymalloc(strlen(unitsfiles[0]) + strlen(file) + 2,
               "(findlocalemap)");
    strcpy(filename, unitsfiles[0]);
    strcpy(pathend(filename), file);

    /* don't bother if we've just checked for it */
    if (lastfilename && strcmp(filename, lastfilename)) {
      map = openfile(filename,"rt");
      if (print==SHOWFILES && !map)
        showfilecheck(errno, filename);
    }
    if (lastfilename)
      free(lastfilename);
  }

  if (map) {
    fclose(map);
    return filename;
  }
  else {
    if (filename)
      free(filename);
    return NULL;
  }
}
#endif

/*
  Find the units database file.

  If print is set to ERRMSG then display error message if the file is
  not valid.  If print is set to SHOWFILES then display files that are
  checked (when the filename is not a fully specified path).  If print
  is set to NOERRMSG then do not display anything.

  Returns filename of valid database file or NULL if no file
  was found.
*/

char *
findunitsfile(int print)
{
  FILE *testfile = NULL;
  char *file;

  file = getenv("UNITSFILE");
  if (nonempty(file)) {
    testfile = openfile(file, "rt");
    if (!testfile) {
      if (print==ERRMSG) {
        fprintf(stderr,
                "%s: cannot open units file '%s' in environment variable UNITSFILE.  ",
                progname, file);
        perror((char *) NULL);
      }
      return NULL;
    }
  }

  if (!testfile && isfullpath(UNITSFILE)){
    file = UNITSFILE;
    testfile = openfile(file, "rt");
    if (!testfile) {
      if (print==ERRMSG) {
        fprintf(stderr,
                "%s: cannot open units data file '%s'.  ", progname, UNITSFILE);
        perror((char *) NULL);
      }
      return NULL;
    }
  }

  if (!testfile && !progdir) {
    if (print==ERRMSG) {
      fprintf(stderr,
              "%s: cannot open units file '%s' and cannot find program directory.\n", progname, UNITSFILE);
      perror((char *) NULL);
    }
    return NULL;
  }

  if (!testfile) {
    /* check the directory containing the units executable */
    file = (char *) mymalloc(strlen(progdir)+strlen(UNITSFILE)+1,
                             "(findunitsfile)");
    strcpy(file, progdir);
    strcat(file, UNITSFILE);
    testfile = openfile(file, "rt");
    if (print==SHOWFILES && !testfile)
      showfilecheck(errno, file);
    if (!testfile)
      free(file);
  }

  /* check data directory */
  if (!testfile && datadir) {
    file = (char *) mymalloc(strlen(datadir) + strlen(UNITSFILE) + 2,
                             "(findunitsfile)");
    strcpy(file, datadir);
    strcat(file, DIRSEPSTR);
    strcat(file, UNITSFILE);
    testfile = openfile(file, "rt");
    if (print==SHOWFILES && !testfile)
      showfilecheck(errno, file);
    if (!testfile)
      free(file);
  }

  if (!testfile) {
    if (print==ERRMSG)
      fprintf(stderr,"%s: cannot find units file '%s'\n", progname, UNITSFILE);
     return NULL;
  }
  else {
    fclose(testfile);
    return file;
  }
}

/*
  Find the user's home directory.  Unlike *nix, Windows doesn't usually
  set HOME, so we check several other places as well.
*/
char *
findhome(char **errmsg)
{
  struct stat statbuf;
  int allocated = 0;
  char *homedir;
  char notfound[] = "Specified home directory '%s' does not exist";
  char notadir[] = "Specified home directory '%s' is not a directory";

  /*
    Under UNIX just check HOME.  Under Windows, if HOME is not set then
    check HOMEDRIVE:HOMEPATH and finally USERPROFILE
  */
  homedir = getenv("HOME");

#ifdef _WIN32
  if (!nonempty(homedir)) {
    /* try a few other places */
    /* try HOMEDRIVE and HOMEPATH */
    char *homedrive, *homepath;
    if ((homedrive = getenv("HOMEDRIVE")) && *homedrive && (homepath = getenv("HOMEPATH")) && *homepath) {
      homedir = mymalloc(strlen(homedrive)+strlen(homepath)+1,"(personalfile)");
      allocated = 1;
      strcpy(homedir, homedrive);
      strcat(homedir, homepath);
    }
    else
      /* finally, try USERPROFILE */
      homedir = getenv("USERPROFILE");
  }
#endif

  /*
    If a home directory is specified, see if it exists and is a
    directory.  If not set error message text.
  */

  if (nonempty(homedir)) {
    if (stat(homedir, &statbuf) != 0) {
      *errmsg = malloc(strlen(notfound)+strlen(homedir));
      sprintf(*errmsg, notfound, homedir);
    }
    else if (!(statbuf.st_mode & S_IFDIR)) {
      *errmsg = malloc(strlen(notadir)+strlen(homedir));
      sprintf(*errmsg, notadir, homedir);
    }
    if (!allocated)
      homedir = dupstr(homedir, "(findhome)");
    return homedir;
  }
  else {
    *errmsg = "no home directory";
    return NULL;
  }
}

/*
  Find a personal file.  First checks the specified environment variable
  (envname) for the filename to  use.  If this is unset then search user's
  home directory for basename.  If there is no home directory, returns
  NULL.  Otherwise if the file exists then returns its name in newly allocated
  space and sets *exists to 1. If the file does not exist then sets *exist to
  zero and:
      With checkonly == 0, prints error message and returns NULL
      With checkonly != 0, returns filename (does not print error message)
*/
char *
personalfile(const char *envname, const char *basename,
             int checkonly, int *exists)
{
  FILE *testfile;
  char *filename=NULL;

  *exists = 0;

  /* First check the specified environment variable for a file name */
  if (envname)
    filename = getenv(envname);
  if (nonempty(filename)){
    testfile = openfile(filename, "rt");
    if (testfile){
      fclose(testfile);
      *exists = 1;
      return filename;
    }
    if (checkonly)
      return filename;
    else {
      fprintf(stderr, "%s: cannot open file '%s' specified in %s environment variable: ",
              progname, filename, envname);
      perror((char *) NULL);
      return NULL;
    }
  }
  /* Environment variable not set: look in home directory */
  else if (nonempty(homedir)) {
    filename = mymalloc(strlen(homedir)+strlen(basename)+2,
                        "(personalfile)");
    strcpy(filename,homedir);

    if (strcmp(homedir, "/") && strcmp(homedir, "\\"))
      strcat(filename,DIRSEPSTR);
    strcat(filename,basename);

    testfile = openfile(filename, "rt");
    if (testfile){
      fclose(testfile);
      *exists = 1;
      return filename;
    }
    if (checkonly)
      return filename;
    else {
      if (errno==EACCES || errno==EISDIR) {
        fprintf(stderr,"%s: cannot open file '%s': ",progname,filename);
        perror(NULL);
      }
      free(filename);
      return NULL;
    }
  }
  else
    return NULL;
}


/* print usage message */

void
usage()
{
  int nlines;
  char *unitsfile;
  char *msg = "\nUsage: %s [options] ['from-unit' 'to-unit']\n\n\
Options:\n\
    -h, --help           show this help and exit\n\
        --list-units     show a list of all units and exit\n\
    -c, --check          check that all units reduce to primitive units\n\
        --check-verbose  like --check, but lists units as they are checked\n\
        --verbose-check    so you can find units that cause endless loops\n\
    -d, --digits         show output to specified number of digits (default: %d)\n\
    -e, --exponential    exponential format output\n\
    -f, --file           specify a units data file (-f '' loads default file)\n"
#ifdef READLINE
"\
    -H, --history        specify readline history file (-H '' disables history)\n"
#endif
"\
    -L, --log            specify a file to log conversions\n\
    -l, --locale         specify a desired locale\n\
    -m, --minus          make - into a subtraction operator (default)\n\
        --oldstar        use old '*' precedence, higher than '/'\n\
        --newstar        use new '*' precedence, equal to '/'\n\
    -n, --nolists        disable conversion to unit lists\n\
    -S, --show-factor    show non-unity factor before 1|x in multi-unit output\n\
        --conformable    in non-interactive mode, show all conformable units\n\
    -o, --output-format  specify printf numeric output format (default: %%.%d%c)\n\
    -p, --product        make '-' into a product operator\n\
    -q, --quiet          suppress prompting\n\
        --silent         same as --quiet\n\
    -s, --strict         suppress reciprocal unit conversion (e.g., Hz<->s)\n\
    -v, --verbose        show slightly more verbose output\n\
        --compact        suppress printing of tab, '*', and '/' character\n\
    -1, --one-line       suppress the second line of output\n\
    -t, --terse          terse output (--strict --compact --quiet --one-line)\n\
    -r, --round          round last element of unit list output to an integer\n\
    -U, --unitsfile      show units data filename and exit\n\
    -u, --units          specify a CGS units system or natural units system:\n\
                            gauss[ian],esu,emu,hlu,natural,natural-gauss,\n\
                            hartree,planck,planck-red,si\n\
    -V, --version        show version, data filenames (with -t: version only)\n\
    -I, --info           show version, files, and program properties\n";
  FILE *fp = NULL;

  unitsfile = findunitsfile(NOERRMSG);

  nlines = countlines(msg);
  /* see if we need a pager */
  fp = get_output_fp(nlines + 4);

  fprintf(fp, msg, progname, DEFAULTPRECISION, DEFAULTPRECISION, DEFAULTTYPE);
  if (!unitsfile)
    fprintf(fp, "Units data file '%s' not found.\n\n", UNITSFILE);
  else
    fprintf(fp, "\nTo learn about the available units look in '%s'\n\n", unitsfile);
  fputs("Report bugs to adrianm@gnu.org.\n\n", fp);

  if (fp != stdout)
    pclose(fp);
}

/* Print message about how to get help */

void
helpmsg()
{
  fprintf(stderr,"\nTry '%s --help' for more information.\n",progname);
  exit(EXIT_FAILURE);
}

/* show units version, and optionally, additional information */
void
printversion()
{
  int exists;
  char *u_unitsfile = NULL;  /* units data file specified in UNITSFILE */
  char *m_unitsfile;         /* personal units data file from HOME_UNITS_ENV */
  char *p_unitsfile;         /* personal units data file */
#ifdef READLINE
  FILE *histfile;
#endif
#ifdef _WIN32
  char *localemap;
  void remaplocale(char *);
#endif

  if (flags.verbose == 0) {
    printf("GNU Units version %s\n", VERSION);
    return;
  }

#ifdef _WIN32

  printf("GNU Units version %s\n%s, %s%s, locale %s",
         VERSION, RVERSTR, UTF8VERSTR, utf8mode ? "" : " (disabled)", mylocale);
  localemap = findlocalemap(0);         /* save error message for later */
  if (localemap) {
    remaplocale(localemap);
    printf(" (%s)", mylocale);
  }
  putchar('\n');

#  ifdef HAVE_MKS_TOOLKIT
  puts("With MKS Toolkit");
#  endif

  if (! utf8mode) {
    printf("To enable UTF-8: ");
    if (! getenv(WINDOWS_TERMINAL))
      printf("run in Windows Terminal and ");
    printf("set code page to 65001\n");
  }
#else
  printf("GNU Units version %s\n%s, %s%s, locale %s\n",
         VERSION, RVERSTR, UTF8VERSTR, utf8mode ? "" : " (disabled)", mylocale);
#endif


  if (flags.verbose == 2) {
    if (!fullprogname)
      getprogdir(progname, &fullprogname);
    if (fullprogname)
      printf("\n%s program is %s\n", progname, fullprogname);
  }

  /* units data file */

  putchar('\n');
  if (isfullpath(UNITSFILE))
    printf("Default units data file is '%s'\n", UNITSFILE);
  else
    printf("Default units data file is '%s';\n"
           "  %s will search for this file\n",
           UNITSFILE, progname);
  if (flags.verbose < 2)
    printf("Default personal units file: %s\n", homeunitsfile);

  if (flags.verbose == 2){
    u_unitsfile = getenv("UNITSFILE");
    if (u_unitsfile)
      printf("Environment variable UNITSFILE set to '%s'\n", u_unitsfile);
    else
      puts("Environment variable UNITSFILE not set");

    unitsfiles[0] = findunitsfile(SHOWFILES);

    if (unitsfiles[0]) {
      /* We searched for the file in program and data dirs */
      if (!isfullpath(UNITSFILE) && !nonempty(u_unitsfile))
        printf("Found data file '%s'\n", unitsfiles[0]);
      else
        printf("Units data file is '%s'\n", unitsfiles[0]);
    }
    else {
      if (errno && (nonempty(u_unitsfile) || isfullpath(UNITSFILE)))
        printf("*** Units data file invalid: %s ***\n",strerror(errno));
      else
        puts("*** Units data file not found ***");
    }
    if (homedir_error)
      printf("\n%s\n", homedir_error);
    else
      printf("\nHome directory is '%s'\n", homedir);
  }

  /* personal units data file: environment */
  if (flags.verbose == 2){
    m_unitsfile = getenv(HOME_UNITS_ENV);
    putchar('\n');
    if (m_unitsfile) {
      printf("Environment variable %s set to '%s'\n",
             HOME_UNITS_ENV,m_unitsfile);
    }
    else
      printf("Environment variable %s not set\n", HOME_UNITS_ENV);

    p_unitsfile = personalfile(HOME_UNITS_ENV, homeunitsfile, 1, &exists);
    if (p_unitsfile) {
      printf("Personal units data file is '%s'\n", p_unitsfile);
      if (!exists){
        if (homedir_error && !nonempty(m_unitsfile))
          printf("  (File invalid: %s)\n", homedir_error);
        else if (errno==ENOENT && !nonempty(m_unitsfile))
          puts("  (File does not exist)");
        else
          printf("  (File invalid: %s)\n",strerror(errno));
      }
    }
    else
      puts("Personal units data file not found: no home directory");
  }
#ifdef READLINE
  if (flags.verbose == 2) {
    historyfile = personalfile(NULL,HISTORY_FILE,1,&exists);
    if (historyfile){
      printf("\nDefault readline history file is '%s'\n", historyfile);
      histfile = openfile(historyfile,"r+");
      if (!histfile)
        printf("  (File invalid: %s)\n",
               homedir_error ? homedir_error : strerror(errno));
      else
        fclose(histfile);
    }
    else
      puts("\nReadline history file unusable: no home directory");
  }
#endif

#ifdef _WIN32
  /* locale map */
  if (flags.verbose == 2) {
    putchar('\n');
    localemap = getenv("UNITSLOCALEMAP");
    if (localemap)
      printf("Environment variable UNITSLOCALEMAP set to '%s'\n", localemap);
    else
      puts("Environment variable UNITSLOCALEMAP not set");

    if (isfullpath(LOCALEMAP))
      printf("Default locale map is '%s'\n", LOCALEMAP);
    else
      printf("Default locale map is '%s';\n"
             "  %s will search for this file\n",
             LOCALEMAP, progname);

    localemap = findlocalemap(SHOWFILES);
    if (localemap && !isfullpath(LOCALEMAP))
      printf("Found locale map '%s'\n", localemap);
    else if (localemap)
      printf("Locale map is '%s'\n", localemap);
    else
      puts("*** Locale map not found ***");
  }
#endif

  printf("\n%s\n\n", LICENSE);
}

void
showunitsfile()
{
  char *unitsfile;
  unitsfile = findunitsfile(NOERRMSG);
  if (unitsfile)
    printf("%s\n", unitsfile);
  else
    puts("Units data file not found");
}


char *shortoptions = "VIUu:vqechSstf:o:d:mnpr1l:L:8"
#ifdef READLINE
    "H:"
#endif
  ;

struct option longoptions[] = {
  {"list-units", no_argument, &flags.listallunits, 1},
  {"check", no_argument, &flags.unitcheck, 1},
  {"check-verbose", no_argument, &flags.unitcheck, 2},
  {"compact", no_argument, &flags.verbose, 0},
  {"digits", required_argument, 0, 'd'},
  {"exponential", no_argument, 0, 'e'},
  {"file", required_argument, 0, 'f'},
  {"help", no_argument, 0, 'h'},
#ifdef READLINE
  {"history", required_argument, 0, 'H'},
#endif
  {"info", no_argument, 0, 'I'},
  {"locale", required_argument, 0, 'l'},
  {"log", required_argument, 0, 'L'},
  {"minus", no_argument, &parserflags.minusminus, 1},
  {"newstar", no_argument, &parserflags.oldstar, 0},
  {"nolists", no_argument, 0, 'n'},
  {"oldstar", no_argument, &parserflags.oldstar, 1},
  {"one-line", no_argument, &flags.oneline, 1},
  {"output-format", required_argument, 0, 'o'},
  {"product", no_argument, &parserflags.minusminus, 0},
  {"quiet", no_argument, &flags.quiet, 1},
  {"round",no_argument, 0, 'r'},
  {"show-factor", no_argument, 0, 'S'},
  {"conformable", no_argument, &flags.showconformable, 1 },
  {"silent", no_argument, &flags.quiet, 1},
  {"strict",no_argument,&flags.strictconvert, 1},
  {"terse",no_argument, 0, 't'},
  {"unitsfile", no_argument, 0, 'U'},
  {"units", required_argument, 0, 'u'},
  {"verbose", no_argument, &flags.verbose, 2},
  {"verbose-check", no_argument, &flags.unitcheck, 2},
  {"version", no_argument, 0, 'V'},
  {0,0,0,0} };

/* Process the args.  Returns 1 if interactive mode is desired, and 0
   for command line operation.  If units appear on the command line
   they are returned in the from and to parameters. */

int
processargs(int argc, char **argv, char **from, char **to)
{
   extern char *optarg;
   extern int optind;
   int optchar, optindex;
   int ind;
   int doprintversion=0;
   char *unitsys = NULL, *digitsstr, *temp;

   digitsstr = NULL;
   while ( -1 !=
      (optchar =
         getopt_long(argc, argv,shortoptions,longoptions, &optindex ))) {
      switch (optchar) {
         case 'm':
            parserflags.minusminus = 1;
            break;
         case 'p':
            parserflags.minusminus = 0;
            break;
         case 't':
            flags.oneline = 1;
            flags.quiet = 1;
            flags.strictconvert = 1;
            flags.verbose = 0;
            break;

         /* numeric output format */
         case 'd':
            digitsstr = optarg;
            num_format.format = NULL;
            break;
         case 'e':
            num_format.type = 'e';
            num_format.format = NULL;
            break;
         case 'o':
            num_format.format = dupstr(optarg, "(processargs)");
            digitsstr = NULL;
            break;

         case 'c':
            flags.unitcheck = 1;
            break;
         case 'f':
            for(ind=0;unitsfiles[ind];ind++);
            if (ind==MAXFILES){
              fprintf(stderr, "At most %d -f specifications are allowed\n",
                      MAXFILES);
              exit(EXIT_FAILURE);
            }
            if (optarg && *optarg)
              unitsfiles[ind] = optarg;
            else {
              unitsfiles[ind] = findunitsfile(ERRMSG);
              if (!unitsfiles[ind])
                exit(EXIT_FAILURE);
            }
            unitsfiles[ind+1] = 0;
            break;
         case 'L':
            logfilename = optarg;
            break;
         case 'l':
            mylocale = optarg;
            break;
         case 'n':
            flags.unitlists = 0;
            break;
         case 'q':
            flags.quiet = 1;
            break;
         case 'r':
            flags.round = 1;
            break;
         case 'S':
            flags.showfactor = 1;
            break;
         case 's':
            flags.strictconvert = 1;
            break;
         case 'v':
            flags.verbose = 2;
            break;
         case '1':
            flags.oneline = 1;
            break;
         case 'I':
            flags.verbose = 2;  /* fall through */
         case 'V':
            doprintversion = 1;
            break;
         case 'U':
            showunitsfile();
            exit(EXIT_SUCCESS);
            break;
         case 'u':
            unitsys = optarg;
            for(ind=0;unitsys[ind];ind++)
              unitsys[ind] = tolower(unitsys[ind]);
            break;
         case 'h':
            usage();
            exit(EXIT_SUCCESS);
#ifdef READLINE
         case 'H':
           if (emptystr(optarg))
              historyfile=NULL;
            else
              historyfile = optarg;
            break;
#endif
         case 0: break;  /* This is reached if a long option is
                            processed with no return value set. */
         case '?':        /* Invalid option or missing argument returns '?' */
         default:
            helpmsg();    /* helpmsg() exits with error */
      }
   }

   if (digitsstr && checksigdigits(digitsstr) < 0)
       exit(EXIT_FAILURE);

   temp = strchr(mylocale,'.');
   if (temp)
     *temp = '\0';

   if (doprintversion){
     printversion();
     exit(EXIT_SUCCESS);
   }

   /* command-line option overwrites environment */
   if (unitsys)
     setenv("UNITS_SYSTEM", unitsys, 1);

   if (flags.unitcheck) {
     if (optind != argc){
       fprintf(stderr,
               "Too many arguments (arguments are not allowed with -c).\n");
       helpmsg();         /* helpmsg() exits with error */
     }
   } else {
     if (optind == argc - 2) {
        if (flags.showconformable) {
          fprintf(stderr,"Too many arguments (only one unit expression allowed with '--conformable').\n");
          helpmsg();             /* helpmsg() exits with error */
        }
        flags.quiet=1;
        *from = argv[optind];
        *to = dupstr(argv[optind+1], "(processargs)"); /* This string may get rewritten later */
        return 0;                     /* and we might call free() on it      */
     }

     if (optind == argc - 1) {
        flags.quiet=1;
        *from = argv[optind];
        *to = NULL;
        return 0;
     }
     if (optind < argc - 2) {
        fprintf(stderr,"Too many arguments (maybe you need quotes).\n");
        helpmsg();             /* helpmsg() exits with error */
     }
   }

   return 1;
}

/*
   Show a pointer under the input to indicate a problem.
   Prints 'position' spaces and then the pointer.
   If 'position' is negative, nothing is printed.
 */

void
showpointer(int position)
{
  if (position >= 0){
    while (position--) putchar(' ');
    puts(POINTER);
  }
} /* end showpointer */


/*
   Process the string 'unitstr' as a unit, placing the processed data
   in the unit structure 'theunit'.  Returns 0 on success and 1 on
   failure.  If an error occurs an error message is printed to stdout.
   A pointer ('^') will be printed if an error is detected, and  promptlen
   should be set to the printing width of the prompt string, or set
   it to NOPOINT to suppress printing of the pointer.
 */


int
processunit(struct unittype *theunit, char *unitstr, int promptlen)
{
  char *errmsg;
  int errloc,err;
  char savechar;

  if (flags.unitlists && strchr(unitstr, UNITSEPCHAR)){
    puts("Unit list not allowed");
    return 1;
  }
  if ((err=parseunit(theunit, unitstr, &errmsg, &errloc))){
    if (promptlen >= 0){
      if (err!=E_UNKNOWNUNIT || !irreducible){
        if (errloc>0) {
          savechar = unitstr[errloc];
          unitstr[errloc] = 0;
          showpointer(promptlen+strwidth(unitstr)-1);
          unitstr[errloc] = savechar;
        }
        else showpointer(promptlen);
      }
    }
    else
      printf("Error in '%s': ", unitstr);
    fputs(errmsg,stdout);
    if (err==E_UNKNOWNUNIT && irreducible)
      printf(" '%s'", irreducible);
    putchar('\n');
    return 1;
  }
  if ((err=completereduce(theunit))){
    fputs(errormsg[err],stdout);
    if (err==E_UNKNOWNUNIT)
      printf(" '%s'", irreducible);
    putchar('\n');
    return 1;
  }
  return 0;
}


/* Checks for a new unit defined on the prompt with the form _<NAME> = <DEF> */

int
definevariable(char *def, int promptlen)
{
  int dummy, err;
  struct unittype unit;
  struct unitlist *uptr;
  int needfree=0;
  char *logstr, *reduced, *oldvalue = NULL;

  char *value = strchr(def,'=');
  if (!value)
    return 0;
  if (value>def && (value[-1]=='<' || value[-1]=='>'))
    return 0;
  if (value[1]=='=')
    return 0;
  logstr = dupstr(def, "(definevariable)");
  *value++ = '\0';
  if (processunit(&unit, value, promptlen + (value-def))) {
      free(logstr);
      return 1;
  }
  if (lastchar(def)==':'){
    char **entry;
    int spaceneeded, numeratorspace;
    int counter;
    lastchar(def) = '\0';
    needfree=1;
    spaceneeded = 30;           /* Space for leading number and a division sign */
    for(entry=unit.numerator;*entry;entry++)
      if (*entry!=NULLUNIT) spaceneeded+=1+strlen(*entry);
    numeratorspace = spaceneeded;
    for(entry=unit.denominator;*entry;entry++)
      if (*entry!=NULLUNIT) spaceneeded+=1+strlen(*entry);
    reduced = (char *)mymalloc(spaceneeded,"(definevariable)");
    snprintf(reduced,spaceneeded, "%.*g ",DBL_DECIMAL_DIG,unit.factor);
    for(entry=unit.numerator;*entry;entry++){
      if (*entry!=NULLUNIT){
        strcat(reduced, *entry);
        for(counter=1;entry[counter] && !strcmp(entry[counter],*entry);counter++);
        if (counter>1){
          strcat(reduced,powerstring);
          snprintf(reduced+strlen(reduced),spaceneeded-strlen(reduced),"%d",counter);
        }
        entry+=counter-1;
        strcat(reduced," ");
      }
    }
    if (numeratorspace<spaceneeded)
      strcat(reduced,"/ ");
    for(entry=unit.denominator;*entry;entry++){
      if (*entry!=NULLUNIT){
        strcat(reduced, *entry);
        for(counter=1;entry[counter] && !strcmp(entry[counter],*entry);counter++);
        if (counter>1){
          strcat(reduced,powerstring);
          snprintf(reduced+strlen(reduced),spaceneeded-strlen(reduced),"%d",counter);
        }
        entry+=counter-1;
        strcat(reduced," ");
      }
    }
    value=reduced;
    removespaces(def);
  }
  else {
    removespaces(def);
    /* see if this is a re-assignment */
    if ((uptr=ulookup(def)))
        /* save the current value so it can be restored if the re-assignment fails */
        oldvalue = dupstr(uptr->value, "(definevariable)");
  }
  removespaces(value);

  err = *def!='_' || newunit(def,value, &dummy, 0, 0, NULL, 1, 1);
  if (err)
    printf("Invalid variable name: %s\n", def);
  /* restore the current value if the assignment is invalid */
  else if (! needfree && processunit(&unit, def, NOPOINT)) {
    if (oldvalue && (uptr = ulookup(def))){
      free(uptr->value);
      uptr->value = dupstr(oldvalue, "(definevariable)");
    }
    err++;
  }
  if (! err && logfile)
      fprintf(logfile, "Var:  %s\n", logstr);
  free(logstr);
  if (oldvalue)
    free(oldvalue);
  if (needfree)
    free(reduced);
  return 1;
}


/*
   Checks the input parameter unitstr (a list of units separated by
   UNITSEPCHAR) for errors.  All units must be parseable and
   conformable to each other.  Returns 0 on success and 1 on failure.

   If an error is found then print an error message on stdout.  A
   pointer ('^') will be printed to mark the error.  The promptlen
   parameter should be set to the printing width of the prompt string
   so that the pointer is correctly aligned.

   To suppress the printing of the pointer set promptlen to NOPOINT.
   To suppress printing of error messages entirely set promptlen to
   NOERRMSG.
*/


int
checkunitlist(char *unitstr, int promptlen)
{
  struct unittype unit[2], one;
  char *firstunitstr,*nextunitstr;
  int unitidx = 0;

  int printerror = promptlen != NOERRMSG;

  initializeunit(&one);

  firstunitstr = unitstr;

  initializeunit(unit);
  initializeunit(unit+1);

  while (unitstr) {
    if ((nextunitstr = strchr(unitstr, UNITSEPCHAR)) != NULL)
      *nextunitstr = '\0';

    if (!unitstr[strspn(unitstr, " ")]) {  /* unitstr is blank */
      if (!nextunitstr) {  /* terminal UNITSEPCHAR indicates repetition */
        freeunit(unit);    /* of last unit and is permitted */
        return 0;
      }
      else {               /* internal blank units are not allowed */
        if (printerror){
          showpointer(promptlen);
          puts("Error: blank unit not allowed");
        }
        freeunit(unit);
        return 1;
      }
    }

    /* processunit() prints error messages; avoid it to suppress them */

    if ((printerror && processunit(unit+unitidx,unitstr,promptlen)) ||
        (!printerror &&
           (parseunit(unit+unitidx, unitstr,0,0)
             || completereduce(unit+unitidx)
             || compareunits(unit+unitidx,&one, ignore_primitive)))){
      if (printerror)
        printf("Error in unit list entry: %s\n",unitstr);
      freeunit(unit);
      freeunit(unit+1);
      return 1;
    }


    if (unitidx == 0)
      unitidx = 1;
    else {
      if (compareunits(unit, unit+1, ignore_dimless)){
        if (printerror){
          int wasverbose = flags.verbose;
          FILE *savelog = logfile;
          logfile = NULL;
          flags.verbose = 2;   /* always use verbose form to be unambiguous */
                               /* coverity[returned_null] */
          *(strchr(firstunitstr, UNITSEPCHAR)) = '\0';
          removespaces(firstunitstr);
          removespaces(unitstr);
          showpointer(promptlen);
          showconformabilityerr(firstunitstr, unit, unitstr, unit+1);
          flags.verbose = wasverbose;
          logfile = savelog;
        }
        freeunit(unit);
        freeunit(unit+1);
        return 1;
      }
      freeunit(unit+1);
    }

    if (nextunitstr) {
      if (promptlen >= 0) promptlen += strwidth(unitstr)+1;
      *(nextunitstr++) = UNITSEPCHAR;
    }
    unitstr = nextunitstr;
  }

  freeunit(unit);

  return 0;
} /* end checkunitlist */


/*
   Call either processunit or checkunitlist, depending on whether the
   string 'unitstr' contains a separator character.  Returns 0 on
   success and 1 on failure.  If an error occurs an error message is
   printed to stdout.

   A pointer will be printed if an error is detected, and  promptlen
   should be set to the printing width of the prompt string, or set
   it to NOPOINT to suppress printing of the pointer.
*/

int
processwant(struct unittype *theunit, char *unitstr, int promptlen)
{
  if (flags.unitlists && strchr(unitstr, UNITSEPCHAR))
    return checkunitlist(unitstr, promptlen);
  else
    return processunit(theunit, unitstr, promptlen);
}


void
checkallaliases(int verbose)
{
  struct wantalias *aliasptr;

  for(aliasptr = firstalias; aliasptr; aliasptr=aliasptr->next){
    if (verbose)
      printf("doing unit list '%s'\n", aliasptr->name);
    if (checkunitlist(aliasptr->definition,NOERRMSG))
      printf("Unit list '%s' contains errors\n", aliasptr->name);
    if (ulookup(aliasptr->name))
      printf("Unit list '%s' hides a unit definition.\n", aliasptr->name);
    if (fnlookup(aliasptr->name))
      printf("Unit list '%s' hides a function definition.\n", aliasptr->name);
  }
}



/*
   Check that all units and prefixes are reducible to primitive units and that
   function definitions are valid and have correct inverses.  A message is
   printed for every unit that does not reduce to primitive units.

*/


void
checkunits(int verbosecheck)
{
  struct unittype have,second,one;
  struct unitlist *uptr;
  struct prefixlist *pptr;
  struct func *funcptr;
  char *prefixbuf, *testunit;
  int i, reduce_err;
  char *err;

  initializeunit(&one);

  /* Check all functions for valid definition and correct inverse */

  for(i=0;i<SIMPLEHASHSIZE;i++)
    for(funcptr=ftab[i];funcptr;funcptr=funcptr->next)
      checkfunc(funcptr, verbosecheck);

  checkallaliases(verbosecheck);

  /* Now check all units for validity */

  for(i=0;i<HASHSIZE;i++)
    for (uptr = utab[i]; uptr; uptr = uptr->next){
      if (verbosecheck)
        printf("doing '%s'\n",uptr->name);
      if (strchr(uptr->value, PRIMITIVECHAR))
        continue;
      else if (fnlookup(uptr->name))
          printf("Unit '%s' hidden by function '%s'\n", uptr->name, uptr->name);
      else if (parseunit(&have, uptr->value,&err,0))
          printf("'%s' defined as '%s': %s\n",uptr->name, uptr->value,err);
      else if ((reduce_err=completereduce(&have))){
          printf("'%s' defined as '%s' irreducible: %s",uptr->name, uptr->value,errormsg[reduce_err]);
          if (reduce_err==E_UNKNOWNUNIT && irreducible)
            printf(" '%s'", irreducible);
          putchar('\n');
      }
      else {
        parserflags.minusminus = !parserflags.minusminus;
                                                 /* coverity[check_return] */
        parseunit(&second, uptr->name, 0, 0);    /* coverity[check_return] */
        completereduce(&second);     /* Can't fail because it worked above */
        if (compareunits(&have, &second, ignore_nothing)){
          printf("'%s': replace '-' with '+-' for subtraction or '*' to multiply\n", uptr->name);
        }
        freeunit(&second);
        parserflags.minusminus=!parserflags.minusminus;
      }

      freeunit(&have);
    }

  /* Check prefixes */

  testunit="meter";
  for(i=0;i<SIMPLEHASHSIZE;i++)
    for(pptr = ptab[i]; pptr; pptr = pptr->next){
      if (verbosecheck)
        printf("doing '%s-'\n",pptr->name);
      prefixbuf = mymalloc(strlen(pptr->name) + strlen(testunit) + 1,
                           "(checkunits)");
      strcpy(prefixbuf,pptr->name);
      strcat(prefixbuf,testunit);
      if (parseunit(&have, prefixbuf,0,0) || completereduce(&have) ||
          compareunits(&have,&one,ignore_primitive))
        printf("'%s-' defined as '%s' irreducible\n",pptr->name, pptr->value);
      else {
        int plevel;    /* check for bad '/' character in prefix */
        char *ch;
        plevel = 0;
        for(ch=pptr->value;*ch;ch++){
          if (*ch==')') plevel--;
          else if (*ch=='(') plevel++;
          else if (plevel==0 && *ch=='/'){
            printf(
              "'%s-' defined as '%s' contains a bad '/'. (Add parentheses.)\n",
              pptr->name, pptr->value);
            break;
          }
        }
      }
      freeunit(&have);
      free(prefixbuf);
    }
}


/*
   Converts the input value 'havestr' (which is already parsed into
   the unit structure 'have') into a sum of the UNITSEPCHAR-separated
   units listed in 'wantstr'.  You must call checkunitlist first to
   ensure 'wantstr' is error-free.  Prints the results (or an error message)
   on stdout.  Returns 0 on success and 1 on failure.
*/

int
showunitlist(char *havestr, struct unittype *have, char *wantstr)
{
  struct unittype want, lastwant;
  char *lastunitstr, *nextunitstr, *lastwantstr = NULL;
  double remainder;     /* portion of have->factor remaining */
  double round_dir;     /* direction of rounding */
  double value;         /* value (rounded to integer with 'r' option) */
  int firstunit = 1;    /* first unit in a multi-unit string */
  int value_shown = 0;  /* has a value been shown? */
  int sigdigits;
  char val_sign;

  initializeunit(&want);
  remainder = fabs(have->factor);
  val_sign = have->factor < 0 ? '-' : '+';
  lastunitstr = 0;
  nextunitstr = 0;
  round_dir = 0;

  if (flags.round) {
    /* disable unit repetition with terminal UNITSEPCHAR when rounding */
    if (lastchar(wantstr) == UNITSEPCHAR)
      lastchar(wantstr) = 0;
    if ((lastwantstr = strrchr(wantstr, UNITSEPCHAR)))
      lastwantstr++;
  }

  while (wantstr) {
    if ((nextunitstr = strchr(wantstr, UNITSEPCHAR)))
      *(nextunitstr++) = '\0';
    removespaces(wantstr);

    /*
      if wantstr ends in UNITSEPCHAR, repeat last unit--to give integer
      and fractional parts (3 oz + 0.371241 oz rather than 3.371241 oz)
    */
    if (emptystr(wantstr))              /* coverity[alias_transfer] */
      wantstr = lastunitstr;

    if (processunit(&want, wantstr, NOPOINT)) {
      freeunit(&want);
      return 1;
    }

    if (firstunit){
      /* checkunitlist() ensures conformability within 'wantstr',
         so we just need to check the first unit to see if it conforms
         to 'have' */
      if (compareunits(have, &want, ignore_dimless)) {
        showconformabilityerr(havestr, have, wantstr, &want);
        freeunit(&want);
        return 1;
      }

      /* round to nearest integral multiple of last unit */
      if (flags.round) {
        value = remainder;
        if (nonempty(lastwantstr)) {      /* more than one unit */
          removespaces(lastwantstr);
          initializeunit(&lastwant);
          if (processunit(&lastwant, lastwantstr, NOPOINT)) {
            freeunit(&lastwant);
            return 1;
          }
          remainder = round(remainder / lastwant.factor) * lastwant.factor;
        }
        else    /* first unit is last unit */
          remainder = round(remainder / want.factor) * want.factor;

        round_dir = remainder - value;
      }
      if (flags.verbose == 2) {
        removespaces(havestr);
        logprintf("\t%s = ", havestr);
      } else if (flags.verbose == 1)
        logputchar('\t');
    } /* end if first unit */

    if (0==(sigdigits = getsigdigits(have->factor, remainder, 10)))
      break;    /* nothing left */

    /* Remove sub-precision junk accumulating in the remainder.  Rounding
       is base 2 to ensure that we keep all valid bits. */
    remainder = round_digits(remainder,
                             getsigdigits(have->factor,remainder,2),2);
    if (nextunitstr)
      remainder = want.factor * modf(remainder / want.factor, &value);
    else
      value = remainder / want.factor;

    /* The remainder represents less than one of the current want unit.
       But with display rounding it may round up to 1, leading to an output
       like "4 feet + 12 inch".  Check for this case and if the remainder
       indeed rounds up to 1 then add that remainder into the current unit
       and set the remainder to zero. */
    if (nextunitstr){
      double rounded_next =
          round_digits(remainder/want.factor,
                       getsigdigits(have->factor, remainder / want.factor, 10),
                       10);
      if (displays_as(1,rounded_next, NULL)){
        value++;
        remainder = 0;         /* Remainder is zero */
      }
    }

    /* Round the value to significant digits to prevent display
       of bogus sub-precision decimal digits */
    value = round_digits(value,sigdigits,10);

    /* This ensures that testing value against zero will work below
       at the last unit, which is the only case where value is not integer */
    if (!nextunitstr && displays_as(0, value, NULL))
      value=0;

    if (!flags.verbose){
      if (!firstunit)
        logputchar(UNITSEPCHAR);
      logprintf(num_format.format,value);
      value_shown=1;
    } else { /* verbose case */
      if (value != 0) {
        if (value_shown) /* have already displayed a number so need a '+' */
          logprintf(" %c ",val_sign);
        else if (val_sign=='-')
          logputs("-");
        showunitname(value, wantstr, PRINTNUM);
        if (sigdigits <= floor(log10(value))+1)   /* Used all sig digits */
          logprintf(" (at %d-digit precision limit)", MAXPRECISION);
        value_shown=1;
      }
    }

    freeunit(&want);
    lastunitstr = wantstr;
    wantstr = nextunitstr;
    firstunit = 0;
  }

  /* if the final unit value was rounded print indication */
  if (!value_shown) {  /* provide output if every value rounded to zero */
    logputs("0 ");
    if (isdecimal(*lastunitstr))
      logputs("* ");
    logputs(lastunitstr);
  }

  if (round_dir != 0) {
    if (flags.verbose){
      if (round_dir > 0)
        logprintf(" (rounded up to nearest %s) ", lastunitstr);
      else
        logprintf(" (rounded down to nearest %s) ", lastunitstr);
    } else
      logprintf("%c%c", UNITSEPCHAR, round_dir > 0 ?'-':'+');
  }
  logputchar('\n');
  return 0;
} /* end showunitlist */


#if defined (_WIN32) && defined (HAVE_MKS_TOOLKIT)
int
ismksmore(char *pager)
{
    static int mksmore = -1;

    if (mksmore >= 0)
      return mksmore;

    /*
      Tries to determine whether the MKS Toolkit version of more(1) or
      less(1) will run.  In older versions of the Toolkit, neither
      accepted '+<lineno>', insisting on the POSIX-compliant '+<lineno>g'.
    */
    if (strstr(pager, "more") || strstr(pager, "less")) {
      char *mypager, *mkspager, *mksroot, *p;
      char pathbuf[FILENAME_MAX + 1];
      struct _stat mybuf, mksbuf;

      mypager = NULL;
      mkspager = NULL;
      mksmore = 0;

      if (strlen(pager) > FILENAME_MAX) /* no point in further checking because it cannot run */
        return 0;
      else if (!isfullpath(pager)) {
        mypager = (char *) mymalloc(strlen(pager) + strlen(EXE_EXT) + 1, "(ismksmore)");
        strcpy(mypager, pager);
        if (!((p = strrchr(mypager, '.')) && isexe(p)))
          strcat(mypager, EXE_EXT);

        _searchenv(mypager, "PATH", pathbuf);
      }
      else
        strcpy(pathbuf, pager);

      if (! *pathbuf)
        return 0;

      mksroot = getenv("ROOTDIR");
      if (mksroot) {
        size_t buflen;
        char
            *mksbin = "\\mksnt\\",      /* for historical reasons, there's a separate 'bin' directory */
            *mksprog;

        if (strstr(pager, "more"))
          mksprog = "more.exe";
        else
          mksprog = "less.exe";

        buflen = snprintf(NULL, 0, "%s%s%s", mksroot, mksbin, mksprog) + 1;
        mkspager = (char *) mymalloc(buflen, "(ismksmore)");
        snprintf(mkspager, buflen, "%s%s%s", mksroot, mksbin, mksprog);
      }

      if (*pathbuf && mksroot) {
        if (_stat(mkspager, &mksbuf)) {
          fprintf(stderr, "%s: cannot stat file '%s'. ", progname, mkspager);
          perror((char *) NULL);
          return 0;
        }
        if (_stat(pathbuf, &mybuf)) {
          fprintf(stderr, "%s: cannot stat file '%s'. ", progname, pathbuf);
          perror((char *) NULL);
          return 0;
        }
        /*
          if we had inodes, this would be simple ... but if it walks
          like a duck and swims like a duck and quacks like a duck ...
        */
        if (mybuf.st_size   == mksbuf.st_size
          && mybuf.st_ctime == mksbuf.st_ctime
          && mybuf.st_mtime == mksbuf.st_mtime
          && mybuf.st_atime == mksbuf.st_atime
          && mybuf.st_mode  == mksbuf.st_mode)
            mksmore = 1;
      }
      if (mypager)
        free(mypager);
      if (mkspager)
        free(mkspager);
    }

    return mksmore;
}
#endif

/*
   Checks to see if the input string contains HELPCOMMAND possibly
   followed by a unit name on which help is sought.  If not, then
   return 0.  Otherwise invoke the pager on units file at the line
   where the specified unit is defined.  Then return 1.  */

int
ishelpquery(char *str, struct unittype *have)
{
  struct unitlist *unit;
  struct func *function;
  struct wantalias *alias;
  struct prefixlist *prefix;
  char *commandbuf;
  size_t buflen;
  int unitline;
  char *file;
  char **exitptr;
  char *commandstr;       /* command string varies with OS */

  if (have && !strcmp(str, UNITMATCH)){
    tryallunits(have,0);
    return 1;
  }
  for(exitptr=exit_commands;*exitptr;exitptr++)
    if (!strcmp(str, *exitptr))
      exit(EXIT_SUCCESS);
  if (startswith(str, SEARCHCOMMAND)){
    str+=strlen(SEARCHCOMMAND);
    if (!emptystr(str) && *str != ' ')
      return 0;
    removespaces(str);
    if (emptystr(str)){
      printf("\n\
Type 'search text' to see a list of all unit names \n\
containing 'text' as a substring\n\n");
      return 1;
    }
    tryallunits(0,str);
    return 1;
  }
  if (startswith(str, HELPCOMMAND)){
    str+=strlen(HELPCOMMAND);
    if (!emptystr(str) && *str != ' ')
      return 0;
    removespaces(str);

    if (emptystr(str)){
      int nlines;
      char *unitsfile;
      char *msg = "\n\
%s converts between different measuring systems and    %s6 inches\n\
acts as a units-aware calculator.  At the '%s'    %scm\n\
prompt, type in the units you want to convert from or             * 15.24\n\
an expression to evaluate.  At the '%s' prompt,           / 0.065\n\
enter the units to convert to or press return to see\n\
the reduced form or definition.                           %stempF(75)\n\
                                                          %stempC\n\
The first example shows that 6 inches is about 15 cm              23.889\n\
or (1/0.065) cm.  The second example shows how to\n\
convert 75 degrees Fahrenheit to Celsius.  The third      %sbu^(1/3)\n\
example converts the cube root of a bushel to a list      %sft;in\n\
of semicolon-separated units.                                     1 ft + 0.9 in\n\
\n\
To quit from %s type 'quit' or 'exit'.       %s2 btu + 450 ft lbf\n\
                                                %s(kg^2/s)/(day lb/m^2)\n\
At the '%s' prompt type '%s' to get a            * 1.0660684e+08\n\
list of conformable units.  At either prompt you        / 9.3802611e-09\n\
type 'help myunit' to browse the units database\n\
and read the comments relating to myunit or see         %s6 tbsp sugar\n\
other units related to myunit.  Typing 'search          %sg\n\
text' will show units whose names contain 'text';               * 75\n\
typing 'search .' will show all units.  Typing                  / 0.013333333\n\
'search _' will show all runtime variables,\n\
whose names begin with '_'.\n\
\n\
Many command-line options can be set interactively by typing\n\
'set <option>=<value>'; typing 'set <option>' will show the value\n\
of that option.  Typing 'set' will show a list of options\n\
that can be set; options set to other than default values will have\n\
a prepended '*'.\n\n";

    char *fmsg = "To learn about the available units look in '%s'\n\n";
    FILE *fp;

    /* presumably, this cannot fail because it was already checked at startup */
    unitsfile = findunitsfile(NOERRMSG);

    nlines = countlines(msg);
    /* but check again anyway ... */
    if (unitsfile)
      nlines += countlines(fmsg);

    /* see if we need a pager */
    fp = get_output_fp(nlines);

    fprintf(fp, msg,
      progname, QUERYHAVE,
      QUERYHAVE, QUERYWANT,
      QUERYWANT,
      QUERYHAVE,QUERYWANT,QUERYHAVE,QUERYWANT,
      progname, QUERYHAVE,QUERYWANT,
      QUERYWANT,
      UNITMATCH,
      QUERYHAVE,QUERYWANT);

      if (unitsfile)
        fprintf(fp, fmsg, unitsfile);

      if (fp != stdout)
        pclose(fp);

      return 1;
    }
    if ((function = fnlookup(str))){
      file = function->file;
      unitline = function->linenumber;
    }
    else if ((unit = ulookup(str))){
      unitline = unit->linenumber;
      file = unit->file;
    }
    else if ((prefix = plookup(str)) && strlen(str)==prefix->len){
      unitline = prefix->linenumber;
      file = prefix->file;
    }
    else if ((alias = aliaslookup(str))){
      unitline = alias->linenumber;
      file = alias->file;
    }
    else {
      printf("Unknown unit '%s'\n",str);
      return 1;
    }

  /*
    With Microsoft compilers, system() uses cmd.exe.
    Inner escaped quotes are necessary for filenames with spaces; outer
    escaped quotes are necessary for cmd.exe to see the command as a
    single string containing one or more quoted strings
    (e.g., cmd /c ""command" "arg1" "arg2" ... ")
  */
#if defined (_WIN32)
  #if defined (HAVE_MKS_TOOLKIT)
    if (strstr(pager, "pg"))
      commandstr = "\"\"%s\" +%d \"%s\"\"";
    else if (ismksmore(pager)) {
      /*
        use the POSIX-compliant '+<number>g' for compatibility with older
        versions of the Toolkit that don't accept '+<number>'
      */
      commandstr = "\"\"%s\" +%dg \"%s\"\"";
    }
    else {
      /*
        more.com apparently takes the number as an offset rather than a
        line number, so that '+1' starts at line 2 of the file.
        more.com also cannot back up, so allow two lines of preceding
        context.
      */
      unitline -= 3;
      if (unitline < 0)
        unitline = 0;
      commandstr = "\"\"%s\"  +%d \"%s\"\"";
    }
  #else /* more.com is probably the only option */
    unitline -= 3;
    if (unitline < 0)
      unitline = 0;
    commandstr = "\"\"%s\"  +%d \"%s\"\"";
  #endif
#else   /* *nix */
    commandstr = "%s +%d %s";
#endif
    buflen = snprintf(NULL, 0, commandstr, pager, unitline, file) + 1;
    commandbuf = (char *) mymalloc(buflen, "(ishelpquery: pager)");
    snprintf(commandbuf, buflen, commandstr, pager, unitline, file);

    if (system(commandbuf))
      fprintf(stderr,"%s: cannot invoke pager '%s' to display help\n",
              progname, pager);
    return 1;
  }
  return 0;
}

/*
    Interactive Option Setting
*/

int
check_yn_choices(char *tag, char *valstr, int *flag)
{
    size_t len;
    char *p;

    len = strlen(valstr);
    p = valstr;
    while (*p)
        *p++ = tolower(*p);

    /* accept yes/no, true/false, 1/0 */
    if (equalnv(valstr, YES_STR, 1, len) || equal(valstr, "1"))
        *flag = 1;
    else if (equalnv(valstr, NO_STR, 1, len) || equal(valstr, "0"))
        *flag = 0;
    else {
        fprintf(stderr, "invalid value (%s) for %s;", valstr, tag);
        fprintf(stderr, " choices are %s\n", YN_CHOICES);
        return -1;
    }

    return 0;
}

int
setflag_yn(char *tag, int n, char *valstr, int *flag)
{
    if (n < 2) {
        printf("  %s = %s\n", tag, *flag ? YES_STR : NO_STR);
        return 1;
    }
    else if (check_yn_choices(tag, valstr, flag) < 0)
        return -1;

    printf("  %s = %s\n", tag, *flag ? YES_STR : NO_STR);
    return 1;
}

int
setflag_ival(char *tag, int n, char *valstr, int min, int max, int *flag)
{
    int value;
    char *nonum;

    if (n < 2) {
        printf("  %s = %d\n", tag, *flag);
        return 1;
    }
    value = strtol(valstr, &nonum, 10);
    if (*nonum) {
        fprintf(stderr, "invalid value (%s) for %s; integer required\n", valstr, tag);
        return -1;
    }
    else if (value < min || value > max) {
        fprintf(stderr, "invalid value (%d) for %s;", value, tag);
        if (max - min > 1)
            fprintf(stderr, " range is %d to %d\n", min, max);
        else
            fprintf(stderr, " values are %d or %d\n", min, max);
        return -1;
    }
    else
        *flag = value;

    printf("  %s = %d\n", tag, *flag);
    return 1;
}

void
show_yn_setting(char *tag, int flag, int def_val, char *desc)
{
    int buflen = 30;
    char yn_status[32];

    snprintf(yn_status, buflen, " %c%s = %s",
               flag != def_val ? SETFLAG : ' ', tag, flag ? YES_STR : NO_STR);
    printf("%-*s (%s) %s\n", SETTING_STATUS_LEN, yn_status, YN_STR, desc);
}

void
show_ival_setting(char *tag, int value, int def_val, char *choices, char *desc)
{
    int buflen = 30;
    char setting_status[32];

    snprintf(setting_status, buflen, " %c%s = %d",
               value != def_val ? SETFLAG : ' ', tag, value);
    if (*choices)
        printf("%-*s (%s) %s\n", SETTING_STATUS_LEN, setting_status, choices, desc);
    else
        printf("%-*s %s\n", SETTING_STATUS_LEN, setting_status, desc);
}

void
show_strval_setting(char *tag, char *strval, char *def_val, char *desc)
{
    int buflen = 30;
    char setting_status[32];

    snprintf(setting_status, buflen, " %c%s = %s",
               equal(strval, def_val) ? ' ' : SETFLAG, tag, strval);
    printf("%-*s %s\n", SETTING_STATUS_LEN, setting_status, desc);
}

/* show version or detailed info; show/set some command-line options */
int
iscommand(char *str, char *comment, struct query *q)
{
  int
      n,
      ndigits,  /* significant digits for E/e/G/g; digits after decimal point for others */
      value;
  size_t vlen;
  char *p, *tag;
  static int lastformatprecision;
  static char lastformattype;
  char setting[1000], valstr[1000];
  void setquery(struct query *);

  if (equal(str, VERCOMMAND)) {
      printf("GNU Units version %s\n", VERSION);
      return 1;
  }
  else if (equal(str, INFOCOMMAND)) {
      int verbose = flags.verbose;

      flags.verbose = 2;
      printversion();
      flags.verbose = verbose;
      return 1;
  }
  else if (!startswith(str, SETCOMMAND))
      return 0;

  /* remove trailing spaces */
  removespaces(str);

  if (strchr("Ee", num_format.type))
      ndigits = num_format.precision + 1;
  else
      ndigits = num_format.precision;

  /* show all options */
  if (equal(str, SETCOMMAND)) {
      int
          terse,
          space_len,    /* number of spaces with which to pad continued line */
          yn_status_len;
      size_t yn_len;    /* length of "(" YN_STR ")" */
      char default_format[16];

      yn_status_len = 22;
      yn_len = strlen("(" YN_STR ")");
      space_len = yn_status_len + yn_len + 2;

      /* prompting ("quiet" mode) */
      show_yn_setting("q[uiet]", flags.quiet, 0, "do/don't suppress prompting");

      /* one-line output */
      show_yn_setting("o[neline]", flags.oneline, 0, "do/don't suppress the second line of output");

      /* reciprocal conversion */
      show_yn_setting("st[rict]", flags.strictconvert, 0,"do/don't suppress reciprocal unit conversion");
      printf("%*s%s\n", space_len, " ", "(e.g., Hz<->s)");

      /* terse output */
      if (flags.oneline == 1 && flags.quiet == 1 &&
          flags.strictconvert == 1 && flags.verbose == 0)
          terse = 1;
      else
          terse = 0;
      show_yn_setting("t[erse]", terse, 0,"do/don't give very terse output");

      /* compact output */
      show_yn_setting("c[ompact]", terse, 0,"do/don't suppress printing tab, SETFLAG, and '/'");
      printf("%*s%s\n", space_len, " ", "characters in results");

      /* verbosity of output */
      show_ival_setting("v[erbose]", flags.verbose, 1, "0|1|2", "amount of information shown");

      /*
          number format
      */
      /* significant digits */
      if (strchr("EeGg", num_format.type))
          show_ival_setting("d[igits]", ndigits, DEFAULTPRECISION, "",
                             "number of significant digits in output");
      else
          show_ival_setting("d[digits]", ndigits, DEFAULTPRECISION, "",
                             "number digits to the right of the decimal point");
      /* exponential format */
      show_yn_setting("e[xponential]", num_format.type == 'e' ? 1 : 0, 0,
                      "do/don't use exponential (\"scientific\") notation");
      /* printf(3) format specification */
      sprintf(default_format, "%%.%d%c", DEFAULTPRECISION, DEFAULTTYPE);
      show_strval_setting("f[ormat]", num_format.format, default_format, "printf(3) format specification");

      /* units lists */
      show_yn_setting("u[nitlists]", flags.unitlists, 1,"do/don't allow conversion to unit lists");

      /* rounding */
      show_yn_setting("r[ound]", flags.round, 0, "do/don't round last element of unit list output");
      printf("%*s%s\n", space_len, " ", "to an integer");

      /* non-unity factors for unit lists */
      show_yn_setting("sh[owfactor]", flags.showfactor, 0, "do/don't show non-unity factor before 1|x");
      printf("%*s%s\n", space_len, " ", "in multi-unit output");

      return 1;
  }
  if (logfile)
      fprintf(logfile, "%s\n", str);

  /* assume it's a unit */
  if (*(p = str + strlen(SETCOMMAND)) != ' ')
      return 0;

  while (*p == ' ')
      p++;

  *valstr = '\0';
  n = sscanf(p, " %[a-z] = %[-a-zA-Z0-9!#$%&'()*+,-./;<=>?@ ]", setting, valstr);

  if (n < 1) {
    fprintf(stderr, "invalid setting (%s)\n", p);
    return -1;
  }
  else if (n < 2) {
      char *p2;

      p2 = p + strlen(setting);
      if (*p2) {
          if (strchr(p2, '=')) {
              fprintf(stderr, "invalid command (%s)--missing value\n", str);
              return -1;
          }
          else {
              fprintf(stderr, "invalid command (%s)\n", str);
              return -1;
          }
      }
  }
  else
      vlen = strlen(valstr);

  /* don't/do suppress prompting (-q, --quiet) */
  if (equalnv(setting, tag = "quiet", 1, strlen(setting))) {
    if (setflag_yn(tag, n, valstr, &flags.quiet) < 0)
        return -1;
    setquery(q);
    return 1;
  }

  /* don't/do  suppress the second line of output (-1, --one-line) */
  else if (equalnv(setting, tag = "oneline", 1, strlen(setting)))
    return setflag_yn(tag, n, valstr, &flags.oneline);

  /* don't/do suppress reciprocal unit conversion (-s, --strict) */
  else if (equalnv(setting, tag = "strict", 2, strlen(setting)))
    return setflag_yn(tag, n, valstr, &flags.strictconvert);

  /* don't/do give very terse output (-t, --terse) */
  else if (equalnv(setting, tag = "terse", 1, strlen(setting))) {
      int value;
      if (n < 2) {
          if (flags.oneline == 1 && flags.quiet == 1 &&
              flags.strictconvert == 1 && flags.verbose == 0)
              printf("  terse = %s\n", YES_STR);
          else
              printf("  terse = %s\n", NO_STR);
          return 1;
      }
      if (check_yn_choices(tag, valstr, &value) < 0)
          return -1;
      else if (value == 1) {
        flags.oneline = 1;
        flags.quiet = 1;
        flags.strictconvert = 1;
        flags.verbose = 0;
      }
      else {
        flags.oneline = 0;
        flags.quiet = 0;
        flags.strictconvert = 0;
        flags.verbose = 1;
      }

      printf("  %s = %s\n", tag, value == 1 ? YES_STR : NO_STR);
      setquery(q);
      return 1;
  }
  /* don't/do suppress printing of tab, '*', and '/' character (--compact) */
  else if (equalnv(setting, tag = "compact", 1, strlen(setting))) {
      int value;

      if (n < 2) {
          printf("  %s = %d\n", tag, flags.verbose == 0 ? 1 : 0);
          return 1;
      }
      else if (check_yn_choices(tag, valstr, &value) < 0)
          return -1;
      else if (value == 1)
          flags.verbose = 0;
      else
          flags.verbose = 1;

      printf("  %s = %s\n", tag, value == 1 ? YES_STR : NO_STR);
      printf("  verbose = %d\n", flags.verbose);
  }
  /* amount of output (-v, --verbose) */
  else if (equalnv(setting, tag = "verbose", 1, strlen(setting)))
    return setflag_ival(tag, n, valstr, 0, 2, &flags.verbose);

  /* suppress/allow conversion to unit lists (-n, --nolists) */
  else if (equalnv(setting, tag = "unitlists", 1, strlen(setting)))
    return setflag_yn(tag, n, valstr, &flags.unitlists);

  /* don't/do round last element of unit list output to an integer (-r, --round) */
  else if (equalnv(setting, tag = "round", 1, strlen(setting)))
    return setflag_yn(tag, n, valstr, &flags.round);

  /* don't/do show non-unity factor before 1|x in multi-unit output (-S, --show-factor) */
  else if (equalnv(setting, tag = "showfactor", 2, strlen(setting)))
    return setflag_yn(tag, n, valstr, &flags.showfactor);

  /* numerical precision (-d, --digits) */
  else if (equalnv(setting, tag = "digits", 1, strlen(setting))) {
      char *nonum;

      lastformatprecision = num_format.precision;

      if (n < 2) {
          printf("  %s = %d\n", tag, ndigits);
          printf("  format = %s\n", num_format.format);
          return 1;
      }
      else if (equal(valstr, "max"))
          ndigits = num_format.precision = MAXPRECISION;
      else {
          value = strtol(valstr, &nonum, 10);
          if (*nonum) {
              fprintf(stderr, "invalid value (%s) for %s; integer required\n", valstr, tag);
              return -1;
          }
          else if (value < 1) {
             fprintf(stderr, "invalid value (%d) for %s; using minimum of %d\n",
                     value, tag, 1);
              num_format.precision = 1;
          }
          /*
              "Digits" is significant digits for 'E', 'e', 'G', and 'g'
              types but digits after the decimal point for other types.
              With fixed point, there is no easy way to control the
              number of significant digits without knowing the value of
              a number.
          */
          else if (strchr("EeGg", num_format.type) && value > MAXPRECISION) {
             fprintf(stderr, "invalid value (%d) for %s; maximum for '%c' format is %d\n",
                     value, tag, num_format.type, MAXPRECISION);
              ndigits = MAXPRECISION;
          }
          else
              ndigits = value;

          /*
              For printf(3), "precision" is significant digits only for
              'G' and 'g' types, but digits after the radix for 'E' and
              'e' types.  So we need to reduce its value by one with 'E'
              or 'e' to avoid showing garbage.
          */
          if (strchr("Ee", num_format.type))
              num_format.precision = ndigits - 1;
          else
              num_format.precision = ndigits;
      }

      if (num_format.precision != lastformatprecision)
          setnumformat();

      printf("  %s = %d\n", tag, ndigits);
      printf("  format = %s\n", num_format.format);

      return 1;
  }

  /* exponential number format  (-e, --exponential) */
  else if (equalnv(setting, tag = "exponential", 1, strlen(setting))) {
      int value, reformat = 0;

      if (n < 2) {
          printf("  %s = %s\n", tag, strchr("Ee", num_format.type) ? YES_STR : NO_STR);
          printf("  format = %s\n", num_format.format);
          return 1;
      }
      if (check_yn_choices(tag, valstr, &value) < 0)
          return -1;
      /* turn on exponential format */
      else if (value == 1) {
          if (!strchr("Ee", num_format.type)) {
              lastformattype = num_format.type;
              reformat = 1;
          }

          if (strchr("Gg", num_format.type))
              num_format.precision--;
          num_format.type = 'e';
      }
      /* turn off exponential format */
      else {    /* value = 0 */
          if (lastformattype) {
              num_format.type = lastformattype;
              if (strchr("Gg", lastformattype))
                  num_format.precision++;
          }
          else {
              num_format.precision++;
              num_format.type = 'g';
          }
          reformat = 1;
      }
      if (reformat)
          setnumformat();

      printf("  %s = %s\n", tag, value == 1 ? YES_STR : NO_STR);
      printf("  format = %s\n", num_format.format);
  }

  /* numerical output format string (-o, --output-format) */
  else if (equalnv(setting, tag = "format", 1, strlen(setting))) {
      if (n < 2) {
          printf("  format = %s\n", num_format.format);
          return 1;
      }
      else {
        int errors = 0;
        char *oldformat;
        char *formatstr;
        char *realcomment;

        /*
            Assume the first '#' is a '#' flag in a format
            specification; a second '#' begins an actual
            comment--get rid of it.  Any spaces preceeding '#' in
            a format specification (e.g., "%- #.3f") were
            stripped along with the "comment," and cannot be
            recovered.
        */
        if (comment && strchr("%-+0", lastchar(valstr))) {
            size_t buflen;
            if ((realcomment = strchr(comment, COMMENTCHAR)))
              *realcomment = '\0';
            removespaces(comment);
            buflen = snprintf(NULL, 0, "%s#%s", valstr, comment) + 1;
            formatstr = mymalloc(buflen, "(iscommand: format)");
            snprintf(formatstr, buflen, "%s#%s", valstr, comment);
        }
        else {
            formatstr = mymalloc(strlen(valstr) + 1, "(iscommand: format)");
            strcpy(formatstr, valstr);
        }

        oldformat = dupstr(num_format.format, "(iscommand: format)");
        free(num_format.format);
        num_format.format = dupstr(formatstr, "(iscommand: format)");
        free(formatstr);

        if (parsenumformat()) {
            free(num_format.format);
            num_format.format = oldformat;
            errors++;
        }
        else
            free(oldformat);

        printf("  format = %s\n", num_format.format);
        if (errors)
            return -1;
      }
  }

  else {
      fprintf(stderr, "invalid setting (%s)\n", setting);
      return -1;
  }

  return 1;
}

#ifdef SUPPORT_UTF8
void
checklocale()
{
  char *temp;
#if defined _WIN32 && defined _MSC_VER
  /* only works in Windows Terminal */
  if (getenv(WINDOWS_TERMINAL)) {
      utf8mode = (GetConsoleCP() == CP_UTF8)
                && (GetConsoleOutputCP() == CP_UTF8);
  }
  else
      utf8mode = 0;
  if (utf8mode)
      temp = setlocale(LC_CTYPE,".utf8");
  else
      temp = setlocale(LC_CTYPE,"");
#else
  temp = setlocale(LC_CTYPE,"");
  utf8mode = (strcmp(nl_langinfo(CODESET),"UTF-8")==0);
#endif
  if (temp){
    mylocale = dupstr(temp, "(setlocale)");
    temp = strchr(mylocale,'.');
    if (temp)
      *temp = '\0';
  } else
    mylocale = DEFAULTLOCALE;
}

#else

void
checklocale()
{
  char *temp = NULL;
  /* environment overrides system */
  temp = getenv("LC_CTYPE");
  if (!temp)
    temp = getenv("LC_ALL");
  if (!temp)
    temp = getenv("LANG");
#ifndef NO_SETLOCALE
  if (temp)
    temp = setlocale(LC_CTYPE,temp);
  if (!temp)
    temp = setlocale(LC_CTYPE,"");      /* try system default */
#endif
  if (!temp)
    mylocale = DEFAULTLOCALE;
  else {
    mylocale = dupstr(temp, "(checklocale)");
    temp = strchr(mylocale,'.');
    if (temp)
      *temp = '\0';
  }
}

#endif

/*
   Replaces an alias in the specified string input.  Returns 1 if the
   alias that is found contains errors.
*/

int
replacealias(char **string, int *buflen)
{
  int usefree = 1;
  struct wantalias *aliasptr;
  char *input;

  if (!flags.readline && buflen)
    usefree = 0;

  if (nonempty(*string)) {  /* check that string is defined and nonempty */
    input = *string;
    removespaces(input);
    if ((aliasptr=aliaslookup(input))){
      if (checkunitlist(aliasptr->definition,NOERRMSG)){
        puts("Unit list definition contains errors.");
        return 1;
      }
      if (usefree){
        free(*string);
        *string = dupstr(aliasptr->definition, "(replacealias)");
      } else {                           /* coverity[dead_error_line] */
        while (strlen(aliasptr->definition)>*buflen)
          growbuffer(string, buflen);
        strcpy(*string, aliasptr->definition);
      }
    }
  }
  return 0;
}


/*
   Remaps the locale name returned on Windows systems to the value
   returned on Unix-like systems
*/

void
remaplocale(char *filename)
{
  FILE *map;
  char *value, *hyphen;
  char name[80];

  map = openfile(filename,"rt");
  if (!map) {
    fprintf(stderr,"%s: cannot open locale map '%s'. ",progname,filename);
    perror((char *) NULL);
  }
  else {
    /* map hyphen to underscore, e.g., allow "en-US" */
    if ((hyphen = strchr(mylocale, '-')))
      *hyphen = '_';
    while(!feof(map)){
      if (!fgets(name,80,map))
        break;
      lastchar(name) = 0;
      value=strchr(name,'#');
      if (value) *value = '\0';
      value=strchr(name,'\t');
      if (!value) continue;
      *value++ = '\0';
      removespaces(value);
      removespaces(name);
      if (!strcmp(name, mylocale)) {
        mylocale = dupstr(value, "(remaplocale)");
        break;
      }
    }
    fclose(map);
  }
}


void
close_logfile(void)
{
  if (logfile){
    fputc('\n',logfile);
    fclose(logfile);
  }
}


void
open_logfile(void)
{
  time_t logtime;
  char * timestr;

  logfile = openfile(logfilename, "at");
  if (!logfile){
    fprintf(stderr, "%s: cannot write to log file '%s'.  ",
            progname, logfilename);
    perror(0);
    exit(EXIT_FAILURE);
  }
  time(&logtime);
  timestr = ctime(&logtime);
  fprintf(logfile, "### Log started %s \n", timestr);
  atexit(close_logfile);
}

void
write_files_sig(int sig)
{
#ifdef READLINE
  if (historyfile)
    save_history();
#endif
  close_logfile();
  signal(sig, SIG_DFL);
  raise(sig);
}

void
setquery(struct query *q)
{
   if (flags.quiet)
     q->have = q->want = "";   /* No prompts are being printed */
   else {
     if (!promptprefix){
       q->have = QUERYHAVE;
       q->want = QUERYWANT;
     } else {
       if (q->have)
           free(q->have);
       q->have = (char *)mymalloc(strlen(promptprefix)+strlen(QUERYHAVE)+1,
                                    "(main)");
       if (q->want)
           free(q->want);
       q->want = (char *)mymalloc(strlen(promptprefix)+strlen(QUERYWANT)+1,
                                    "(main)");
       strcpy(q->have, promptprefix);
       strcat(q->have, QUERYHAVE);
       memset(q->want, ' ', strlen(promptprefix));
       strcpy(q->want+strlen(promptprefix), QUERYWANT);
     }
   }
   q->havewidth = strwidth(q->have);
   q->wantwidth = strwidth(q->want);
}

int
main(int argc, char **argv)
{
   static struct unittype have, want;
   struct query q;
   char *havestr = NULL, *wantstr = NULL;
   int leading_spaces;
   struct func *funcval;
   struct wantalias *alias;
   int havestrsize=0;   /* Only used if READLINE is undefined */
   int wantstrsize=0;   /* Only used if READLINE is undefined */
   int readerr;
   char **unitfileptr;
   int unitcount=0, prefixcount=0, funccount=0;   /* for counting units */
   char *comment;
#ifdef _WIN32
   char *localemap;
#endif

   /* Set program parameter defaults */
   num_format.format = NULL;
   num_format.flags = dupstr("", "(main)");
   num_format.precision = DEFAULTPRECISION;
   num_format.type = DEFAULTTYPE;

   flags.listallunits = 0; /* Do not just list all units and exit */
   flags.quiet = 0;       /* Do not suppress prompting */
   flags.unitcheck = 0;   /* Unit checking is off */
   flags.verbose = 1;     /* Medium verbosity */
   flags.round = 0;       /* Rounding off */
   flags.strictconvert=0; /* Strict conversion disabled (reciprocals active) */
   flags.unitlists = 1;   /* Multi-unit conversion active */
   flags.oneline = 0;     /* One line output is disabled */
   flags.showconformable=0;  /* show unit conversion rather than all conformable units */
   flags.showfactor = 0;  /* Don't show a multiplier for a 1|x fraction */
                          /*       in unit list output */
   parserflags.minusminus = 1;  /* '-' character gives subtraction */
   parserflags.oldstar = 0;     /* '*' has same precedence as '/' */

   q.have = q.want = NULL;

   progname = getprogramname(argv[0]);

   /*
     unless UNITSFILE and LOCALEMAP have absolute pathnames, we may need
     progdir to search for supporting files
   */

   if (!(isfullpath(UNITSFILE) && isfullpath(LOCALEMAP)))
     progdir = getprogdir(argv[0], &fullprogname);
   else {
     progdir = NULL;
     fullprogname = NULL;
   }
   datadir = getdatadir();      /* directory to search as last resort */

   checklocale();

   homedir = findhome(&homedir_error);

#ifdef READLINE
#  if RL_READLINE_VERSION > 0x0402
      rl_completion_entry_function = (rl_compentry_func_t *)completeunits;
#  else
      rl_completion_entry_function = (Function *)completeunits;
#  endif
   rl_basic_word_break_characters = " \t+-*/()|^;";
   flags.readline = isatty(0);
   if (flags.readline){
     int file_exists;
     historyfile = personalfile(NULL,HISTORY_FILE,1,&file_exists);
   }
#else
   flags.readline = 0;
#endif

   unitsfiles[0] = 0;

#ifdef _WIN32
   if (!strcmp(homeunitsfile,".units"))
     homeunitsfile = "unitdef.units";
#endif

   pager = getenv("PAGER");
   if (!pager)
     pager = DEFAULTPAGER;

   flags.interactive = processargs(argc, argv, &havestr, &wantstr);

#ifdef READLINE
   if (flags.interactive && flags.readline && historyfile){
     rl_initialize();
     read_history(historyfile);
     init_history_length = history_length;
     init_history_base = history_base;
     atexit(save_history);
   }
#endif

   signal(SIGINT, write_files_sig);
   signal(SIGTERM, write_files_sig);
#ifdef SIGQUIT
   signal(SIGQUIT, SIG_IGN);         /* Ignore QUIT signal sent by Ctrl-\ or Ctrl-4 */
#endif
   if (logfilename) {
     if (!flags.interactive)
       fprintf(stderr,
               "Log file '%s' ignored in non-interactive mode.\n",logfilename);
     else open_logfile();
   }

   if (num_format.format != NULL) {
       /* user has specified the complete format--use it */
       if (parsenumformat())
         exit(EXIT_FAILURE);
   }
   else {
       /* set 'g' or 'e' and significant digits; with 'e', printf(3)
          precision is one less than significant digits
       */
       if (strchr("Ee", num_format.type))
           num_format.precision--;
       setnumformat();
   }

   if (flags.verbose==0)
     deftext = "";

   if (!unitsfiles[0]){
     char *unitsfile;
     unitsfile = findunitsfile(ERRMSG);
     if (!unitsfile)
       exit(EXIT_FAILURE);
     else {
       int file_exists;

       unitsfiles[0] = unitsfile;
       unitsfiles[1] = personalfile(HOME_UNITS_ENV,homeunitsfile,
                                         0, &file_exists);
       unitsfiles[2] = 0;
     }
   }

#ifdef _WIN32
   localemap = findlocalemap(ERRMSG);
   if (localemap)
     remaplocale(localemap);
#endif

   if (flags.listallunits)
     flags.quiet = 1;      /* don't show currency and CPI messages */
   for(unitfileptr=unitsfiles;*unitfileptr;unitfileptr++){
     readerr = readunits(*unitfileptr, stderr, &unitcount, &prefixcount,
                         &funccount, 0);
     if (readerr==E_MEMORY || readerr==E_FILE)
       exit(EXIT_FAILURE);
   }

   setquery(&q);

   if (flags.listallunits) {
     tryallunits(0,0);
     exit(EXIT_SUCCESS);
   }
   if (! flags.quiet)
     printf("%d units, %d prefixes, %d nonlinear units\n\n",
            unitcount, prefixcount,funccount);

   if (flags.unitcheck) {
     checkunits(flags.unitcheck==2 || flags.verbose==2);
     exit(EXIT_SUCCESS);
   }

     /*********************/
     /**  noninteractive **/
     /*********************/
   if (!flags.interactive) {
     replacectrlchars(havestr);
     if (wantstr)
       replacectrlchars(wantstr);
#ifdef SUPPORT_UTF8
     if (strwidth(havestr)<0){
       printf("Error: %s on input\n",invalid_utf8);
       exit(EXIT_FAILURE);
     }
     if (wantstr && strwidth(wantstr)<0){
       printf("Error: %s on input\n",invalid_utf8);
       exit(EXIT_FAILURE);
     }
#endif
     replace_operators(havestr);
     removespaces(havestr);
     if (wantstr) {
       replace_operators(wantstr);
       removespaces(wantstr);
     }
     if ((funcval = fnlookup(havestr))){
       showfuncdefinition(funcval, FUNCTION);
       exit(EXIT_SUCCESS);
     }
     if ((funcval = invfnlookup(havestr))){
       showfuncdefinition(funcval, INVERSE);
       exit(EXIT_SUCCESS);
     }
     if ((alias = aliaslookup(havestr))){
       showunitlistdef(alias);
       exit(EXIT_SUCCESS);
     }
     if (processunit(&have, havestr, NOPOINT))
       exit(EXIT_FAILURE);
     if (flags.showconformable == 1) {
       tryallunits(&have,0);
       exit(EXIT_SUCCESS);
     }
     if (!wantstr){
       showdefinition(havestr,&have);
       exit(EXIT_SUCCESS);
     }
     if (replacealias(&wantstr, 0)) /* the 0 says that we can free wantstr */
       exit(EXIT_FAILURE);
     if ((funcval = fnlookup(wantstr))){
       if (showfunc(havestr, &have, funcval))  /* Clobbers have */
         exit(EXIT_FAILURE);
       else
         exit(EXIT_SUCCESS);
     }
     if (processwant(&want, wantstr, NOPOINT))
       exit(EXIT_FAILURE);
     if (strchr(wantstr, UNITSEPCHAR)){
       if (showunitlist(havestr, &have, wantstr))
         exit(EXIT_FAILURE);
       else
         exit(EXIT_SUCCESS);
     }
     if (showanswer(havestr,&have,wantstr,&want))
       exit(EXIT_FAILURE);
     else
       exit(EXIT_SUCCESS);
   } else {
     /******************/
     /**  interactive **/
     /******************/
     initializeunit(&lastunit);
     for (;;) {
       do {
         fflush(stdout);
         getuser(&havestr,&havestrsize,q.have);
         replace_operators(havestr);
         comment = strip_comment(havestr);
         leading_spaces = strspn(havestr," ");
         removespaces(havestr);
         if (logfile && comment && emptystr(havestr))
           fprintf(logfile, "#%s\n", comment);
       } while (emptystr(havestr)
                || ishelpquery(havestr,0)
                || iscommand(havestr, comment, &q)
                || definevariable(havestr,q.havewidth+leading_spaces)
                || (!fnlookup(havestr)
                    && !invfnlookup(havestr)
                    && !aliaslookup(havestr)
                    && processunit(&have, havestr, q.havewidth+leading_spaces)));
       if (logfile) {
         if (comment)
           fprintf(logfile, "%s%s\t#%s\n", LOGFROM, havestr,comment);
         else
           fprintf(logfile, "%s%s\n", LOGFROM, havestr);
       }
       if ((alias = aliaslookup(havestr))){
         showunitlistdef(alias);
         continue;
       }
       if ((funcval = fnlookup(havestr))){
         showfuncdefinition(funcval, FUNCTION);
         continue;
       }
       if ((funcval = invfnlookup(havestr))){
         showfuncdefinition(funcval, INVERSE);
         continue;
       }
       do {
         int repeat;
         do {
           repeat = 0;
           fflush(stdout);
           getuser(&wantstr,&wantstrsize,q.want);
           replace_operators(wantstr);
           comment = strip_comment(wantstr);
           leading_spaces = strspn(wantstr," ");
           removespaces(wantstr);
           if (logfile && comment && emptystr(wantstr)){
             fprintf(logfile, "#%s\n", comment);
             repeat = 1;
           }
           if (ishelpquery(wantstr, &have)){
             repeat = 1;
             printf("%s%s\n",q.have, havestr);
           }
           else if (iscommand(wantstr, comment, &q))
               repeat = 1;
         } while (repeat);
       } while (replacealias(&wantstr, &wantstrsize)
                || (!fnlookup(wantstr)
                    && processwant(&want, wantstr, q.wantwidth+leading_spaces)));
       if (logfile) {
         fprintf(logfile, "%s", LOGTO);
         tightprint(logfile, wantstr);
         if (comment)
           fprintf(logfile, "\t#%s", comment);
         putc('\n', logfile);
       }
       if (emptystr(wantstr))
         showdefinition(havestr,&have);
       else if (strchr(wantstr, UNITSEPCHAR))
         showunitlist(havestr, &have, wantstr);
       else if ((funcval = fnlookup(wantstr)))
         showfunc(havestr, &have, funcval); /* Clobbers have */
       else {
         showanswer(havestr,&have,wantstr, &want);
         freeunit(&want);
       }
       unitcopy(&lastunit, &have);
       lastunitset=1;
       freeunit(&have);
     }
   }
}

/* NOTES:

mymalloc, growbuffer and tryallunits are the only places with print
statements that should (?) be removed to make a library.  How can
error reporting in these functions (memory allocation errors) be
handled cleanly for a library implementation?

Way to report the reduced form of the two operands of a sum when
they are not conformable.

Way to report the required domain when getting an domain error.

*/
