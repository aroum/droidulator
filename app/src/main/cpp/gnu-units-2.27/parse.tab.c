/* A Bison parser, made by GNU Bison 3.8.2.  */

/* Bison implementation for Yacc-like parsers in C

   Copyright (C) 1984, 1989-1990, 2000-2015, 2018-2021 Free Software Foundation,
   Inc.

   This program is free software: you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation, either version 3 of the License, or
   (at your option) any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program.  If not, see <https://www.gnu.org/licenses/>.  */

/* As a special exception, you may create a larger work that contains
   part or all of the Bison parser skeleton and distribute that work
   under terms of your choice, so long as that work isn't itself a
   parser generator using the skeleton or a modified version thereof
   as a parser skeleton.  Alternatively, if you modify or redistribute
   the parser skeleton itself, you may (at your option) remove this
   special exception, which will cause the skeleton and the resulting
   Bison output files to be licensed under the GNU General Public
   License without this special exception.

   This special exception was added by the Free Software Foundation in
   version 2.2 of Bison.  */

/* C LALR(1) parser skeleton written by Richard Stallman, by
   simplifying the original so-called "semantic" parser.  */

/* DO NOT RELY ON FEATURES THAT ARE NOT DOCUMENTED in the manual,
   especially those whose name start with YY_ or yy_.  They are
   private implementation details that can be changed or removed.  */

/* All symbols defined below should begin with yy or YY, to avoid
   infringing on user name space.  This should be done even for local
   variables, as they might otherwise be expanded by user macros.
   There are some unavoidable exceptions within include files to
   define necessary library symbols; they are noted "INFRINGES ON
   USER NAME SPACE" below.  */

/* Identify Bison output, and Bison version.  */
#define YYBISON 30802

/* Bison version string.  */
#define YYBISON_VERSION "3.8.2"

/* Skeleton name.  */
#define YYSKELETON_NAME "yacc.c"

/* Pure parsers.  */
#define YYPURE 2

/* Push parsers.  */
#define YYPUSH 0

/* Pull parsers.  */
#define YYPULL 1

/* Substitute the type names.  */
#define YYSTYPE         UNITSSTYPE
/* Substitute the variable and function names.  */
#define yyparse         unitsparse
#define yylex           unitslex
#define yyerror         unitserror
#define yydebug         unitsdebug
#define yynerrs         unitsnerrs

/* First part of user prologue.  */
#line 24 "parse.y"

#include<stdio.h>
#include<float.h>
#include "units.h"

struct commtype {
   int location;
   const char *data;
   struct unittype *result;
   int errorcode;
};

static int err;  /* value used by parser to store return values */

/* 
   The CHECK macro aborts parse if an error has occurred.  It optionally
   destroys a variable.  Call with CHECK(0) if no variables need destruction 
   on error. 
*/
 
#define CHECK(var) if (err) { comm->errorcode=err; \
                              if (var) destroyunit(var); \
                              YYABORT; }

union UNITSSTYPE;
int yylex(union UNITSSTYPE *lvalp, struct commtype *comm);
void yyerror(struct commtype *comm, char *);

#define MAXMEM 100
int unitcount=0;    /* Counts the number of units allocated by the parser */

struct function { 
   char *name; 
   double (*func)(double); 
   int type;
};

struct unitarray {
  int count;
  struct unittype *units[MAX_FUNC_PARAMS];
};

#define DIMENSIONLESS 0
#define ANGLEIN 1
#define ANGLEOUT 2
#define NATURAL 3

struct unittype *
getnewunit()
{
  struct unittype *unit;

  if (unitcount>=MAXMEM)
    return 0;
  unit = (struct unittype *) 
    mymalloc(sizeof(struct unittype),"(getnewunit)");
  if (!unit)
    return 0;
  initializeunit(unit);
  unitcount++;
  return unit;
}


void
destroyunit(struct unittype *unit)
{
  freeunit(unit);
  free(unit);
  unitcount--;
}  
 
struct unitarray *
addtoarray(struct unitarray *array, struct unittype *unit)
{
  if (!array){
    array = mymalloc(sizeof(struct unitarray), ("addtoarray"));
    array->count=0;
  }
  if (array->count<MAX_FUNC_PARAMS){
    array->units[array->count]=unit;
    array->count++;
    return array;
  }
  return 0;
}  


struct unittype *
stubfunc(struct unitarray *array)
{ int i;
  //for(i=0;i<array->count;i++) showunit(array->units[i]);
  return array->units[1];
}

  
void
destroyarray(struct unitarray *array)
{
  while(array->count>0){
    array->count--;
    destroyunit(array->units[array->count]);
  }
  free(array);
}


struct unittype *
onearg(struct unitarray *array, int strict)
{
  if (strict && array->count!=1){
    if (strict==1)  
      err=E_TOOMANYARGS;
    else
      err=E_ARGSNOFUNC;
  }
  while(array->count>1){
    array->count--;
    destroyunit(array->units[array->count]);
  }
  array->count=0;
  return array->units[0];
}  

      
struct unittype *
makenumunit(double num,int *myerr)
{
  struct unittype *ret;
  ret=getnewunit();
  if (!ret){
    *myerr = E_PARSEMEM;
    return 0;  
  }
  ret->factor = num;
  *myerr = 0;
  return ret;
}

int
logunit(struct unittype *theunit, int base)
{  
  if ((err=unit2num(theunit)))
    return err;
  if (base==2)
    theunit->factor = log2(theunit->factor);
  else if (base==10)
    theunit->factor = log10(theunit->factor);
  else
    theunit->factor = log(theunit->factor)/log((double)base);
  if (errno)
    return E_FUNC;
  return 0;
}
 
int
funcunit(struct unittype *theunit, struct function const *fun)
{
  struct unittype angleunit;
  if (fun->type==ANGLEIN){
    err=unit2num(theunit);
    if (err==E_NOTANUMBER){
      initializeunit(&angleunit);
      angleunit.denominator[0] = dupstr("radian","(funcunit)");
      angleunit.denominator[1] = 0;
      err = multunit(theunit, &angleunit);
      freeunit(&angleunit);
      if (!err)
        err = unit2num(theunit);
    }
    if (err)
      return err;
  } else if (fun->type==ANGLEOUT || fun->type == DIMENSIONLESS || fun->type == NATURAL) {
    if ((err=unit2num(theunit)))
      return err;
    if (fun->type==NATURAL && (theunit->factor<0 || trunc(theunit->factor)!=theunit->factor))
      return E_NOTINDOMAIN;
  } else 
     return E_BADFUNCTYPE;
  errno = 0;
  theunit->factor = (*(fun->func))(theunit->factor);
  if (errno)
    return E_FUNC;
  if (fun->type==ANGLEOUT) {
    theunit->numerator[0] = dupstr("radian","(funcunit)");
    theunit->numerator[1] = 0;
  }
  return 0;
}



#line 270 "parse.tab.c"

# ifndef YY_CAST
#  ifdef __cplusplus
#   define YY_CAST(Type, Val) static_cast<Type> (Val)
#   define YY_REINTERPRET_CAST(Type, Val) reinterpret_cast<Type> (Val)
#  else
#   define YY_CAST(Type, Val) ((Type) (Val))
#   define YY_REINTERPRET_CAST(Type, Val) ((Type) (Val))
#  endif
# endif
# ifndef YY_NULLPTR
#  if defined __cplusplus
#   if 201103L <= __cplusplus
#    define YY_NULLPTR nullptr
#   else
#    define YY_NULLPTR 0
#   endif
#  else
#   define YY_NULLPTR ((void*)0)
#  endif
# endif


/* Debug traces.  */
#ifndef UNITSDEBUG
# if defined YYDEBUG
#if YYDEBUG
#   define UNITSDEBUG 1
#  else
#   define UNITSDEBUG 0
#  endif
# else /* ! defined YYDEBUG */
#  define UNITSDEBUG 0
# endif /* ! defined YYDEBUG */
#endif  /* ! defined UNITSDEBUG */
#if UNITSDEBUG
extern int unitsdebug;
#endif

/* Token kinds.  */
#ifndef UNITSTOKENTYPE
# define UNITSTOKENTYPE
  enum unitstokentype
  {
    UNITSEMPTY = -2,
    UNITSEOF = 0,                  /* "end of file"  */
    UNITSerror = 256,              /* error  */
    UNITSUNDEF = 257,              /* "invalid token"  */
    REAL = 258,                    /* REAL  */
    UNIT = 259,                    /* UNIT  */
    REALFUNC = 260,                /* REALFUNC  */
    LOG = 261,                     /* LOG  */
    UNITFUNC = 262,                /* UNITFUNC  */
    EXPONENT = 263,                /* EXPONENT  */
    MULTIPLY = 264,                /* MULTIPLY  */
    MULTSTAR = 265,                /* MULTSTAR  */
    DIVIDE = 266,                  /* DIVIDE  */
    NUMDIV = 267,                  /* NUMDIV  */
    SQRT = 268,                    /* SQRT  */
    CUBEROOT = 269,                /* CUBEROOT  */
    MULTMINUS = 270,               /* MULTMINUS  */
    EOL = 271,                     /* EOL  */
    FUNCINV = 272,                 /* FUNCINV  */
    MEMERROR = 273,                /* MEMERROR  */
    BADNUMBER = 274,               /* BADNUMBER  */
    NUMOVERFLOW = 275,             /* NUMOVERFLOW  */
    NUMUNDERFLOW = 276,            /* NUMUNDERFLOW  */
    UNITEND = 277,                 /* UNITEND  */
    LASTUNSET = 278,               /* LASTUNSET  */
    LT = 279,                      /* LT  */
    GT = 280,                      /* GT  */
    EQ = 281,                      /* EQ  */
    GTE = 282,                     /* GTE  */
    LTE = 283,                     /* LTE  */
    ADD = 284,                     /* ADD  */
    MINUS = 285,                   /* MINUS  */
    UNARY = 286                    /* UNARY  */
  };
  typedef enum unitstokentype unitstoken_kind_t;
#endif

/* Value type.  */
#if ! defined UNITSSTYPE && ! defined UNITSSTYPE_IS_DECLARED
union UNITSSTYPE
{
#line 222 "parse.y"

  double number;
  int integer;
  struct unittype *unit;
  struct function *realfunc;
  struct func *unitfunc;
  struct unitarray *unitarray;

#line 365 "parse.tab.c"

};
typedef union UNITSSTYPE UNITSSTYPE;
# define UNITSSTYPE_IS_TRIVIAL 1
# define UNITSSTYPE_IS_DECLARED 1
#endif




int unitsparse (struct commtype *comm);



/* Symbol kind.  */
enum yysymbol_kind_t
{
  YYSYMBOL_YYEMPTY = -2,
  YYSYMBOL_YYEOF = 0,                      /* "end of file"  */
  YYSYMBOL_YYerror = 1,                    /* error  */
  YYSYMBOL_YYUNDEF = 2,                    /* "invalid token"  */
  YYSYMBOL_REAL = 3,                       /* REAL  */
  YYSYMBOL_UNIT = 4,                       /* UNIT  */
  YYSYMBOL_REALFUNC = 5,                   /* REALFUNC  */
  YYSYMBOL_LOG = 6,                        /* LOG  */
  YYSYMBOL_UNITFUNC = 7,                   /* UNITFUNC  */
  YYSYMBOL_EXPONENT = 8,                   /* EXPONENT  */
  YYSYMBOL_MULTIPLY = 9,                   /* MULTIPLY  */
  YYSYMBOL_MULTSTAR = 10,                  /* MULTSTAR  */
  YYSYMBOL_DIVIDE = 11,                    /* DIVIDE  */
  YYSYMBOL_NUMDIV = 12,                    /* NUMDIV  */
  YYSYMBOL_SQRT = 13,                      /* SQRT  */
  YYSYMBOL_CUBEROOT = 14,                  /* CUBEROOT  */
  YYSYMBOL_MULTMINUS = 15,                 /* MULTMINUS  */
  YYSYMBOL_EOL = 16,                       /* EOL  */
  YYSYMBOL_FUNCINV = 17,                   /* FUNCINV  */
  YYSYMBOL_MEMERROR = 18,                  /* MEMERROR  */
  YYSYMBOL_BADNUMBER = 19,                 /* BADNUMBER  */
  YYSYMBOL_NUMOVERFLOW = 20,               /* NUMOVERFLOW  */
  YYSYMBOL_NUMUNDERFLOW = 21,              /* NUMUNDERFLOW  */
  YYSYMBOL_UNITEND = 22,                   /* UNITEND  */
  YYSYMBOL_LASTUNSET = 23,                 /* LASTUNSET  */
  YYSYMBOL_LT = 24,                        /* LT  */
  YYSYMBOL_GT = 25,                        /* GT  */
  YYSYMBOL_EQ = 26,                        /* EQ  */
  YYSYMBOL_GTE = 27,                       /* GTE  */
  YYSYMBOL_LTE = 28,                       /* LTE  */
  YYSYMBOL_ADD = 29,                       /* ADD  */
  YYSYMBOL_MINUS = 30,                     /* MINUS  */
  YYSYMBOL_UNARY = 31,                     /* UNARY  */
  YYSYMBOL_32_ = 32,                       /* '('  */
  YYSYMBOL_33_ = 33,                       /* ','  */
  YYSYMBOL_34_ = 34,                       /* ')'  */
  YYSYMBOL_YYACCEPT = 35,                  /* $accept  */
  YYSYMBOL_input = 36,                     /* input  */
  YYSYMBOL_unitexpr = 37,                  /* unitexpr  */
  YYSYMBOL_divlist = 38,                   /* divlist  */
  YYSYMBOL_expr = 39,                      /* expr  */
  YYSYMBOL_numexpr = 40,                   /* numexpr  */
  YYSYMBOL_unitarray = 41,                 /* unitarray  */
  YYSYMBOL_pexpr = 42,                     /* pexpr  */
  YYSYMBOL_list = 43                       /* list  */
};
typedef enum yysymbol_kind_t yysymbol_kind_t;




#ifdef short
# undef short
#endif

/* On compilers that do not define __PTRDIFF_MAX__ etc., make sure
   <limits.h> and (if available) <stdint.h> are included
   so that the code can choose integer types of a good width.  */

#ifndef __PTRDIFF_MAX__
# include <limits.h> /* INFRINGES ON USER NAME SPACE */
# if defined __STDC_VERSION__ && 199901 <= __STDC_VERSION__
#  include <stdint.h> /* INFRINGES ON USER NAME SPACE */
#  define YY_STDINT_H
# endif
#endif

/* Narrow types that promote to a signed type and that can represent a
   signed or unsigned integer of at least N bits.  In tables they can
   save space and decrease cache pressure.  Promoting to a signed type
   helps avoid bugs in integer arithmetic.  */

#ifdef __INT_LEAST8_MAX__
typedef __INT_LEAST8_TYPE__ yytype_int8;
#elif defined YY_STDINT_H
typedef int_least8_t yytype_int8;
#else
typedef signed char yytype_int8;
#endif

#ifdef __INT_LEAST16_MAX__
typedef __INT_LEAST16_TYPE__ yytype_int16;
#elif defined YY_STDINT_H
typedef int_least16_t yytype_int16;
#else
typedef short yytype_int16;
#endif

/* Work around bug in HP-UX 11.23, which defines these macros
   incorrectly for preprocessor constants.  This workaround can likely
   be removed in 2023, as HPE has promised support for HP-UX 11.23
   (aka HP-UX 11i v2) only through the end of 2022; see Table 2 of
   <https://h20195.www2.hpe.com/V2/getpdf.aspx/4AA4-7673ENW.pdf>.  */
#ifdef __hpux
# undef UINT_LEAST8_MAX
# undef UINT_LEAST16_MAX
# define UINT_LEAST8_MAX 255
# define UINT_LEAST16_MAX 65535
#endif

#if defined __UINT_LEAST8_MAX__ && __UINT_LEAST8_MAX__ <= __INT_MAX__
typedef __UINT_LEAST8_TYPE__ yytype_uint8;
#elif (!defined __UINT_LEAST8_MAX__ && defined YY_STDINT_H \
       && UINT_LEAST8_MAX <= INT_MAX)
typedef uint_least8_t yytype_uint8;
#elif !defined __UINT_LEAST8_MAX__ && UCHAR_MAX <= INT_MAX
typedef unsigned char yytype_uint8;
#else
typedef short yytype_uint8;
#endif

#if defined __UINT_LEAST16_MAX__ && __UINT_LEAST16_MAX__ <= __INT_MAX__
typedef __UINT_LEAST16_TYPE__ yytype_uint16;
#elif (!defined __UINT_LEAST16_MAX__ && defined YY_STDINT_H \
       && UINT_LEAST16_MAX <= INT_MAX)
typedef uint_least16_t yytype_uint16;
#elif !defined __UINT_LEAST16_MAX__ && USHRT_MAX <= INT_MAX
typedef unsigned short yytype_uint16;
#else
typedef int yytype_uint16;
#endif

#ifndef YYPTRDIFF_T
# if defined __PTRDIFF_TYPE__ && defined __PTRDIFF_MAX__
#  define YYPTRDIFF_T __PTRDIFF_TYPE__
#  define YYPTRDIFF_MAXIMUM __PTRDIFF_MAX__
# elif defined PTRDIFF_MAX
#  ifndef ptrdiff_t
#   include <stddef.h> /* INFRINGES ON USER NAME SPACE */
#  endif
#  define YYPTRDIFF_T ptrdiff_t
#  define YYPTRDIFF_MAXIMUM PTRDIFF_MAX
# else
#  define YYPTRDIFF_T long
#  define YYPTRDIFF_MAXIMUM LONG_MAX
# endif
#endif

#ifndef YYSIZE_T
# ifdef __SIZE_TYPE__
#  define YYSIZE_T __SIZE_TYPE__
# elif defined size_t
#  define YYSIZE_T size_t
# elif defined __STDC_VERSION__ && 199901 <= __STDC_VERSION__
#  include <stddef.h> /* INFRINGES ON USER NAME SPACE */
#  define YYSIZE_T size_t
# else
#  define YYSIZE_T unsigned
# endif
#endif

#define YYSIZE_MAXIMUM                                  \
  YY_CAST (YYPTRDIFF_T,                                 \
           (YYPTRDIFF_MAXIMUM < YY_CAST (YYSIZE_T, -1)  \
            ? YYPTRDIFF_MAXIMUM                         \
            : YY_CAST (YYSIZE_T, -1)))

#define YYSIZEOF(X) YY_CAST (YYPTRDIFF_T, sizeof (X))


/* Stored state numbers (used for stacks). */
typedef yytype_int8 yy_state_t;

/* State numbers in computations.  */
typedef int yy_state_fast_t;

#ifndef YY_
# if defined YYENABLE_NLS && YYENABLE_NLS
#  if ENABLE_NLS
#   include <libintl.h> /* INFRINGES ON USER NAME SPACE */
#   define YY_(Msgid) dgettext ("bison-runtime", Msgid)
#  endif
# endif
# ifndef YY_
#  define YY_(Msgid) Msgid
# endif
#endif


#ifndef YY_ATTRIBUTE_PURE
# if defined __GNUC__ && 2 < __GNUC__ + (96 <= __GNUC_MINOR__)
#  define YY_ATTRIBUTE_PURE __attribute__ ((__pure__))
# else
#  define YY_ATTRIBUTE_PURE
# endif
#endif

#ifndef YY_ATTRIBUTE_UNUSED
# if defined __GNUC__ && 2 < __GNUC__ + (7 <= __GNUC_MINOR__)
#  define YY_ATTRIBUTE_UNUSED __attribute__ ((__unused__))
# else
#  define YY_ATTRIBUTE_UNUSED
# endif
#endif

/* Suppress unused-variable warnings by "using" E.  */
#if ! defined lint || defined __GNUC__
# define YY_USE(E) ((void) (E))
#else
# define YY_USE(E) /* empty */
#endif

/* Suppress an incorrect diagnostic about yylval being uninitialized.  */
#if defined __GNUC__ && ! defined __ICC && 406 <= __GNUC__ * 100 + __GNUC_MINOR__
# if __GNUC__ * 100 + __GNUC_MINOR__ < 407
#  define YY_IGNORE_MAYBE_UNINITIALIZED_BEGIN                           \
    _Pragma ("GCC diagnostic push")                                     \
    _Pragma ("GCC diagnostic ignored \"-Wuninitialized\"")
# else
#  define YY_IGNORE_MAYBE_UNINITIALIZED_BEGIN                           \
    _Pragma ("GCC diagnostic push")                                     \
    _Pragma ("GCC diagnostic ignored \"-Wuninitialized\"")              \
    _Pragma ("GCC diagnostic ignored \"-Wmaybe-uninitialized\"")
# endif
# define YY_IGNORE_MAYBE_UNINITIALIZED_END      \
    _Pragma ("GCC diagnostic pop")
#else
# define YY_INITIAL_VALUE(Value) Value
#endif
#ifndef YY_IGNORE_MAYBE_UNINITIALIZED_BEGIN
# define YY_IGNORE_MAYBE_UNINITIALIZED_BEGIN
# define YY_IGNORE_MAYBE_UNINITIALIZED_END
#endif
#ifndef YY_INITIAL_VALUE
# define YY_INITIAL_VALUE(Value) /* Nothing. */
#endif

#if defined __cplusplus && defined __GNUC__ && ! defined __ICC && 6 <= __GNUC__
# define YY_IGNORE_USELESS_CAST_BEGIN                          \
    _Pragma ("GCC diagnostic push")                            \
    _Pragma ("GCC diagnostic ignored \"-Wuseless-cast\"")
# define YY_IGNORE_USELESS_CAST_END            \
    _Pragma ("GCC diagnostic pop")
#endif
#ifndef YY_IGNORE_USELESS_CAST_BEGIN
# define YY_IGNORE_USELESS_CAST_BEGIN
# define YY_IGNORE_USELESS_CAST_END
#endif


#define YY_ASSERT(E) ((void) (0 && (E)))

#if !defined yyoverflow

/* The parser invokes alloca or malloc; define the necessary symbols.  */

# ifdef YYSTACK_USE_ALLOCA
#  if YYSTACK_USE_ALLOCA
#   ifdef __GNUC__
#    define YYSTACK_ALLOC __builtin_alloca
#   elif defined __BUILTIN_VA_ARG_INCR
#    include <alloca.h> /* INFRINGES ON USER NAME SPACE */
#   elif defined _AIX
#    define YYSTACK_ALLOC __alloca
#   elif defined _MSC_VER
#    include <malloc.h> /* INFRINGES ON USER NAME SPACE */
#    define alloca _alloca
#   else
#    define YYSTACK_ALLOC alloca
#    if ! defined _ALLOCA_H && ! defined EXIT_SUCCESS
#     include <stdlib.h> /* INFRINGES ON USER NAME SPACE */
      /* Use EXIT_SUCCESS as a witness for stdlib.h.  */
#     ifndef EXIT_SUCCESS
#      define EXIT_SUCCESS 0
#     endif
#    endif
#   endif
#  endif
# endif

# ifdef YYSTACK_ALLOC
   /* Pacify GCC's 'empty if-body' warning.  */
#  define YYSTACK_FREE(Ptr) do { /* empty */; } while (0)
#  ifndef YYSTACK_ALLOC_MAXIMUM
    /* The OS might guarantee only one guard page at the bottom of the stack,
       and a page size can be as small as 4096 bytes.  So we cannot safely
       invoke alloca (N) if N exceeds 4096.  Use a slightly smaller number
       to allow for a few compiler-allocated temporary stack slots.  */
#   define YYSTACK_ALLOC_MAXIMUM 4032 /* reasonable circa 2006 */
#  endif
# else
#  define YYSTACK_ALLOC YYMALLOC
#  define YYSTACK_FREE YYFREE
#  ifndef YYSTACK_ALLOC_MAXIMUM
#   define YYSTACK_ALLOC_MAXIMUM YYSIZE_MAXIMUM
#  endif
#  if (defined __cplusplus && ! defined EXIT_SUCCESS \
       && ! ((defined YYMALLOC || defined malloc) \
             && (defined YYFREE || defined free)))
#   include <stdlib.h> /* INFRINGES ON USER NAME SPACE */
#   ifndef EXIT_SUCCESS
#    define EXIT_SUCCESS 0
#   endif
#  endif
#  ifndef YYMALLOC
#   define YYMALLOC malloc
#   if ! defined malloc && ! defined EXIT_SUCCESS
void *malloc (YYSIZE_T); /* INFRINGES ON USER NAME SPACE */
#   endif
#  endif
#  ifndef YYFREE
#   define YYFREE free
#   if ! defined free && ! defined EXIT_SUCCESS
void free (void *); /* INFRINGES ON USER NAME SPACE */
#   endif
#  endif
# endif
#endif /* !defined yyoverflow */

#if (! defined yyoverflow \
     && (! defined __cplusplus \
         || (defined UNITSSTYPE_IS_TRIVIAL && UNITSSTYPE_IS_TRIVIAL)))

/* A type that is properly aligned for any stack member.  */
union yyalloc
{
  yy_state_t yyss_alloc;
  YYSTYPE yyvs_alloc;
};

/* The size of the maximum gap between one aligned stack and the next.  */
# define YYSTACK_GAP_MAXIMUM (YYSIZEOF (union yyalloc) - 1)

/* The size of an array large to enough to hold all stacks, each with
   N elements.  */
# define YYSTACK_BYTES(N) \
     ((N) * (YYSIZEOF (yy_state_t) + YYSIZEOF (YYSTYPE)) \
      + YYSTACK_GAP_MAXIMUM)

# define YYCOPY_NEEDED 1

/* Relocate STACK from its old location to the new one.  The
   local variables YYSIZE and YYSTACKSIZE give the old and new number of
   elements in the stack, and YYPTR gives the new location of the
   stack.  Advance YYPTR to a properly aligned location for the next
   stack.  */
# define YYSTACK_RELOCATE(Stack_alloc, Stack)                           \
    do                                                                  \
      {                                                                 \
        YYPTRDIFF_T yynewbytes;                                         \
        YYCOPY (&yyptr->Stack_alloc, Stack, yysize);                    \
        Stack = &yyptr->Stack_alloc;                                    \
        yynewbytes = yystacksize * YYSIZEOF (*Stack) + YYSTACK_GAP_MAXIMUM; \
        yyptr += yynewbytes / YYSIZEOF (*yyptr);                        \
      }                                                                 \
    while (0)

#endif

#if defined YYCOPY_NEEDED && YYCOPY_NEEDED
/* Copy COUNT objects from SRC to DST.  The source and destination do
   not overlap.  */
# ifndef YYCOPY
#  if defined __GNUC__ && 1 < __GNUC__
#   define YYCOPY(Dst, Src, Count) \
      __builtin_memcpy (Dst, Src, YY_CAST (YYSIZE_T, (Count)) * sizeof (*(Src)))
#  else
#   define YYCOPY(Dst, Src, Count)              \
      do                                        \
        {                                       \
          YYPTRDIFF_T yyi;                      \
          for (yyi = 0; yyi < (Count); yyi++)   \
            (Dst)[yyi] = (Src)[yyi];            \
        }                                       \
      while (0)
#  endif
# endif
#endif /* !YYCOPY_NEEDED */

/* YYFINAL -- State number of the termination state.  */
#define YYFINAL  40
/* YYLAST -- Last index in YYTABLE.  */
#define YYLAST   190

/* YYNTOKENS -- Number of terminals.  */
#define YYNTOKENS  35
/* YYNNTS -- Number of nonterminals.  */
#define YYNNTS  9
/* YYNRULES -- Number of rules.  */
#define YYNRULES  42
/* YYNSTATES -- Number of states.  */
#define YYNSTATES  68

/* YYMAXUTOK -- Last valid token kind.  */
#define YYMAXUTOK   286


/* YYTRANSLATE(TOKEN-NUM) -- Symbol number corresponding to TOKEN-NUM
   as returned by yylex, with out-of-bounds checking.  */
#define YYTRANSLATE(YYX)                                \
  (0 <= (YYX) && (YYX) <= YYMAXUTOK                     \
   ? YY_CAST (yysymbol_kind_t, yytranslate[YYX])        \
   : YYSYMBOL_YYUNDEF)

/* YYTRANSLATE[TOKEN-NUM] -- Symbol number corresponding to TOKEN-NUM
   as returned by yylex.  */
static const yytype_int8 yytranslate[] =
{
       0,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
      32,    34,     2,     2,    33,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     1,     2,     3,     4,
       5,     6,     7,     8,     9,    10,    11,    12,    13,    14,
      15,    16,    17,    18,    19,    20,    21,    22,    23,    24,
      25,    26,    27,    28,    29,    30,    31
};

#if UNITSDEBUG
/* YYRLINE[YYN] -- Source line where rule number YYN was defined.  */
static const yytype_int16 yyrline[] =
{
       0,   275,   275,   277,   278,   281,   282,   285,   286,   290,
     291,   292,   307,   309,   312,   314,   316,   320,   321,   324,
     325,   328,   334,   335,   336,   338,   340,   342,   343,   344,
     345,   346,   347,   349,   351,   354,   357,   358,   359,   360,
     361,   362,   363
};
#endif

/** Accessing symbol of state STATE.  */
#define YY_ACCESSING_SYMBOL(State) YY_CAST (yysymbol_kind_t, yystos[State])

#if UNITSDEBUG || 0
/* The user-facing name of the symbol whose (internal) number is
   YYSYMBOL.  No bounds checking.  */
static const char *yysymbol_name (yysymbol_kind_t yysymbol) YY_ATTRIBUTE_UNUSED;

/* YYTNAME[SYMBOL-NUM] -- String name of the symbol SYMBOL-NUM.
   First, the terminals, then, starting at YYNTOKENS, nonterminals.  */
static const char *const yytname[] =
{
  "\"end of file\"", "error", "\"invalid token\"", "REAL", "UNIT",
  "REALFUNC", "LOG", "UNITFUNC", "EXPONENT", "MULTIPLY", "MULTSTAR",
  "DIVIDE", "NUMDIV", "SQRT", "CUBEROOT", "MULTMINUS", "EOL", "FUNCINV",
  "MEMERROR", "BADNUMBER", "NUMOVERFLOW", "NUMUNDERFLOW", "UNITEND",
  "LASTUNSET", "LT", "GT", "EQ", "GTE", "LTE", "ADD", "MINUS", "UNARY",
  "'('", "','", "')'", "$accept", "input", "unitexpr", "divlist", "expr",
  "numexpr", "unitarray", "pexpr", "list", YY_NULLPTR
};

static const char *
yysymbol_name (yysymbol_kind_t yysymbol)
{
  return yytname[yysymbol];
}
#endif

#define YYPACT_NINF (-27)

#define yypact_value_is_default(Yyn) \
  ((Yyn) == YYPACT_NINF)

#define YYTABLE_NINF (-1)

#define yytable_value_is_error(Yyn) \
  0

/* YYPACT[STATE-NUM] -- Index in YYTABLE of the portion describing
   STATE-NUM.  */
static const yytype_int16 yypact[] =
{
      53,   -27,   -27,   -27,   -26,   -26,   -26,   158,   -26,   -26,
     158,   -27,     1,   -27,   -27,   -27,   -27,   -27,   -27,   158,
      74,     9,    -2,     6,    -7,    17,   -27,    95,   -27,   -27,
     -27,    95,   -27,   -27,    95,   -27,   -26,    95,    -7,   -18,
     -27,   -27,   -27,    74,    74,    74,    74,    74,    15,   116,
     158,   137,   -27,    74,   -27,   -27,    12,    12,     2,     2,
     -27,   158,   158,    22,   137,    -7,    22,    22
};

/* YYDEFACT[STATE-NUM] -- Default reduction number in state STATE-NUM.
   Performed when YYTABLE does not specify something else to do.  Zero
   means the default is an error.  */
static const yytype_int8 yydefact[] =
{
       0,     4,    17,    23,     0,     0,     0,     0,     0,     0,
       0,     2,     0,    39,    36,    37,    38,    40,    41,     0,
       0,     0,     0,     6,     5,    22,    27,     9,    30,    31,
      32,     7,    28,    29,    10,    42,     0,    11,    19,     0,
       1,     3,     8,     0,     0,     0,     0,     0,     0,     0,
       0,    26,    33,     0,    21,    15,    16,    14,    12,    13,
      18,     0,     0,    24,    25,    20,    34,    35
};

/* YYPGOTO[NTERM-NUM].  */
static const yytype_int8 yypgoto[] =
{
     -27,   -27,   -27,    37,   -19,   -10,   -27,    27,     0
};

/* YYDEFGOTO[NTERM-NUM].  */
static const yytype_int8 yydefgoto[] =
{
       0,    21,    22,    42,    24,    25,    39,    26,    51
};

/* YYTABLE[YYPACT[STATE-NUM]] -- What to do in state STATE-NUM.  If
   positive, shift that token.  If negative, reduce the rule whose
   number is the opposite.  If YYTABLE_NINF, syntax error.  */
static const yytype_int8 yytable[] =
{
      27,    38,    43,    44,    45,    35,    20,    31,    36,    40,
      34,    43,    44,    45,    41,    53,    54,     7,     2,    37,
      27,    43,    46,    47,    55,    56,    57,    58,    59,    48,
      49,    28,    29,    30,    65,    32,    33,    23,    60,     0,
       0,     0,     0,    27,    27,    27,    27,    27,     0,    63,
      64,     0,     0,    27,     1,     0,     2,     3,     4,     5,
       6,    66,    67,    52,     7,     0,     8,     9,    10,    11,
      12,    13,    14,    15,    16,    17,    18,     2,     3,     4,
       5,     6,     0,    19,     0,    20,     0,     8,     9,    10,
       0,    12,    13,    14,    15,    16,    17,    18,     2,     3,
       4,     5,     6,    49,    19,     0,    20,     0,     8,     9,
      50,     0,    12,    13,    14,    15,    16,    17,    18,     2,
       3,     4,     5,     6,     0,     0,     0,    20,     0,     8,
       9,    61,     0,    12,    13,    14,    15,    16,    17,    18,
       2,     3,     4,     5,     6,    49,    62,     0,    20,     0,
       8,     9,     0,     0,    12,    13,    14,    15,    16,    17,
      18,     2,     3,     4,     5,     6,     0,     0,     0,    20,
       0,     8,     9,     0,     0,    12,    13,    14,    15,    16,
      17,    18,     0,     0,     0,     0,     0,     0,     0,     0,
      20
};

static const yytype_int8 yycheck[] =
{
       0,    20,     9,    10,    11,     4,    32,     7,     7,     0,
      10,     9,    10,    11,    16,    33,    34,    11,     3,    19,
      20,     9,    29,    30,    43,    44,    45,    46,    47,    12,
       8,     4,     5,     6,    53,     8,     9,     0,    48,    -1,
      -1,    -1,    -1,    43,    44,    45,    46,    47,    -1,    49,
      50,    -1,    -1,    53,     1,    -1,     3,     4,     5,     6,
       7,    61,    62,    36,    11,    -1,    13,    14,    15,    16,
      17,    18,    19,    20,    21,    22,    23,     3,     4,     5,
       6,     7,    -1,    30,    -1,    32,    -1,    13,    14,    15,
      -1,    17,    18,    19,    20,    21,    22,    23,     3,     4,
       5,     6,     7,     8,    30,    -1,    32,    -1,    13,    14,
      15,    -1,    17,    18,    19,    20,    21,    22,    23,     3,
       4,     5,     6,     7,    -1,    -1,    -1,    32,    -1,    13,
      14,    15,    -1,    17,    18,    19,    20,    21,    22,    23,
       3,     4,     5,     6,     7,     8,    30,    -1,    32,    -1,
      13,    14,    -1,    -1,    17,    18,    19,    20,    21,    22,
      23,     3,     4,     5,     6,     7,    -1,    -1,    -1,    32,
      -1,    13,    14,    -1,    -1,    17,    18,    19,    20,    21,
      22,    23,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      32
};

/* YYSTOS[STATE-NUM] -- The symbol kind of the accessing symbol of
   state STATE-NUM.  */
static const yytype_int8 yystos[] =
{
       0,     1,     3,     4,     5,     6,     7,    11,    13,    14,
      15,    16,    17,    18,    19,    20,    21,    22,    23,    30,
      32,    36,    37,    38,    39,    40,    42,    43,    42,    42,
      42,    43,    42,    42,    43,     4,     7,    43,    39,    41,
       0,    16,    38,     9,    10,    11,    29,    30,    12,     8,
      15,    43,    42,    33,    34,    39,    39,    39,    39,    39,
      40,    15,    30,    43,    43,    39,    43,    43
};

/* YYR1[RULE-NUM] -- Symbol kind of the left-hand side of rule RULE-NUM.  */
static const yytype_int8 yyr1[] =
{
       0,    35,    36,    36,    36,    37,    37,    38,    38,    39,
      39,    39,    39,    39,    39,    39,    39,    40,    40,    41,
      41,    42,    43,    43,    43,    43,    43,    43,    43,    43,
      43,    43,    43,    43,    43,    43,    43,    43,    43,    43,
      43,    43,    43
};

/* YYR2[RULE-NUM] -- Number of symbols on the right-hand side of rule RULE-NUM.  */
static const yytype_int8 yyr2[] =
{
       0,     2,     1,     2,     1,     1,     1,     2,     2,     1,
       2,     2,     3,     3,     3,     3,     3,     1,     3,     1,
       3,     3,     1,     1,     3,     3,     2,     1,     2,     2,
       2,     2,     2,     3,     4,     4,     1,     1,     1,     1,
       1,     1,     2
};


enum { YYENOMEM = -2 };

#define yyerrok         (yyerrstatus = 0)
#define yyclearin       (yychar = UNITSEMPTY)

#define YYACCEPT        goto yyacceptlab
#define YYABORT         goto yyabortlab
#define YYERROR         goto yyerrorlab
#define YYNOMEM         goto yyexhaustedlab


#define YYRECOVERING()  (!!yyerrstatus)

#define YYBACKUP(Token, Value)                                    \
  do                                                              \
    if (yychar == UNITSEMPTY)                                        \
      {                                                           \
        yychar = (Token);                                         \
        yylval = (Value);                                         \
        YYPOPSTACK (yylen);                                       \
        yystate = *yyssp;                                         \
        goto yybackup;                                            \
      }                                                           \
    else                                                          \
      {                                                           \
        yyerror (comm, YY_("syntax error: cannot back up")); \
        YYERROR;                                                  \
      }                                                           \
  while (0)

/* Backward compatibility with an undocumented macro.
   Use UNITSerror or UNITSUNDEF. */
#define YYERRCODE UNITSUNDEF


/* Enable debugging if requested.  */
#if UNITSDEBUG

# ifndef YYFPRINTF
#  include <stdio.h> /* INFRINGES ON USER NAME SPACE */
#  define YYFPRINTF fprintf
# endif

# define YYDPRINTF(Args)                        \
do {                                            \
  if (yydebug)                                  \
    YYFPRINTF Args;                             \
} while (0)




# define YY_SYMBOL_PRINT(Title, Kind, Value, Location)                    \
do {                                                                      \
  if (yydebug)                                                            \
    {                                                                     \
      YYFPRINTF (stderr, "%s ", Title);                                   \
      yy_symbol_print (stderr,                                            \
                  Kind, Value, comm); \
      YYFPRINTF (stderr, "\n");                                           \
    }                                                                     \
} while (0)


/*-----------------------------------.
| Print this symbol's value on YYO.  |
`-----------------------------------*/

static void
yy_symbol_value_print (FILE *yyo,
                       yysymbol_kind_t yykind, YYSTYPE const * const yyvaluep, struct commtype *comm)
{
  FILE *yyoutput = yyo;
  YY_USE (yyoutput);
  YY_USE (comm);
  if (!yyvaluep)
    return;
  YY_IGNORE_MAYBE_UNINITIALIZED_BEGIN
  YY_USE (yykind);
  YY_IGNORE_MAYBE_UNINITIALIZED_END
}


/*---------------------------.
| Print this symbol on YYO.  |
`---------------------------*/

static void
yy_symbol_print (FILE *yyo,
                 yysymbol_kind_t yykind, YYSTYPE const * const yyvaluep, struct commtype *comm)
{
  YYFPRINTF (yyo, "%s %s (",
             yykind < YYNTOKENS ? "token" : "nterm", yysymbol_name (yykind));

  yy_symbol_value_print (yyo, yykind, yyvaluep, comm);
  YYFPRINTF (yyo, ")");
}

/*------------------------------------------------------------------.
| yy_stack_print -- Print the state stack from its BOTTOM up to its |
| TOP (included).                                                   |
`------------------------------------------------------------------*/

static void
yy_stack_print (yy_state_t *yybottom, yy_state_t *yytop)
{
  YYFPRINTF (stderr, "Stack now");
  for (; yybottom <= yytop; yybottom++)
    {
      int yybot = *yybottom;
      YYFPRINTF (stderr, " %d", yybot);
    }
  YYFPRINTF (stderr, "\n");
}

# define YY_STACK_PRINT(Bottom, Top)                            \
do {                                                            \
  if (yydebug)                                                  \
    yy_stack_print ((Bottom), (Top));                           \
} while (0)


/*------------------------------------------------.
| Report that the YYRULE is going to be reduced.  |
`------------------------------------------------*/

static void
yy_reduce_print (yy_state_t *yyssp, YYSTYPE *yyvsp,
                 int yyrule, struct commtype *comm)
{
  int yylno = yyrline[yyrule];
  int yynrhs = yyr2[yyrule];
  int yyi;
  YYFPRINTF (stderr, "Reducing stack by rule %d (line %d):\n",
             yyrule - 1, yylno);
  /* The symbols being reduced.  */
  for (yyi = 0; yyi < yynrhs; yyi++)
    {
      YYFPRINTF (stderr, "   $%d = ", yyi + 1);
      yy_symbol_print (stderr,
                       YY_ACCESSING_SYMBOL (+yyssp[yyi + 1 - yynrhs]),
                       &yyvsp[(yyi + 1) - (yynrhs)], comm);
      YYFPRINTF (stderr, "\n");
    }
}

# define YY_REDUCE_PRINT(Rule)          \
do {                                    \
  if (yydebug)                          \
    yy_reduce_print (yyssp, yyvsp, Rule, comm); \
} while (0)

/* Nonzero means print parse trace.  It is left uninitialized so that
   multiple parsers can coexist.  */
int yydebug;
#else /* !UNITSDEBUG */
# define YYDPRINTF(Args) ((void) 0)
# define YY_SYMBOL_PRINT(Title, Kind, Value, Location)
# define YY_STACK_PRINT(Bottom, Top)
# define YY_REDUCE_PRINT(Rule)
#endif /* !UNITSDEBUG */


/* YYINITDEPTH -- initial size of the parser's stacks.  */
#ifndef YYINITDEPTH
# define YYINITDEPTH 200
#endif

/* YYMAXDEPTH -- maximum size the stacks can grow to (effective only
   if the built-in stack extension method is used).

   Do not make this value too large; the results are undefined if
   YYSTACK_ALLOC_MAXIMUM < YYSTACK_BYTES (YYMAXDEPTH)
   evaluated with infinite-precision integer arithmetic.  */

#ifndef YYMAXDEPTH
# define YYMAXDEPTH 10000
#endif






/*-----------------------------------------------.
| Release the memory associated to this symbol.  |
`-----------------------------------------------*/

static void
yydestruct (const char *yymsg,
            yysymbol_kind_t yykind, YYSTYPE *yyvaluep, struct commtype *comm)
{
  YY_USE (yyvaluep);
  YY_USE (comm);
  if (!yymsg)
    yymsg = "Deleting";
  YY_SYMBOL_PRINT (yymsg, yykind, yyvaluep, yylocationp);

  YY_IGNORE_MAYBE_UNINITIALIZED_BEGIN
  switch (yykind)
    {
    case YYSYMBOL_UNIT: /* UNIT  */
#line 261 "parse.y"
            { destroyunit(((*yyvaluep).unit));}
#line 1190 "parse.tab.c"
        break;

    case YYSYMBOL_unitexpr: /* unitexpr  */
#line 261 "parse.y"
            { destroyunit(((*yyvaluep).unit));}
#line 1196 "parse.tab.c"
        break;

    case YYSYMBOL_divlist: /* divlist  */
#line 261 "parse.y"
            { destroyunit(((*yyvaluep).unit));}
#line 1202 "parse.tab.c"
        break;

    case YYSYMBOL_expr: /* expr  */
#line 261 "parse.y"
            { destroyunit(((*yyvaluep).unit));}
#line 1208 "parse.tab.c"
        break;

    case YYSYMBOL_unitarray: /* unitarray  */
#line 262 "parse.y"
            { destroyarray(((*yyvaluep).unitarray));}
#line 1214 "parse.tab.c"
        break;

    case YYSYMBOL_pexpr: /* pexpr  */
#line 262 "parse.y"
            { destroyarray(((*yyvaluep).unitarray));}
#line 1220 "parse.tab.c"
        break;

    case YYSYMBOL_list: /* list  */
#line 261 "parse.y"
            { destroyunit(((*yyvaluep).unit));}
#line 1226 "parse.tab.c"
        break;

      default:
        break;
    }
  YY_IGNORE_MAYBE_UNINITIALIZED_END
}






/*----------.
| yyparse.  |
`----------*/

int
yyparse (struct commtype *comm)
{
/* Lookahead token kind.  */
int yychar;


/* The semantic value of the lookahead symbol.  */
/* Default value used for initialization, for pacifying older GCCs
   or non-GCC compilers.  */
YY_INITIAL_VALUE (static YYSTYPE yyval_default;)
YYSTYPE yylval YY_INITIAL_VALUE (= yyval_default);

    /* Number of syntax errors so far.  */
    int yynerrs = 0;

    yy_state_fast_t yystate = 0;
    /* Number of tokens to shift before error messages enabled.  */
    int yyerrstatus = 0;

    /* Refer to the stacks through separate pointers, to allow yyoverflow
       to reallocate them elsewhere.  */

    /* Their size.  */
    YYPTRDIFF_T yystacksize = YYINITDEPTH;

    /* The state stack: array, bottom, top.  */
    yy_state_t yyssa[YYINITDEPTH];
    yy_state_t *yyss = yyssa;
    yy_state_t *yyssp = yyss;

    /* The semantic value stack: array, bottom, top.  */
    YYSTYPE yyvsa[YYINITDEPTH];
    YYSTYPE *yyvs = yyvsa;
    YYSTYPE *yyvsp = yyvs;

  int yyn;
  /* The return value of yyparse.  */
  int yyresult;
  /* Lookahead symbol kind.  */
  yysymbol_kind_t yytoken = YYSYMBOL_YYEMPTY;
  /* The variables used to return semantic value and location from the
     action routines.  */
  YYSTYPE yyval;



#define YYPOPSTACK(N)   (yyvsp -= (N), yyssp -= (N))

  /* The number of symbols on the RHS of the reduced rule.
     Keep to zero when no symbol should be popped.  */
  int yylen = 0;

  YYDPRINTF ((stderr, "Starting parse\n"));

  yychar = UNITSEMPTY; /* Cause a token to be read.  */

  goto yysetstate;


/*------------------------------------------------------------.
| yynewstate -- push a new state, which is found in yystate.  |
`------------------------------------------------------------*/
yynewstate:
  /* In all cases, when you get here, the value and location stacks
     have just been pushed.  So pushing a state here evens the stacks.  */
  yyssp++;


/*--------------------------------------------------------------------.
| yysetstate -- set current state (the top of the stack) to yystate.  |
`--------------------------------------------------------------------*/
yysetstate:
  YYDPRINTF ((stderr, "Entering state %d\n", yystate));
  YY_ASSERT (0 <= yystate && yystate < YYNSTATES);
  YY_IGNORE_USELESS_CAST_BEGIN
  *yyssp = YY_CAST (yy_state_t, yystate);
  YY_IGNORE_USELESS_CAST_END
  YY_STACK_PRINT (yyss, yyssp);

  if (yyss + yystacksize - 1 <= yyssp)
#if !defined yyoverflow && !defined YYSTACK_RELOCATE
    YYNOMEM;
#else
    {
      /* Get the current used size of the three stacks, in elements.  */
      YYPTRDIFF_T yysize = yyssp - yyss + 1;

# if defined yyoverflow
      {
        /* Give user a chance to reallocate the stack.  Use copies of
           these so that the &'s don't force the real ones into
           memory.  */
        yy_state_t *yyss1 = yyss;
        YYSTYPE *yyvs1 = yyvs;

        /* Each stack pointer address is followed by the size of the
           data in use in that stack, in bytes.  This used to be a
           conditional around just the two extra args, but that might
           be undefined if yyoverflow is a macro.  */
        yyoverflow (YY_("memory exhausted"),
                    &yyss1, yysize * YYSIZEOF (*yyssp),
                    &yyvs1, yysize * YYSIZEOF (*yyvsp),
                    &yystacksize);
        yyss = yyss1;
        yyvs = yyvs1;
      }
# else /* defined YYSTACK_RELOCATE */
      /* Extend the stack our own way.  */
      if (YYMAXDEPTH <= yystacksize)
        YYNOMEM;
      yystacksize *= 2;
      if (YYMAXDEPTH < yystacksize)
        yystacksize = YYMAXDEPTH;

      {
        yy_state_t *yyss1 = yyss;
        union yyalloc *yyptr =
          YY_CAST (union yyalloc *,
                   YYSTACK_ALLOC (YY_CAST (YYSIZE_T, YYSTACK_BYTES (yystacksize))));
        if (! yyptr)
          YYNOMEM;
        YYSTACK_RELOCATE (yyss_alloc, yyss);
        YYSTACK_RELOCATE (yyvs_alloc, yyvs);
#  undef YYSTACK_RELOCATE
        if (yyss1 != yyssa)
          YYSTACK_FREE (yyss1);
      }
# endif

      yyssp = yyss + yysize - 1;
      yyvsp = yyvs + yysize - 1;

      YY_IGNORE_USELESS_CAST_BEGIN
      YYDPRINTF ((stderr, "Stack size increased to %ld\n",
                  YY_CAST (long, yystacksize)));
      YY_IGNORE_USELESS_CAST_END

      if (yyss + yystacksize - 1 <= yyssp)
        YYABORT;
    }
#endif /* !defined yyoverflow && !defined YYSTACK_RELOCATE */


  if (yystate == YYFINAL)
    YYACCEPT;

  goto yybackup;


/*-----------.
| yybackup.  |
`-----------*/
yybackup:
  /* Do appropriate processing given the current state.  Read a
     lookahead token if we need one and don't already have one.  */

  /* First try to decide what to do without reference to lookahead token.  */
  yyn = yypact[yystate];
  if (yypact_value_is_default (yyn))
    goto yydefault;

  /* Not known => get a lookahead token if don't already have one.  */

  /* YYCHAR is either empty, or end-of-input, or a valid lookahead.  */
  if (yychar == UNITSEMPTY)
    {
      YYDPRINTF ((stderr, "Reading a token\n"));
      yychar = yylex (&yylval, comm);
    }

  if (yychar <= UNITSEOF)
    {
      yychar = UNITSEOF;
      yytoken = YYSYMBOL_YYEOF;
      YYDPRINTF ((stderr, "Now at end of input.\n"));
    }
  else if (yychar == UNITSerror)
    {
      /* The scanner already issued an error message, process directly
         to error recovery.  But do not keep the error token as
         lookahead, it is too special and may lead us to an endless
         loop in error recovery. */
      yychar = UNITSUNDEF;
      yytoken = YYSYMBOL_YYerror;
      goto yyerrlab1;
    }
  else
    {
      yytoken = YYTRANSLATE (yychar);
      YY_SYMBOL_PRINT ("Next token is", yytoken, &yylval, &yylloc);
    }

  /* If the proper action on seeing token YYTOKEN is to reduce or to
     detect an error, take that action.  */
  yyn += yytoken;
  if (yyn < 0 || YYLAST < yyn || yycheck[yyn] != yytoken)
    goto yydefault;
  yyn = yytable[yyn];
  if (yyn <= 0)
    {
      if (yytable_value_is_error (yyn))
        goto yyerrlab;
      yyn = -yyn;
      goto yyreduce;
    }

  /* Count tokens shifted since error; after three, turn off error
     status.  */
  if (yyerrstatus)
    yyerrstatus--;

  /* Shift the lookahead token.  */
  YY_SYMBOL_PRINT ("Shifting", yytoken, &yylval, &yylloc);
  yystate = yyn;
  YY_IGNORE_MAYBE_UNINITIALIZED_BEGIN
  *++yyvsp = yylval;
  YY_IGNORE_MAYBE_UNINITIALIZED_END

  /* Discard the shifted token.  */
  yychar = UNITSEMPTY;
  goto yynewstate;


/*-----------------------------------------------------------.
| yydefault -- do the default action for the current state.  |
`-----------------------------------------------------------*/
yydefault:
  yyn = yydefact[yystate];
  if (yyn == 0)
    goto yyerrlab;
  goto yyreduce;


/*-----------------------------.
| yyreduce -- do a reduction.  |
`-----------------------------*/
yyreduce:
  /* yyn is the number of a rule to reduce with.  */
  yylen = yyr2[yyn];

  /* If YYLEN is nonzero, implement the default value of the action:
     '$$ = $1'.

     Otherwise, the following line sets YYVAL to garbage.
     This behavior is undocumented and Bison
     users should not rely upon it.  Assigning to YYVAL
     unconditionally makes the parser a bit smaller, and it avoids a
     GCC warning that YYVAL may be used uninitialized.  */
  yyval = yyvsp[1-yylen];


  YY_REDUCE_PRINT (yyn);
  switch (yyn)
    {
  case 2: /* input: EOL  */
#line 275 "parse.y"
                     { comm->result = makenumunit(1,&err); CHECK(0);
                       comm->errorcode = 0; YYACCEPT; }
#line 1503 "parse.tab.c"
    break;

  case 3: /* input: unitexpr EOL  */
#line 277 "parse.y"
                     { comm->result = (yyvsp[-1].unit); comm->errorcode = 0; YYACCEPT; }
#line 1509 "parse.tab.c"
    break;

  case 4: /* input: error  */
#line 278 "parse.y"
                     { YYABORT; }
#line 1515 "parse.tab.c"
    break;

  case 5: /* unitexpr: expr  */
#line 281 "parse.y"
                                    { (yyval.unit) = (yyvsp[0].unit);}
#line 1521 "parse.tab.c"
    break;

  case 6: /* unitexpr: divlist  */
#line 282 "parse.y"
                                    { (yyval.unit) = (yyvsp[0].unit);}
#line 1527 "parse.tab.c"
    break;

  case 7: /* divlist: DIVIDE list  */
#line 285 "parse.y"
                                    { invertunit((yyvsp[0].unit)); (yyval.unit)=(yyvsp[0].unit);}
#line 1533 "parse.tab.c"
    break;

  case 8: /* divlist: divlist divlist  */
#line 286 "parse.y"
                                         {err = multunit((yyvsp[-1].unit),(yyvsp[0].unit)); destroyunit((yyvsp[0].unit));
                                          CHECK((yyvsp[-1].unit));(yyval.unit)=(yyvsp[-1].unit);}
#line 1540 "parse.tab.c"
    break;

  case 9: /* expr: list  */
#line 290 "parse.y"
                                    { (yyval.unit) = (yyvsp[0].unit); }
#line 1546 "parse.tab.c"
    break;

  case 10: /* expr: MULTMINUS list  */
#line 291 "parse.y"
                                    { (yyval.unit) = (yyvsp[0].unit); (yyval.unit)->factor *= -1; }
#line 1552 "parse.tab.c"
    break;

  case 11: /* expr: MINUS list  */
#line 292 "parse.y"
                                    { (yyval.unit) = (yyvsp[0].unit); (yyval.unit)->factor *= -1; }
#line 1558 "parse.tab.c"
    break;

  case 12: /* expr: expr ADD expr  */
#line 307 "parse.y"
                                    { err = addunit((yyvsp[-2].unit),(yyvsp[0].unit)); destroyunit((yyvsp[0].unit));
                                      CHECK((yyvsp[-2].unit));(yyval.unit)=(yyvsp[-2].unit);}
#line 1565 "parse.tab.c"
    break;

  case 13: /* expr: expr MINUS expr  */
#line 309 "parse.y"
                                    { (yyvsp[0].unit)->factor *= -1;
                                      err = addunit((yyvsp[-2].unit),(yyvsp[0].unit)); destroyunit((yyvsp[0].unit));
                                      CHECK((yyvsp[-2].unit));(yyval.unit)=(yyvsp[-2].unit);}
#line 1573 "parse.tab.c"
    break;

  case 14: /* expr: expr DIVIDE expr  */
#line 312 "parse.y"
                                    { err = divunit((yyvsp[-2].unit), (yyvsp[0].unit)); destroyunit((yyvsp[0].unit));
                                      CHECK((yyvsp[-2].unit));(yyval.unit)=(yyvsp[-2].unit);}
#line 1580 "parse.tab.c"
    break;

  case 15: /* expr: expr MULTIPLY expr  */
#line 314 "parse.y"
                                    { err = multunit((yyvsp[-2].unit),(yyvsp[0].unit)); destroyunit((yyvsp[0].unit));
                                      CHECK((yyvsp[-2].unit));(yyval.unit)=(yyvsp[-2].unit);}
#line 1587 "parse.tab.c"
    break;

  case 16: /* expr: expr MULTSTAR expr  */
#line 316 "parse.y"
                                    { err = multunit((yyvsp[-2].unit),(yyvsp[0].unit)); destroyunit((yyvsp[0].unit));
                                      CHECK((yyvsp[-2].unit));(yyval.unit)=(yyvsp[-2].unit);}
#line 1594 "parse.tab.c"
    break;

  case 17: /* numexpr: REAL  */
#line 320 "parse.y"
                                    { (yyval.number) = (yyvsp[0].number);         }
#line 1600 "parse.tab.c"
    break;

  case 18: /* numexpr: numexpr NUMDIV numexpr  */
#line 321 "parse.y"
                                    { (yyval.number) = (yyvsp[-2].number) / (yyvsp[0].number);    }
#line 1606 "parse.tab.c"
    break;

  case 19: /* unitarray: expr  */
#line 324 "parse.y"
                                    { (yyval.unitarray) = addtoarray(NULL, (yyvsp[0].unit)); }
#line 1612 "parse.tab.c"
    break;

  case 20: /* unitarray: unitarray ',' expr  */
#line 325 "parse.y"
                                    { (yyval.unitarray) = addtoarray((yyvsp[-2].unitarray), (yyvsp[0].unit)); }
#line 1618 "parse.tab.c"
    break;

  case 21: /* pexpr: '(' unitarray ')'  */
#line 328 "parse.y"
                                        { (yyval.unitarray) = (yyvsp[-1].unitarray); }
#line 1624 "parse.tab.c"
    break;

  case 22: /* list: numexpr  */
#line 334 "parse.y"
                                   { (yyval.unit) = makenumunit((yyvsp[0].number),&err); CHECK(0);}
#line 1630 "parse.tab.c"
    break;

  case 23: /* list: UNIT  */
#line 335 "parse.y"
                                   { (yyval.unit) = (yyvsp[0].unit); }
#line 1636 "parse.tab.c"
    break;

  case 24: /* list: list EXPONENT list  */
#line 336 "parse.y"
                                   { err = unitpower((yyvsp[-2].unit),(yyvsp[0].unit));destroyunit((yyvsp[0].unit));
                                     CHECK((yyvsp[-2].unit));(yyval.unit)=(yyvsp[-2].unit);}
#line 1643 "parse.tab.c"
    break;

  case 25: /* list: list MULTMINUS list  */
#line 338 "parse.y"
                                   { err = multunit((yyvsp[-2].unit),(yyvsp[0].unit)); destroyunit((yyvsp[0].unit));
                                     CHECK((yyvsp[-2].unit));(yyval.unit)=(yyvsp[-2].unit);}
#line 1650 "parse.tab.c"
    break;

  case 26: /* list: list list  */
#line 340 "parse.y"
                                   { err = multunit((yyvsp[-1].unit),(yyvsp[0].unit)); destroyunit((yyvsp[0].unit));
                                     CHECK((yyvsp[-1].unit));(yyval.unit)=(yyvsp[-1].unit);}
#line 1657 "parse.tab.c"
    break;

  case 27: /* list: pexpr  */
#line 342 "parse.y"
                                   { (yyval.unit) = onearg((yyvsp[0].unitarray),2); CHECK((yyval.unit)); }
#line 1663 "parse.tab.c"
    break;

  case 28: /* list: SQRT pexpr  */
#line 343 "parse.y"
                                   { (yyval.unit) = onearg((yyvsp[0].unitarray),1); if (!err) err = rootunit((yyval.unit),2); CHECK((yyval.unit)); }
#line 1669 "parse.tab.c"
    break;

  case 29: /* list: CUBEROOT pexpr  */
#line 344 "parse.y"
                                   { (yyval.unit) = onearg((yyvsp[0].unitarray),1); if (!err) err = rootunit((yyval.unit),3); CHECK((yyval.unit)); }
#line 1675 "parse.tab.c"
    break;

  case 30: /* list: REALFUNC pexpr  */
#line 345 "parse.y"
                                   { (yyval.unit) = onearg((yyvsp[0].unitarray),1); if (!err) err = funcunit((yyval.unit),(yyvsp[-1].realfunc));CHECK((yyval.unit)); }
#line 1681 "parse.tab.c"
    break;

  case 31: /* list: LOG pexpr  */
#line 346 "parse.y"
                                   { (yyval.unit) = onearg((yyvsp[0].unitarray),1); if (!err) err = logunit((yyval.unit),(yyvsp[-1].integer)); CHECK((yyval.unit)); }
#line 1687 "parse.tab.c"
    break;

  case 32: /* list: UNITFUNC pexpr  */
#line 347 "parse.y"
                                   { err = evalfunc((yyvsp[0].unitarray)->count, (yyvsp[0].unitarray)->units, (yyvsp[-1].unitfunc), 0, 0);
                                     (yyval.unit) = onearg((yyvsp[0].unitarray),0); CHECK((yyval.unit)); }
#line 1694 "parse.tab.c"
    break;

  case 33: /* list: FUNCINV UNITFUNC pexpr  */
#line 349 "parse.y"
                                   { err = evalfunc((yyvsp[0].unitarray)->count, (yyvsp[0].unitarray)->units, (yyvsp[-1].unitfunc), 1, 0);
                                     (yyval.unit)=onearg((yyvsp[0].unitarray),0); CHECK((yyval.unit)); }
#line 1701 "parse.tab.c"
    break;

  case 34: /* list: list EXPONENT MULTMINUS list  */
#line 352 "parse.y"
                                   { (yyvsp[0].unit)->factor *= -1; err = unitpower((yyvsp[-3].unit),(yyvsp[0].unit));
                                     destroyunit((yyvsp[0].unit));CHECK((yyvsp[-3].unit));(yyval.unit)=(yyvsp[-3].unit);}
#line 1708 "parse.tab.c"
    break;

  case 35: /* list: list EXPONENT MINUS list  */
#line 355 "parse.y"
                                   { (yyvsp[0].unit)->factor *= -1; err = unitpower((yyvsp[-3].unit),(yyvsp[0].unit));
                                     destroyunit((yyvsp[0].unit));CHECK((yyvsp[-3].unit));(yyval.unit)=(yyvsp[-3].unit);}
#line 1715 "parse.tab.c"
    break;

  case 36: /* list: BADNUMBER  */
#line 357 "parse.y"
                                   { err = E_BADNUM;   CHECK(0); (yyval.unit)=NULL;}
#line 1721 "parse.tab.c"
    break;

  case 37: /* list: NUMOVERFLOW  */
#line 358 "parse.y"
                                   { err = E_OVERFLOW; CHECK(0); (yyval.unit)=NULL;}
#line 1727 "parse.tab.c"
    break;

  case 38: /* list: NUMUNDERFLOW  */
#line 359 "parse.y"
                                   { err = E_UNDERFLOW;CHECK(0); (yyval.unit)=NULL;}
#line 1733 "parse.tab.c"
    break;

  case 39: /* list: MEMERROR  */
#line 360 "parse.y"
                                   { err = E_PARSEMEM; CHECK(0); (yyval.unit)=NULL;}
#line 1739 "parse.tab.c"
    break;

  case 40: /* list: UNITEND  */
#line 361 "parse.y"
                                   { err = E_UNITEND;  CHECK(0); (yyval.unit)=NULL;}
#line 1745 "parse.tab.c"
    break;

  case 41: /* list: LASTUNSET  */
#line 362 "parse.y"
                                   { err = E_LASTUNSET;CHECK(0); (yyval.unit)=NULL;}
#line 1751 "parse.tab.c"
    break;

  case 42: /* list: FUNCINV UNIT  */
#line 363 "parse.y"
                                   { err = E_NOTAFUNC; CHECK((yyvsp[0].unit));(yyval.unit)=NULL;}
#line 1757 "parse.tab.c"
    break;


#line 1761 "parse.tab.c"

      default: break;
    }
  /* User semantic actions sometimes alter yychar, and that requires
     that yytoken be updated with the new translation.  We take the
     approach of translating immediately before every use of yytoken.
     One alternative is translating here after every semantic action,
     but that translation would be missed if the semantic action invokes
     YYABORT, YYACCEPT, or YYERROR immediately after altering yychar or
     if it invokes YYBACKUP.  In the case of YYABORT or YYACCEPT, an
     incorrect destructor might then be invoked immediately.  In the
     case of YYERROR or YYBACKUP, subsequent parser actions might lead
     to an incorrect destructor call or verbose syntax error message
     before the lookahead is translated.  */
  YY_SYMBOL_PRINT ("-> $$ =", YY_CAST (yysymbol_kind_t, yyr1[yyn]), &yyval, &yyloc);

  YYPOPSTACK (yylen);
  yylen = 0;

  *++yyvsp = yyval;

  /* Now 'shift' the result of the reduction.  Determine what state
     that goes to, based on the state we popped back to and the rule
     number reduced by.  */
  {
    const int yylhs = yyr1[yyn] - YYNTOKENS;
    const int yyi = yypgoto[yylhs] + *yyssp;
    yystate = (0 <= yyi && yyi <= YYLAST && yycheck[yyi] == *yyssp
               ? yytable[yyi]
               : yydefgoto[yylhs]);
  }

  goto yynewstate;


/*--------------------------------------.
| yyerrlab -- here on detecting error.  |
`--------------------------------------*/
yyerrlab:
  /* Make sure we have latest lookahead translation.  See comments at
     user semantic actions for why this is necessary.  */
  yytoken = yychar == UNITSEMPTY ? YYSYMBOL_YYEMPTY : YYTRANSLATE (yychar);
  /* If not already recovering from an error, report this error.  */
  if (!yyerrstatus)
    {
      ++yynerrs;
      yyerror (comm, YY_("syntax error"));
    }

  if (yyerrstatus == 3)
    {
      /* If just tried and failed to reuse lookahead token after an
         error, discard it.  */

      if (yychar <= UNITSEOF)
        {
          /* Return failure if at end of input.  */
          if (yychar == UNITSEOF)
            YYABORT;
        }
      else
        {
          yydestruct ("Error: discarding",
                      yytoken, &yylval, comm);
          yychar = UNITSEMPTY;
        }
    }

  /* Else will try to reuse lookahead token after shifting the error
     token.  */
  goto yyerrlab1;


/*---------------------------------------------------.
| yyerrorlab -- error raised explicitly by YYERROR.  |
`---------------------------------------------------*/
yyerrorlab:
  /* Pacify compilers when the user code never invokes YYERROR and the
     label yyerrorlab therefore never appears in user code.  */
  if (0)
    YYERROR;
  ++yynerrs;

  /* Do not reclaim the symbols of the rule whose action triggered
     this YYERROR.  */
  YYPOPSTACK (yylen);
  yylen = 0;
  YY_STACK_PRINT (yyss, yyssp);
  yystate = *yyssp;
  goto yyerrlab1;


/*-------------------------------------------------------------.
| yyerrlab1 -- common code for both syntax error and YYERROR.  |
`-------------------------------------------------------------*/
yyerrlab1:
  yyerrstatus = 3;      /* Each real token shifted decrements this.  */

  /* Pop stack until we find a state that shifts the error token.  */
  for (;;)
    {
      yyn = yypact[yystate];
      if (!yypact_value_is_default (yyn))
        {
          yyn += YYSYMBOL_YYerror;
          if (0 <= yyn && yyn <= YYLAST && yycheck[yyn] == YYSYMBOL_YYerror)
            {
              yyn = yytable[yyn];
              if (0 < yyn)
                break;
            }
        }

      /* Pop the current state because it cannot handle the error token.  */
      if (yyssp == yyss)
        YYABORT;


      yydestruct ("Error: popping",
                  YY_ACCESSING_SYMBOL (yystate), yyvsp, comm);
      YYPOPSTACK (1);
      yystate = *yyssp;
      YY_STACK_PRINT (yyss, yyssp);
    }

  YY_IGNORE_MAYBE_UNINITIALIZED_BEGIN
  *++yyvsp = yylval;
  YY_IGNORE_MAYBE_UNINITIALIZED_END


  /* Shift the error token.  */
  YY_SYMBOL_PRINT ("Shifting", YY_ACCESSING_SYMBOL (yyn), yyvsp, yylsp);

  yystate = yyn;
  goto yynewstate;


/*-------------------------------------.
| yyacceptlab -- YYACCEPT comes here.  |
`-------------------------------------*/
yyacceptlab:
  yyresult = 0;
  goto yyreturnlab;


/*-----------------------------------.
| yyabortlab -- YYABORT comes here.  |
`-----------------------------------*/
yyabortlab:
  yyresult = 1;
  goto yyreturnlab;


/*-----------------------------------------------------------.
| yyexhaustedlab -- YYNOMEM (memory exhaustion) comes here.  |
`-----------------------------------------------------------*/
yyexhaustedlab:
  yyerror (comm, YY_("memory exhausted"));
  yyresult = 2;
  goto yyreturnlab;


/*----------------------------------------------------------.
| yyreturnlab -- parsing is finished, clean up and return.  |
`----------------------------------------------------------*/
yyreturnlab:
  if (yychar != UNITSEMPTY)
    {
      /* Make sure we have latest lookahead translation.  See comments at
         user semantic actions for why this is necessary.  */
      yytoken = YYTRANSLATE (yychar);
      yydestruct ("Cleanup: discarding lookahead",
                  yytoken, &yylval, comm);
    }
  /* Do not reclaim the symbols of the rule whose action triggered
     this YYABORT or YYACCEPT.  */
  YYPOPSTACK (yylen);
  YY_STACK_PRINT (yyss, yyssp);
  while (yyssp != yyss)
    {
      yydestruct ("Cleanup: popping",
                  YY_ACCESSING_SYMBOL (+*yyssp), yyvsp, comm);
      YYPOPSTACK (1);
    }
#ifndef yyoverflow
  if (yyss != yyssa)
    YYSTACK_FREE (yyss);
#endif

  return yyresult;
}

#line 366 "parse.y"


double
factorial(double x)
{
  return tgamma(x+1);
}



double
csch(double x)
{
    return 1.0 / sinh(x);
}

double
sech(double x)
{
    return 1.0 / cosh(x);
}

double
coth(double x)
{
    return 1.0 / tanh(x);
}

double
acsch(double x)
{
    double xinv = 1.0 / x;
    return log(xinv + sqrt(xinv * xinv + 1.0));
}

double
asech(double x)
{
    double xinv = 1.0 / x;
    return log(xinv + sqrt(xinv * xinv - 1.0));
}

double
acoth(double x)
{
    return log(x + 1.0) / 2 - log(x - 1.0) / 2;
}

double
cot(double x)
{
  return cos(x)/sin(x);
}


double
secant(double x)
{  
  return 1.0 / cos(x);
}

double
csc(double x)
{
  return 1.0 / sin(x);
}


double
acot(double x)
{
  return atan(1/x);
}


double
asecant(double x)
{
  return acos(1/x);
}


double
acsc(double x)
{
  return asin(1/x);
}



struct function 
  realfunctions[] = { {"sin", sin,    ANGLEIN},
                      {"cos", cos,    ANGLEIN},
                      {"tan", tan,    ANGLEIN},
                      {"secant", secant, ANGLEIN},
                      {"csc", csc,    ANGLEIN},
                      {"cot", cot,    ANGLEIN},
                      {"ln", log,     DIMENSIONLESS},
                      {"log", log10,  DIMENSIONLESS},
                      {"exp", exp,    DIMENSIONLESS},
                      {"acos", acos,  ANGLEOUT},
                      {"atan", atan,  ANGLEOUT},
                      {"asin", asin,  ANGLEOUT},
                      {"asecant", asecant,  ANGLEOUT},
                      {"acot", acot,  ANGLEOUT},
                      {"acsc", acsc,  ANGLEOUT},
		      {"sinh", sinh, DIMENSIONLESS},
		      {"cosh", cosh, DIMENSIONLESS},		      
		      {"tanh", tanh, DIMENSIONLESS},
		      {"asinh", asinh, DIMENSIONLESS},
		      {"acosh", acosh, DIMENSIONLESS},		      
		      {"atanh", atanh, DIMENSIONLESS},
		      {"csch", csch, DIMENSIONLESS},
		      {"sech", sech, DIMENSIONLESS},
		      {"coth", coth, DIMENSIONLESS},
		      {"acsch", acsch, DIMENSIONLESS},
		      {"asech", asech, DIMENSIONLESS},
		      {"acoth", acoth, DIMENSIONLESS},
                      {"round", round, DIMENSIONLESS},
                      {"floor", floor, DIMENSIONLESS},
                      {"ceil", ceil, DIMENSIONLESS},
                      {"erf", erf, DIMENSIONLESS},
                      {"erfc", erfc, DIMENSIONLESS},
                      {"Gamma", tgamma, DIMENSIONLESS},
                      {"lnGamma", lgamma, DIMENSIONLESS},
                      {"factorial", factorial, NATURAL},
                      {0, 0, 0}};

struct {
  char op;
  int value;
} optable[] = { {'*', MULTIPLY},
                {'/', DIVIDE},
                {'|', NUMDIV},
                {'+', ADD},
                {'(', '('},
                {')', ')'},
                {',', ','},
                {'^', EXPONENT},
                {'~', FUNCINV},
                {0, 0}};

struct {
  char *op;
  int value;
} comptable[] = {
                  {"<=", LTE},
                  {">=", GTE},
                  {"==", EQ},
                  {"<", LT},
                  {">", GT},
                  {0,0}
                };
                  

struct {
  char *name;
  int value;
} strtable[] = { {"sqrt", SQRT},
                 {"cuberoot", CUBEROOT},
                 {"per" , DIVIDE},
                 {0, 0}};

#define LASTUNIT '_'     /* Last unit symbol */


int yylex(YYSTYPE *lvalp, struct commtype *comm)
{
  int length, count;
  struct unittype *output;
  const char *inptr;
  char *name;

#if 0  
  char *nonunitchars = ",~;+-*/|\t\n^ ()<>"; /* Chars not allowed in unit name --- also defined in units.c */
#endif
  char *nonunitchars = ",~;+-*/|\t\n^ ()"; /* Chars not allowed in unit name --- also defined in units.c */  
  char *nonunitstart = ".,";              /* Can't start a unit */
  char *nonunitend = ".,_";              /* Can't end a unit */
  char *number_start = ".0123456789";    /* Can be first char of a number */
  
  if (comm->location==-1) return 0;
  inptr = comm->data + comm->location;   /* Point to start of data */

  /* Skip spaces */
  while(*inptr==' ') inptr++, comm->location++;

  if (*inptr==0) {
    comm->location = -1;
    return EOL;  /* Return failure if string has ended */
  }  

  /* Check for **, an exponent operator.  */

  if (0==strncmp("**",inptr,2)){
    comm->location += 2;
    return EXPONENT;
  }

  /* Check for '-' and '*' which get special handling */

  if (*inptr=='-'){
    comm->location++;
    if (parserflags.minusminus)
      return MINUS;
    return MULTMINUS;
  }      

  if (*inptr=='*'){
    comm->location++;
    if (parserflags.oldstar)
      return MULTIPLY;
    return MULTSTAR;
  }      

  /* Look for single character ops */

  for(count=0; optable[count].op; count++){
    if (*inptr==optable[count].op) {
       comm->location++;
       return optable[count].value;
    }
  }

  /* Look for comparison operators */

  /*
  for(count=0;comptable[count].op;count++){
    if (0==strncmp(inptr, comptable[count].op, strlen(comptable[count].op))){
      comm->location += strlen(comptable[count].op);
      return comptable[count].value;
    }
  }
  */

  /* Look for numbers */

  if (strchr(number_start,*inptr)){  /* prevent "nan" from being recognized */
    char *endloc;
    errno=0;
    lvalp->number = strtod(inptr, &endloc);
    if (inptr != endloc) { 
      comm->location += (endloc-inptr);
      if (*endloc && strchr(number_start,*endloc))
        return BADNUMBER;
      else if (errno){
        errno=0;
        if (fabs(lvalp->number)==HUGE_VAL) return NUMOVERFLOW;
        else return NUMUNDERFLOW;
      }
      else
        return REAL;
    }
  }

  /* Look for a word (function name or unit name) */

  length = strcspn(inptr,nonunitchars);   
  
  if (!length){  /* Next char is not a valid unit char */
    printf("skipping invalid: '%c'\n",*inptr);
     comm->location++;
     return 0;
  }

  /* Check for the "last unit" symbol, with possible exponent */ 

  if (*inptr == LASTUNIT &&
      (length==1 || length==2 && strchr("23456789",inptr[1]))){
    comm->location++;
    if (!lastunitset) 
      return LASTUNSET;
    output = getnewunit();
    if (!output)
      return MEMERROR;
    unitcopy(output, &lastunit);
    if (length==2){
      expunit(output, inptr[1]-'0');
      comm->location++;
    }
    lvalp->unit = output;
    return UNIT;
  } 

  /* Check that unit name doesn't start or end with forbidden chars */
  if (strchr(nonunitstart,*inptr)){
    comm->location++;
    return 0;
  }
  if (strchr(nonunitend, inptr[length-1])){
    comm->location+=length;
    return 0;
  }

  name = dupnstr(inptr, length, "(yylex)");

  /* Look for string operators */

  for(count=0;strtable[count].name;count++){
    if (!strcmp(name,strtable[count].name)){
      free(name);
      comm->location += length;
      return strtable[count].value;
    }
  }

  /* Look for real function names */

  for(count=0;realfunctions[count].name;count++){
    if (!strcmp(name,realfunctions[count].name)){
      lvalp->realfunc = realfunctions+count;
      comm->location += length;
      free(name);
      return REALFUNC;
    }
  }

  /* Check for arbitrary base log */
  
  if (!strncmp(name, "log",3)){
    count = strspn(name+3,"1234567890");
    if (count+3 == strlen(name)){
      lvalp->integer=atoi(name+3);
      if (lvalp->integer>1){      /* Log base must be larger than 1 */
	comm->location += length;
	free(name);
	return LOG;
      }
    }
  }
      
  /* Look for function parameter */

  for(count=0;count<function_parameter_count;count++)
    if (function_parameter[count] && !strcmp(name,function_parameter[count])){
      free(name);
      output = getnewunit();
      if (!output)
        return MEMERROR;
      unitcopy(output, parameter_value[count]);
      lvalp->unit = output;
      comm->location += length;
      return UNIT;
    } 

  /* Look for user defined function */

  lvalp->unitfunc = fnlookup(name);
  if (lvalp->unitfunc){
    comm->location += length;
    free(name);
    return UNITFUNC;
  }

  /* Didn't find a special string, so treat it as unit name */

  comm->location+=length;
  if (strchr("23456789",inptr[length-1]) && !hassubscript(name)) {
    /* ends with digit but not a subscript, so do exponent handling like m3 */
    count = name[length-1] - '0';
    length--;
    if (strchr(number_start, name[length-1])){
      free(name);
      return UNITEND;
    }
  } else count=1;

  free(name);
    
  output = getnewunit();
  if (!output)
    return MEMERROR;
  output->numerator[count--]=0;
  for(;count>=0;count--)
    output->numerator[count] = dupnstr(inptr, length, "(yylex)");
  lvalp->unit=output;
  return UNIT;
}


void yyerror(struct commtype *comm, char *s){}


int
parseunit(struct unittype *output, char const *input,char **errstr,int *errloc)
{
  struct commtype comm;
  int saveunitcount;
  saveunitcount = unitcount;
  initializeunit(output);
  comm.result = 0;
  comm.location = 0;
  comm.data = input;
  comm.errorcode = E_PARSE;    /* Assume parse error */
  errno=0;
  /* errno should only be set in the case of invalid function arguments */
  if (yyparse(&comm) || errno){
    if (comm.location==-1) 
      comm.location = strlen(input);
    if (errstr){
      if (comm.errorcode==E_FUNC || errno)
        *errstr = strerror(errno);
      else
        *errstr=errormsg[comm.errorcode];
    }
    if (errloc)
      *errloc = comm.location;
    if (unitcount!=saveunitcount)
      fprintf(stderr,"units: Parser leaked memory with error: %d in %d out\n",
             saveunitcount, unitcount);
    return comm.errorcode;
  } else {
    if (errstr)
      *errstr = 0;
    multunit(output,comm.result);
    destroyunit(comm.result);
    if (unitcount!=saveunitcount)
      fprintf(stderr,"units: Parser leaked memory without error: %d in %d out\n",
	      saveunitcount, unitcount);
    return 0;
  }
}


