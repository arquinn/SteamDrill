/* A Bison parser, made by GNU Bison 2.5.  */

/* Bison implementation for Yacc-like parsers in C
   
      Copyright (C) 1984, 1989-1990, 2000-2011 Free Software Foundation, Inc.
   
   This program is free software: you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation, either version 3 of the License, or
   (at your option) any later version.
   
   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.
   
   You should have received a copy of the GNU General Public License
   along with this program.  If not, see <http://www.gnu.org/licenses/>.  */

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

/* All symbols defined below should begin with yy or YY, to avoid
   infringing on user name space.  This should be done even for local
   variables, as they might otherwise be expanded by user macros.
   There are some unavoidable exceptions within include files to
   define necessary library symbols; they are noted "INFRINGES ON
   USER NAME SPACE" below.  */

/* Identify Bison output.  */
#define YYBISON 1

/* Bison version.  */
#define YYBISON_VERSION "2.5"

/* Skeleton name.  */
#define YYSKELETON_NAME "yacc.c"

/* Pure parsers.  */
#define YYPURE 0

/* Push parsers.  */
#define YYPUSH 0

/* Pull parsers.  */
#define YYPULL 1

/* Using locations.  */
#define YYLSP_NEEDED 0



/* Copy the first part of user declarations.  */

/* Line 268 of yacc.c  */
#line 5 "sql_parser.y"

#include <stdlib.h>
#include <stdarg.h>
#include <string.h>
#include <stdio.h>
#include <fcntl.h>

#include <vector>
#include <fstream>
#include <boost/archive/text_oarchive.hpp>
#include <boost/serialization/vector.hpp>

#define YYDEBUG 1
#include "query_types.h"

void yyerror(const char *s, ...);
void emit(const char *s, ...);
int yylex(void);
void selection(std::vector<Query::Expression*> *sl, Query::Tables *tab);

extern int yylineno;
extern int yydebug;
boost::archive::text_oarchive *extract_archive;


/* Line 268 of yacc.c  */
#line 97 "sql_parser.tab.c"

/* Enabling traces.  */
#ifndef YYDEBUG
# define YYDEBUG 1
#endif

/* Enabling verbose error messages.  */
#ifdef YYERROR_VERBOSE
# undef YYERROR_VERBOSE
# define YYERROR_VERBOSE 1
#else
# define YYERROR_VERBOSE 0
#endif

/* Enabling the token table.  */
#ifndef YYTOKEN_TABLE
# define YYTOKEN_TABLE 0
#endif


/* Tokens.  */
#ifndef YYTOKENTYPE
# define YYTOKENTYPE
   /* Put the tokens into the symbol table, so that GDB and other debuggers
      know about them.  */
   enum yytokentype {
     NAME = 258,
     STRING = 259,
     INTNUM = 260,
     BOOL = 261,
     APPROXNUM = 262,
     USERVAR = 263,
     ASSIGN = 264,
     OR = 265,
     XOR = 266,
     ANDOP = 267,
     REGEXP = 268,
     LIKE = 269,
     IS = 270,
     IN = 271,
     NOT = 272,
     BETWEEN = 273,
     COMPARISON = 274,
     SHIFT = 275,
     MOD = 276,
     UMINUS = 277,
     ADD = 278,
     ALL = 279,
     ALTER = 280,
     ANALYZE = 281,
     AND = 282,
     ANY = 283,
     AS = 284,
     ASC = 285,
     AUTO_INCREMENT = 286,
     BEFORE = 287,
     BIGINT = 288,
     BINARY = 289,
     BIT = 290,
     BLOB = 291,
     BOTH = 292,
     BY = 293,
     CALL = 294,
     CASCADE = 295,
     CASE = 296,
     CHANGE = 297,
     CHAR = 298,
     CHECK = 299,
     COLLATE = 300,
     COLUMN = 301,
     COMMENT = 302,
     CONDITION = 303,
     CONSTRAINT = 304,
     CONTINUE = 305,
     CONVERT = 306,
     CREATE = 307,
     CROSS = 308,
     CURRENT_DATE = 309,
     CURRENT_TIME = 310,
     CURRENT_TIMESTAMP = 311,
     CURRENT_USER = 312,
     CURSOR = 313,
     DATABASE = 314,
     DATABASES = 315,
     DATE = 316,
     DATETIME = 317,
     DAY_HOUR = 318,
     DAY_MICROSECOND = 319,
     DAY_MINUTE = 320,
     DAY_SECOND = 321,
     DECIMAL = 322,
     DECLARE = 323,
     DEFAULT = 324,
     DELAYED = 325,
     DESC = 326,
     DESCRIBE = 327,
     DETERMINISTIC = 328,
     DISTINCT = 329,
     DISTINCTROW = 330,
     DIV = 331,
     DOUBLE = 332,
     DROP = 333,
     DUAL = 334,
     EACH = 335,
     ELSE = 336,
     ELSEIF = 337,
     END = 338,
     ENUM = 339,
     EXIT = 340,
     EXPLAIN = 341,
     FETCH = 342,
     FLOAT = 343,
     FOR = 344,
     FORCE = 345,
     FOREIGN = 346,
     FROM = 347,
     FULLTEXT = 348,
     GRANT = 349,
     GROUP = 350,
     HAVING = 351,
     HIGH_PRIORITY = 352,
     HOUR_MICROSECOND = 353,
     HOUR_MINUTE = 354,
     HOUR_SECOND = 355,
     IF = 356,
     IGNORE = 357,
     INFILE = 358,
     INDEX = 359,
     INNER = 360,
     INOUT = 361,
     INSENSITIVE = 362,
     INT = 363,
     INTEGER = 364,
     INTERVAL = 365,
     INTO = 366,
     ITERATE = 367,
     JOIN = 368,
     KEY = 369,
     KEYS = 370,
     KILL = 371,
     LEADING = 372,
     LEAVE = 373,
     LEFT = 374,
     LIMIT = 375,
     LINES = 376,
     LOAD = 377,
     LOCALTIME = 378,
     LOCALTIMESTAMP = 379,
     LOCK = 380,
     LONG = 381,
     LONGBLOB = 382,
     LONGTEXT = 383,
     LOOP = 384,
     LOW_PRIORITY = 385,
     MATCH = 386,
     MEDIUMBLOB = 387,
     MEDIUMINT = 388,
     MEDIUMTEXT = 389,
     MINUTE_MICROSECOND = 390,
     MINUTE_SECOND = 391,
     MODIFIES = 392,
     NATURAL = 393,
     NO_WRITE_TO_BINLOG = 394,
     NULLX = 395,
     NUMBER = 396,
     ON = 397,
     ONDUPLICATE = 398,
     OPTIMIZE = 399,
     OPTION = 400,
     OPTIONALLY = 401,
     ORDER = 402,
     OUT = 403,
     OUTER = 404,
     OUTFILE = 405,
     PRECISION = 406,
     PRIMARY = 407,
     PROCEDURE = 408,
     PURGE = 409,
     QUICK = 410,
     READ = 411,
     READS = 412,
     REAL = 413,
     REFERENCES = 414,
     RELEASE = 415,
     RENAME = 416,
     REPEAT = 417,
     REPLACE = 418,
     REQUIRE = 419,
     RESTRICT = 420,
     RETURN = 421,
     REVOKE = 422,
     RIGHT = 423,
     ROLLUP = 424,
     SCHEMA = 425,
     SCHEMAS = 426,
     SECOND_MICROSECOND = 427,
     SELECT = 428,
     SENSITIVE = 429,
     SEPARATOR = 430,
     SET = 431,
     SHOW = 432,
     SMALLINT = 433,
     SOME = 434,
     SONAME = 435,
     SPATIAL = 436,
     SPECIFIC = 437,
     SQL = 438,
     SQLEXCEPTION = 439,
     SQLSTATE = 440,
     SQLWARNING = 441,
     SQL_BIG_RESULT = 442,
     SQL_CALC_FOUND_ROWS = 443,
     SQL_SMALL_RESULT = 444,
     SSL = 445,
     STARTING = 446,
     STRAIGHT_JOIN = 447,
     TABLE = 448,
     TEMPORARY = 449,
     TERMINATED = 450,
     TEXT = 451,
     THEN = 452,
     TIME = 453,
     TIMESTAMP = 454,
     TINYBLOB = 455,
     TINYINT = 456,
     TINYTEXT = 457,
     TO = 458,
     TRAILING = 459,
     TRIGGER = 460,
     UNDO = 461,
     UNION = 462,
     UNIQUE = 463,
     UNLOCK = 464,
     UNSIGNED = 465,
     UPDATE = 466,
     USAGE = 467,
     USE = 468,
     USING = 469,
     UTC_DATE = 470,
     UTC_TIME = 471,
     UTC_TIMESTAMP = 472,
     VALUES = 473,
     VARBINARY = 474,
     VARCHAR = 475,
     VARYING = 476,
     WHEN = 477,
     WHERE = 478,
     WHILE = 479,
     WITH = 480,
     WRITE = 481,
     YEAR = 482,
     YEAR_MONTH = 483,
     ZEROFILL = 484,
     ESCAPED = 485,
     EXISTS = 486,
     FSUBSTRING = 487,
     FTRIM = 488,
     FDATE_ADD = 489,
     FDATE_SUB = 490,
     FCOUNT = 491
   };
#endif



#if ! defined YYSTYPE && ! defined YYSTYPE_IS_DECLARED
typedef union YYSTYPE
{

/* Line 293 of yacc.c  */
#line 32 "sql_parser.y"

    int intval;
    double floatval;
    char *strval;
    int subtok;

    std::vector<Query::Expression*> *sl;
    Query::Expression *e;
    Query::Tables *tables;



/* Line 293 of yacc.c  */
#line 382 "sql_parser.tab.c"
} YYSTYPE;
# define YYSTYPE_IS_TRIVIAL 1
# define yystype YYSTYPE /* obsolescent; will be withdrawn */
# define YYSTYPE_IS_DECLARED 1
#endif


/* Copy the second part of user declarations.  */


/* Line 343 of yacc.c  */
#line 394 "sql_parser.tab.c"

#ifdef short
# undef short
#endif

#ifdef YYTYPE_UINT8
typedef YYTYPE_UINT8 yytype_uint8;
#else
typedef unsigned char yytype_uint8;
#endif

#ifdef YYTYPE_INT8
typedef YYTYPE_INT8 yytype_int8;
#elif (defined __STDC__ || defined __C99__FUNC__ \
     || defined __cplusplus || defined _MSC_VER)
typedef signed char yytype_int8;
#else
typedef short int yytype_int8;
#endif

#ifdef YYTYPE_UINT16
typedef YYTYPE_UINT16 yytype_uint16;
#else
typedef unsigned short int yytype_uint16;
#endif

#ifdef YYTYPE_INT16
typedef YYTYPE_INT16 yytype_int16;
#else
typedef short int yytype_int16;
#endif

#ifndef YYSIZE_T
# ifdef __SIZE_TYPE__
#  define YYSIZE_T __SIZE_TYPE__
# elif defined size_t
#  define YYSIZE_T size_t
# elif ! defined YYSIZE_T && (defined __STDC__ || defined __C99__FUNC__ \
     || defined __cplusplus || defined _MSC_VER)
#  include <stddef.h> /* INFRINGES ON USER NAME SPACE */
#  define YYSIZE_T size_t
# else
#  define YYSIZE_T unsigned int
# endif
#endif

#define YYSIZE_MAXIMUM ((YYSIZE_T) -1)

#ifndef YY_
# if defined YYENABLE_NLS && YYENABLE_NLS
#  if ENABLE_NLS
#   include <libintl.h> /* INFRINGES ON USER NAME SPACE */
#   define YY_(msgid) dgettext ("bison-runtime", msgid)
#  endif
# endif
# ifndef YY_
#  define YY_(msgid) msgid
# endif
#endif

/* Suppress unused-variable warnings by "using" E.  */
#if ! defined lint || defined __GNUC__
# define YYUSE(e) ((void) (e))
#else
# define YYUSE(e) /* empty */
#endif

/* Identity function, used to suppress warnings about constant conditions.  */
#ifndef lint
# define YYID(n) (n)
#else
#if (defined __STDC__ || defined __C99__FUNC__ \
     || defined __cplusplus || defined _MSC_VER)
static int
YYID (int yyi)
#else
static int
YYID (yyi)
    int yyi;
#endif
{
  return yyi;
}
#endif

#if ! defined yyoverflow || YYERROR_VERBOSE

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
#    if ! defined _ALLOCA_H && ! defined EXIT_SUCCESS && (defined __STDC__ || defined __C99__FUNC__ \
     || defined __cplusplus || defined _MSC_VER)
#     include <stdlib.h> /* INFRINGES ON USER NAME SPACE */
#     ifndef EXIT_SUCCESS
#      define EXIT_SUCCESS 0
#     endif
#    endif
#   endif
#  endif
# endif

# ifdef YYSTACK_ALLOC
   /* Pacify GCC's `empty if-body' warning.  */
#  define YYSTACK_FREE(Ptr) do { /* empty */; } while (YYID (0))
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
#   if ! defined malloc && ! defined EXIT_SUCCESS && (defined __STDC__ || defined __C99__FUNC__ \
     || defined __cplusplus || defined _MSC_VER)
void *malloc (YYSIZE_T); /* INFRINGES ON USER NAME SPACE */
#   endif
#  endif
#  ifndef YYFREE
#   define YYFREE free
#   if ! defined free && ! defined EXIT_SUCCESS && (defined __STDC__ || defined __C99__FUNC__ \
     || defined __cplusplus || defined _MSC_VER)
void free (void *); /* INFRINGES ON USER NAME SPACE */
#   endif
#  endif
# endif
#endif /* ! defined yyoverflow || YYERROR_VERBOSE */


#if (! defined yyoverflow \
     && (! defined __cplusplus \
	 || (defined YYSTYPE_IS_TRIVIAL && YYSTYPE_IS_TRIVIAL)))

/* A type that is properly aligned for any stack member.  */
union yyalloc
{
  yytype_int16 yyss_alloc;
  YYSTYPE yyvs_alloc;
};

/* The size of the maximum gap between one aligned stack and the next.  */
# define YYSTACK_GAP_MAXIMUM (sizeof (union yyalloc) - 1)

/* The size of an array large to enough to hold all stacks, each with
   N elements.  */
# define YYSTACK_BYTES(N) \
     ((N) * (sizeof (yytype_int16) + sizeof (YYSTYPE)) \
      + YYSTACK_GAP_MAXIMUM)

# define YYCOPY_NEEDED 1

/* Relocate STACK from its old location to the new one.  The
   local variables YYSIZE and YYSTACKSIZE give the old and new number of
   elements in the stack, and YYPTR gives the new location of the
   stack.  Advance YYPTR to a properly aligned location for the next
   stack.  */
# define YYSTACK_RELOCATE(Stack_alloc, Stack)				\
    do									\
      {									\
	YYSIZE_T yynewbytes;						\
	YYCOPY (&yyptr->Stack_alloc, Stack, yysize);			\
	Stack = &yyptr->Stack_alloc;					\
	yynewbytes = yystacksize * sizeof (*Stack) + YYSTACK_GAP_MAXIMUM; \
	yyptr += yynewbytes / sizeof (*yyptr);				\
      }									\
    while (YYID (0))

#endif

#if defined YYCOPY_NEEDED && YYCOPY_NEEDED
/* Copy COUNT objects from FROM to TO.  The source and destination do
   not overlap.  */
# ifndef YYCOPY
#  if defined __GNUC__ && 1 < __GNUC__
#   define YYCOPY(To, From, Count) \
      __builtin_memcpy (To, From, (Count) * sizeof (*(From)))
#  else
#   define YYCOPY(To, From, Count)		\
      do					\
	{					\
	  YYSIZE_T yyi;				\
	  for (yyi = 0; yyi < (Count); yyi++)	\
	    (To)[yyi] = (From)[yyi];		\
	}					\
      while (YYID (0))
#  endif
# endif
#endif /* !YYCOPY_NEEDED */

/* YYFINAL -- State number of the termination state.  */
#define YYFINAL  11
/* YYLAST -- Last index in YYTABLE.  */
#define YYLAST   969

/* YYNTOKENS -- Number of terminals.  */
#define YYNTOKENS  251
/* YYNNTS -- Number of nonterminals.  */
#define YYNNTS  25
/* YYNRULES -- Number of rules.  */
#define YYNRULES  116
/* YYNRULES -- Number of states.  */
#define YYNSTATES  237

/* YYTRANSLATE(YYLEX) -- Bison symbol number corresponding to YYLEX.  */
#define YYUNDEFTOK  2
#define YYMAXUTOK   491

#define YYTRANSLATE(YYX)						\
  ((unsigned int) (YYX) <= YYMAXUTOK ? yytranslate[YYX] : YYUNDEFTOK)

/* YYTRANSLATE[YYLEX] -- Bison symbol number corresponding to YYLEX.  */
static const yytype_uint8 yytranslate[] =
{
       0,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,    17,     2,     2,     2,    28,    22,     2,
     248,   249,    26,    24,   250,    25,   247,    27,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,   246,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,    30,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,    21,     2,     2,     2,     2,     2,
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
      15,    16,    18,    19,    20,    23,    29,    31,    32,    33,
      34,    35,    36,    37,    38,    39,    40,    41,    42,    43,
      44,    45,    46,    47,    48,    49,    50,    51,    52,    53,
      54,    55,    56,    57,    58,    59,    60,    61,    62,    63,
      64,    65,    66,    67,    68,    69,    70,    71,    72,    73,
      74,    75,    76,    77,    78,    79,    80,    81,    82,    83,
      84,    85,    86,    87,    88,    89,    90,    91,    92,    93,
      94,    95,    96,    97,    98,    99,   100,   101,   102,   103,
     104,   105,   106,   107,   108,   109,   110,   111,   112,   113,
     114,   115,   116,   117,   118,   119,   120,   121,   122,   123,
     124,   125,   126,   127,   128,   129,   130,   131,   132,   133,
     134,   135,   136,   137,   138,   139,   140,   141,   142,   143,
     144,   145,   146,   147,   148,   149,   150,   151,   152,   153,
     154,   155,   156,   157,   158,   159,   160,   161,   162,   163,
     164,   165,   166,   167,   168,   169,   170,   171,   172,   173,
     174,   175,   176,   177,   178,   179,   180,   181,   182,   183,
     184,   185,   186,   187,   188,   189,   190,   191,   192,   193,
     194,   195,   196,   197,   198,   199,   200,   201,   202,   203,
     204,   205,   206,   207,   208,   209,   210,   211,   212,   213,
     214,   215,   216,   217,   218,   219,   220,   221,   222,   223,
     224,   225,   226,   227,   228,   229,   230,   231,   232,   233,
     234,   235,   236,   237,   238,   239,   240,   241,   242,   243,
     244,   245
};

#if YYDEBUG
/* YYPRHS[YYN] -- Index of the first RHS symbol of rule number YYN in
   YYRHS.  */
static const yytype_uint16 yyprhs[] =
{
       0,     0,     3,     6,    10,    12,    16,    18,    20,    22,
      24,    28,    30,    32,    34,    36,    38,    42,    46,    50,
      54,    58,    62,    65,    69,    73,    77,    81,    85,    89,
      93,    96,    99,   103,   109,   116,   123,   130,   134,   139,
     143,   148,   152,   158,   160,   164,   165,   167,   173,   180,
     186,   193,   198,   203,   208,   213,   218,   225,   234,   239,
     247,   249,   251,   253,   260,   267,   271,   275,   279,   283,
     287,   291,   295,   299,   303,   308,   315,   319,   325,   330,
     336,   340,   345,   349,   354,   357,   359,   370,   371,   374,
     375,   379,   382,   387,   388,   390,   392,   393,   396,   397,
     401,   402,   405,   410,   411,   414,   416,   420,   422,   426,
     428,   431,   433,   434,   436,   440,   442
};

/* YYRHS -- A `-1'-separated list of the rules' RHS.  */
static const yytype_int16 yyrhs[] =
{
     252,     0,    -1,   260,   246,    -1,   252,   260,   246,    -1,
       3,    -1,     3,   247,     3,    -1,     8,    -1,     4,    -1,
     253,    -1,     3,    -1,     3,   247,     3,    -1,     8,    -1,
       4,    -1,     5,    -1,     7,    -1,     6,    -1,   254,    24,
     254,    -1,   254,    25,   254,    -1,   254,    26,   254,    -1,
     254,    27,   254,    -1,   254,    28,   254,    -1,   254,    29,
     254,    -1,    25,   254,    -1,   254,    12,   254,    -1,   254,
      10,   254,    -1,   254,    11,   254,    -1,   254,    21,   254,
      -1,   254,    22,   254,    -1,   254,    30,   254,    -1,   254,
      23,   254,    -1,    18,   254,    -1,    17,   254,    -1,   254,
      20,   254,    -1,   254,    20,   248,   261,   249,    -1,   254,
      20,    37,   248,   261,   249,    -1,   254,    20,   188,   248,
     261,   249,    -1,   254,    20,    33,   248,   261,   249,    -1,
     254,    15,   149,    -1,   254,    15,    18,   149,    -1,   254,
      15,     6,    -1,   254,    15,    18,     6,    -1,     8,     9,
     254,    -1,   254,    19,   254,    36,   254,    -1,   254,    -1,
     254,   250,   255,    -1,    -1,   255,    -1,   254,    16,   248,
     255,   249,    -1,   254,    18,    16,   248,   255,   249,    -1,
     254,    16,   248,   261,   249,    -1,   254,    18,    16,   248,
     261,   249,    -1,   240,   248,   261,   249,    -1,     3,   248,
     256,   249,    -1,   245,   248,    26,   249,    -1,   245,   248,
     254,   249,    -1,   241,   248,   255,   249,    -1,   241,   248,
     254,   101,   254,   249,    -1,   241,   248,   254,   101,   254,
      98,   254,   249,    -1,   242,   248,   255,   249,    -1,   242,
     248,   257,   254,   101,   255,   249,    -1,   126,    -1,   213,
      -1,    46,    -1,   243,   248,   254,   250,   258,   249,    -1,
     244,   248,   254,   250,   258,   249,    -1,   119,   254,    72,
      -1,   119,   254,    73,    -1,   119,   254,    74,    -1,   119,
     254,    75,    -1,   119,   254,   237,    -1,   119,   254,   236,
      -1,   119,   254,   107,    -1,   119,   254,   108,    -1,   119,
     254,   109,    -1,    50,   254,   259,    92,    -1,    50,   254,
     259,    90,   254,    92,    -1,    50,   259,    92,    -1,    50,
     259,    90,   254,    92,    -1,   231,   254,   206,   254,    -1,
     259,   231,   254,   206,   254,    -1,   254,    14,   254,    -1,
     254,    18,    14,   254,    -1,   254,    13,   254,    -1,   254,
      18,    13,   254,    -1,    43,   254,    -1,   261,    -1,   182,
     271,   101,   274,   262,   263,   266,   267,   268,   269,    -1,
      -1,   232,   254,    -1,    -1,   104,    47,   264,    -1,   254,
     265,    -1,   264,   250,   254,   265,    -1,    -1,    39,    -1,
      80,    -1,    -1,   105,   254,    -1,    -1,   156,    47,   264,
      -1,    -1,   129,   254,    -1,   129,   254,   250,   254,    -1,
      -1,   120,   270,    -1,     3,    -1,   270,   250,     3,    -1,
     272,    -1,   271,   250,   272,    -1,   253,    -1,    38,     3,
      -1,     3,    -1,    -1,   275,    -1,   274,   250,   275,    -1,
     253,    -1,   253,   273,    -1
};

/* YYRLINE[YYN] -- source line where rule number YYN was defined.  */
static const yytype_uint16 yyrline[] =
{
       0,   314,   314,   315,   320,   321,   322,   323,   328,   329,
     330,   331,   332,   333,   334,   335,   338,   339,   340,   341,
     342,   343,   344,   345,   346,   347,   348,   349,   350,   351,
     352,   353,   354,   357,   358,   359,   360,   363,   364,   365,
     366,   368,   371,   374,   375,   378,   379,   382,   383,   384,
     385,   386,   390,   394,   395,   397,   398,   399,   401,   402,
     405,   406,   407,   410,   411,   414,   415,   416,   417,   418,
     419,   420,   421,   422,   424,   425,   426,   427,   430,   431,
     433,   434,   437,   438,   441,   445,   447,   452,   453,   455,
     456,   459,   461,   465,   466,   467,   470,   471,   473,   474,
     477,   477,   478,   481,   482,   485,   486,   489,   490,   493,
     495,   496,   497,   500,   501,   504,   505
};
#endif

#if YYDEBUG || YYERROR_VERBOSE || YYTOKEN_TABLE
/* YYTNAME[SYMBOL-NUM] -- String name of the symbol SYMBOL-NUM.
   First, the terminals, then, starting at YYNTOKENS, nonterminals.  */
static const char *const yytname[] =
{
  "$end", "error", "$undefined", "NAME", "STRING", "INTNUM", "BOOL",
  "APPROXNUM", "USERVAR", "ASSIGN", "OR", "XOR", "ANDOP", "REGEXP", "LIKE",
  "IS", "IN", "'!'", "NOT", "BETWEEN", "COMPARISON", "'|'", "'&'", "SHIFT",
  "'+'", "'-'", "'*'", "'/'", "'%'", "MOD", "'^'", "UMINUS", "ADD", "ALL",
  "ALTER", "ANALYZE", "AND", "ANY", "AS", "ASC", "AUTO_INCREMENT",
  "BEFORE", "BIGINT", "BINARY", "BIT", "BLOB", "BOTH", "BY", "CALL",
  "CASCADE", "CASE", "CHANGE", "CHAR", "CHECK", "COLLATE", "COLUMN",
  "COMMENT", "CONDITION", "CONSTRAINT", "CONTINUE", "CONVERT", "CREATE",
  "CROSS", "CURRENT_DATE", "CURRENT_TIME", "CURRENT_TIMESTAMP",
  "CURRENT_USER", "CURSOR", "DATABASE", "DATABASES", "DATE", "DATETIME",
  "DAY_HOUR", "DAY_MICROSECOND", "DAY_MINUTE", "DAY_SECOND", "DECIMAL",
  "DECLARE", "DEFAULT", "DELAYED", "DESC", "DESCRIBE", "DETERMINISTIC",
  "DISTINCT", "DISTINCTROW", "DIV", "DOUBLE", "DROP", "DUAL", "EACH",
  "ELSE", "ELSEIF", "END", "ENUM", "EXIT", "EXPLAIN", "FETCH", "FLOAT",
  "FOR", "FORCE", "FOREIGN", "FROM", "FULLTEXT", "GRANT", "GROUP",
  "HAVING", "HIGH_PRIORITY", "HOUR_MICROSECOND", "HOUR_MINUTE",
  "HOUR_SECOND", "IF", "IGNORE", "INFILE", "INDEX", "INNER", "INOUT",
  "INSENSITIVE", "INT", "INTEGER", "INTERVAL", "INTO", "ITERATE", "JOIN",
  "KEY", "KEYS", "KILL", "LEADING", "LEAVE", "LEFT", "LIMIT", "LINES",
  "LOAD", "LOCALTIME", "LOCALTIMESTAMP", "LOCK", "LONG", "LONGBLOB",
  "LONGTEXT", "LOOP", "LOW_PRIORITY", "MATCH", "MEDIUMBLOB", "MEDIUMINT",
  "MEDIUMTEXT", "MINUTE_MICROSECOND", "MINUTE_SECOND", "MODIFIES",
  "NATURAL", "NO_WRITE_TO_BINLOG", "NULLX", "NUMBER", "ON", "ONDUPLICATE",
  "OPTIMIZE", "OPTION", "OPTIONALLY", "ORDER", "OUT", "OUTER", "OUTFILE",
  "PRECISION", "PRIMARY", "PROCEDURE", "PURGE", "QUICK", "READ", "READS",
  "REAL", "REFERENCES", "RELEASE", "RENAME", "REPEAT", "REPLACE",
  "REQUIRE", "RESTRICT", "RETURN", "REVOKE", "RIGHT", "ROLLUP", "SCHEMA",
  "SCHEMAS", "SECOND_MICROSECOND", "SELECT", "SENSITIVE", "SEPARATOR",
  "SET", "SHOW", "SMALLINT", "SOME", "SONAME", "SPATIAL", "SPECIFIC",
  "SQL", "SQLEXCEPTION", "SQLSTATE", "SQLWARNING", "SQL_BIG_RESULT",
  "SQL_CALC_FOUND_ROWS", "SQL_SMALL_RESULT", "SSL", "STARTING",
  "STRAIGHT_JOIN", "TABLE", "TEMPORARY", "TERMINATED", "TEXT", "THEN",
  "TIME", "TIMESTAMP", "TINYBLOB", "TINYINT", "TINYTEXT", "TO", "TRAILING",
  "TRIGGER", "UNDO", "UNION", "UNIQUE", "UNLOCK", "UNSIGNED", "UPDATE",
  "USAGE", "USE", "USING", "UTC_DATE", "UTC_TIME", "UTC_TIMESTAMP",
  "VALUES", "VARBINARY", "VARCHAR", "VARYING", "WHEN", "WHERE", "WHILE",
  "WITH", "WRITE", "YEAR", "YEAR_MONTH", "ZEROFILL", "ESCAPED", "EXISTS",
  "FSUBSTRING", "FTRIM", "FDATE_ADD", "FDATE_SUB", "FCOUNT", "';'", "'.'",
  "'('", "')'", "','", "$accept", "stmt_list", "keyword", "expr",
  "val_list", "opt_val_list", "trim_ltb", "interval_exp", "case_list",
  "stmt", "select_stmt", "opt_where", "opt_groupby", "groupby_list",
  "opt_asc_desc", "opt_having", "opt_orderby", "opt_limit",
  "opt_into_list", "column_list", "select_expr_list", "select_expr",
  "opt_as_alias", "table_references", "table_reference", 0
};
#endif

# ifdef YYPRINT
/* YYTOKNUM[YYLEX-NUM] -- Internal token number corresponding to
   token YYLEX-NUM.  */
static const yytype_uint16 yytoknum[] =
{
       0,   256,   257,   258,   259,   260,   261,   262,   263,   264,
     265,   266,   267,   268,   269,   270,   271,    33,   272,   273,
     274,   124,    38,   275,    43,    45,    42,    47,    37,   276,
      94,   277,   278,   279,   280,   281,   282,   283,   284,   285,
     286,   287,   288,   289,   290,   291,   292,   293,   294,   295,
     296,   297,   298,   299,   300,   301,   302,   303,   304,   305,
     306,   307,   308,   309,   310,   311,   312,   313,   314,   315,
     316,   317,   318,   319,   320,   321,   322,   323,   324,   325,
     326,   327,   328,   329,   330,   331,   332,   333,   334,   335,
     336,   337,   338,   339,   340,   341,   342,   343,   344,   345,
     346,   347,   348,   349,   350,   351,   352,   353,   354,   355,
     356,   357,   358,   359,   360,   361,   362,   363,   364,   365,
     366,   367,   368,   369,   370,   371,   372,   373,   374,   375,
     376,   377,   378,   379,   380,   381,   382,   383,   384,   385,
     386,   387,   388,   389,   390,   391,   392,   393,   394,   395,
     396,   397,   398,   399,   400,   401,   402,   403,   404,   405,
     406,   407,   408,   409,   410,   411,   412,   413,   414,   415,
     416,   417,   418,   419,   420,   421,   422,   423,   424,   425,
     426,   427,   428,   429,   430,   431,   432,   433,   434,   435,
     436,   437,   438,   439,   440,   441,   442,   443,   444,   445,
     446,   447,   448,   449,   450,   451,   452,   453,   454,   455,
     456,   457,   458,   459,   460,   461,   462,   463,   464,   465,
     466,   467,   468,   469,   470,   471,   472,   473,   474,   475,
     476,   477,   478,   479,   480,   481,   482,   483,   484,   485,
     486,   487,   488,   489,   490,   491,    59,    46,    40,    41,
      44
};
# endif

/* YYR1[YYN] -- Symbol number of symbol that rule YYN derives.  */
static const yytype_uint16 yyr1[] =
{
       0,   251,   252,   252,   253,   253,   253,   253,   254,   254,
     254,   254,   254,   254,   254,   254,   254,   254,   254,   254,
     254,   254,   254,   254,   254,   254,   254,   254,   254,   254,
     254,   254,   254,   254,   254,   254,   254,   254,   254,   254,
     254,   254,   254,   255,   255,   256,   256,   254,   254,   254,
     254,   254,   254,   254,   254,   254,   254,   254,   254,   254,
     257,   257,   257,   254,   254,   258,   258,   258,   258,   258,
     258,   258,   258,   258,   254,   254,   254,   254,   259,   259,
     254,   254,   254,   254,   254,   260,   261,   262,   262,   263,
     263,   264,   264,   265,   265,   265,   266,   266,   267,   267,
     268,   268,   268,   269,   269,   270,   270,   271,   271,   272,
     273,   273,   273,   274,   274,   275,   275
};

/* YYR2[YYN] -- Number of symbols composing right hand side of rule YYN.  */
static const yytype_uint8 yyr2[] =
{
       0,     2,     2,     3,     1,     3,     1,     1,     1,     1,
       3,     1,     1,     1,     1,     1,     3,     3,     3,     3,
       3,     3,     2,     3,     3,     3,     3,     3,     3,     3,
       2,     2,     3,     5,     6,     6,     6,     3,     4,     3,
       4,     3,     5,     1,     3,     0,     1,     5,     6,     5,
       6,     4,     4,     4,     4,     4,     6,     8,     4,     7,
       1,     1,     1,     6,     6,     3,     3,     3,     3,     3,
       3,     3,     3,     3,     4,     6,     3,     5,     4,     5,
       3,     4,     3,     4,     2,     1,    10,     0,     2,     0,
       3,     2,     4,     0,     1,     1,     0,     2,     0,     3,
       0,     2,     4,     0,     2,     1,     3,     1,     3,     1,
       2,     1,     0,     1,     3,     1,     2
};

/* YYDEFACT[STATE-NAME] -- Default reduction number in state STATE-NUM.
   Performed when YYTABLE doesn't specify something else to do.  Zero
   means the default is an error.  */
static const yytype_uint8 yydefact[] =
{
       0,     0,     0,     0,    85,     4,     7,     6,   109,     0,
     107,     1,     0,     2,     0,     0,     0,     3,     5,   112,
      87,   113,   108,   111,     0,   116,     0,     0,    89,   110,
       4,     7,    13,    15,    14,     6,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     8,    88,   114,
       0,    96,     0,    45,     0,    31,    30,    22,    84,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
      98,     5,    43,    46,     0,    41,     0,     0,     0,    76,
       0,     0,    43,     0,    62,    60,    61,     0,     0,     0,
       0,     0,     0,    24,    25,    23,    82,    80,    39,     0,
      37,     0,     0,     0,     0,     0,     0,     0,     0,     0,
      32,    26,    27,    29,    16,    17,    18,    19,    20,    21,
      28,    93,    90,    97,     0,   100,     0,    52,     0,     0,
      74,     0,     0,    51,     0,    55,    58,     0,     0,     0,
      53,    54,    40,    38,     0,     0,    83,    81,     0,     0,
       0,     0,     0,     0,    94,    95,    91,     0,     0,     0,
     103,    44,    78,     0,    77,     0,     0,     0,     0,     0,
       0,    47,    49,     0,     0,    42,     0,     0,     0,    33,
      93,    99,   101,     0,    86,    75,    79,     0,    56,     0,
       0,    63,    64,    48,    50,    36,    34,    35,    92,     0,
     105,   104,     0,    59,    65,    66,    67,    68,    71,    72,
      73,    70,    69,   102,     0,    57,   106
};

/* YYDEFGOTO[NTERM-NUM].  */
static const yytype_int16 yydefgoto[] =
{
      -1,     2,    47,    92,    93,    94,   108,   189,    61,     3,
       4,    28,    51,   142,   176,    90,   145,   180,   204,   221,
       9,    10,    25,    20,    21
};

/* YYPACT[STATE-NUM] -- Index in YYTABLE of the portion describing
   STATE-NUM.  */
#define YYPACT_NINF -236
static const yytype_int16 yypact[] =
{
    -173,   106,    39,  -230,  -236,  -229,  -236,  -236,  -236,   -91,
    -236,  -236,  -226,  -236,    18,   106,   106,  -236,  -236,     3,
    -221,  -236,  -236,  -236,    33,  -236,    99,   106,   -62,  -236,
    -235,  -236,  -236,  -236,  -236,    35,    99,    99,    99,    99,
      47,  -202,  -200,  -199,  -181,  -163,  -162,  -236,   902,  -236,
      45,    -7,    96,    99,    99,   108,   108,  -236,  -236,    99,
     485,   -57,  -173,    99,    20,    99,    99,    70,    99,    99,
      99,    99,    99,     1,  -148,    55,    99,    -3,    99,    99,
      99,    99,    99,    99,    99,    99,    99,    99,    99,    99,
     -48,  -236,   168,  -236,  -138,   902,   530,    -1,    99,  -236,
      99,  -137,   142,  -134,  -236,  -236,  -236,  -131,    99,   189,
     335,  -128,   422,   505,   921,   401,   939,   939,  -236,     2,
    -236,    76,    99,    99,  -126,   872,  -125,  -123,  -109,  -173,
     245,   279,   299,    32,   268,   268,   110,   110,   110,   110,
    -236,   839,  -107,   902,    94,    15,    99,  -236,    99,    99,
    -236,   797,   568,  -236,    99,  -236,  -236,   776,    26,    26,
    -236,  -236,  -236,  -236,  -102,  -101,   939,   939,    76,    99,
    -173,  -173,  -173,   -76,  -236,  -236,  -236,    99,    99,    99,
      86,  -236,   902,   818,  -236,    99,   383,    99,    99,   -74,
     -29,  -236,  -236,   -27,   -26,   226,   -25,   -24,   -22,  -236,
     839,  -107,   356,   225,  -236,  -236,   902,    99,  -236,   -20,
     464,  -236,  -236,  -236,  -236,  -236,  -236,  -236,  -236,    99,
    -236,   -19,   443,  -236,  -236,  -236,  -236,  -236,  -236,  -236,
    -236,  -236,  -236,   902,   229,  -236,  -236
};

/* YYPGOTO[NTERM-NUM].  */
static const yytype_int16 yypgoto[] =
{
    -236,  -236,    16,   574,   113,  -236,  -236,    77,   175,   242,
     164,  -236,  -236,    79,    80,  -236,  -236,  -236,  -236,  -236,
    -236,   260,  -236,  -236,   250
};

/* YYTABLE[YYPACT[STATE-NUM]].  What to do in state STATE-NUM.  If
   positive, shift that token.  If negative, reduce the rule which
   number is the opposite.  If YYTABLE_NINF, syntax error.  */
#define YYTABLE_NINF -1
static const yytype_int16 yytable[] =
{
      30,    31,    32,    33,    34,    35,    23,   118,   162,     1,
      15,    26,    52,    53,    36,    37,    13,     8,    14,   119,
      17,    18,    38,    30,    31,    32,    33,    34,    35,    27,
     126,    19,     8,    98,   127,    99,    29,    36,    37,    11,
      39,    24,    50,    19,    54,    38,    62,    40,    63,    64,
      30,    31,    32,    33,    34,    35,    81,    82,    83,    84,
      85,    86,    87,    39,    36,    37,   104,    65,   122,   123,
      40,   124,    38,    30,    31,    32,    33,    34,    35,    30,
      31,    32,    33,    34,    35,    66,    67,    36,    37,   149,
      39,   150,    88,    36,    37,    38,   111,    40,    89,    91,
     121,    38,    30,    31,    32,    33,    34,    35,   144,     5,
       6,   147,   153,    39,     7,   155,    36,    37,   156,    39,
      40,   160,   168,   170,    38,   171,    40,    76,    77,    78,
      79,    80,    81,    82,    83,    84,    85,    86,    87,   172,
      87,   178,    39,   177,   179,   188,   105,   191,   192,    40,
     120,   163,    68,    69,    70,    71,    72,    73,    74,    16,
      75,    76,    77,    78,    79,    80,    81,    82,    83,    84,
      85,    86,    87,   199,   100,   211,   103,   107,    68,    69,
      70,    71,    72,    73,    74,   128,    75,    76,    77,    78,
      79,    80,    81,    82,    83,    84,    85,    86,    87,    68,
      69,    70,    71,    72,    73,    74,   203,    75,    76,    77,
      78,    79,    80,    81,    82,    83,    84,    85,    86,    87,
     212,     1,   213,   214,   215,   216,   101,   217,   220,   223,
     100,   234,   236,   106,   164,    97,   190,    41,    42,    43,
      44,    45,    46,   154,    12,   129,    77,    78,    79,    80,
      81,    82,    83,    84,    85,    86,    87,   201,     1,   181,
      41,    42,    43,    44,    45,    46,    78,    79,    80,    81,
      82,    83,    84,    85,    86,    87,    22,    49,    59,     0,
     218,   193,     0,     0,     0,   165,     0,    41,    42,    43,
      44,    45,    46,   173,    83,    84,    85,    86,    87,     0,
     209,    79,    80,    81,    82,    83,    84,    85,    86,    87,
      41,    42,    43,    44,    45,    46,    41,    42,    43,    44,
      45,    46,    80,    81,    82,    83,    84,    85,    86,    87,
       0,     0,   194,     0,   196,   197,   198,     0,     0,    41,
      42,    43,    44,    45,    46,    68,    69,    70,    71,    72,
      73,    74,     0,    75,    76,    77,    78,    79,    80,    81,
      82,    83,    84,    85,    86,    87,    68,    69,    70,    71,
      72,    73,    74,     0,    75,    76,    77,    78,    79,    80,
      81,    82,    83,    84,    85,    86,    87,     0,     0,     0,
       0,     0,   146,    68,    69,    70,    71,    72,    73,    74,
       0,    75,    76,    77,    78,    79,    80,    81,    82,    83,
      84,    85,    86,    87,    71,    72,    73,    74,   146,    75,
      76,    77,    78,    79,    80,    81,    82,    83,    84,    85,
      86,    87,    68,    69,    70,    71,    72,    73,    74,   158,
      75,    76,    77,    78,    79,    80,    81,    82,    83,    84,
      85,    86,    87,    68,    69,    70,    71,    72,    73,    74,
       0,    75,    76,    77,    78,    79,    80,    81,    82,    83,
      84,    85,    86,    87,    68,    69,    70,    71,    72,    73,
      74,   207,    75,    76,    77,    78,    79,    80,    81,    82,
      83,    84,    85,    86,    87,    68,    69,    70,    71,    72,
      73,    74,     0,    75,    76,    77,    78,    79,    80,    81,
      82,    83,    84,    85,    86,    87,    69,    70,    71,    72,
      73,    74,     0,    75,    76,    77,    78,    79,    80,    81,
      82,    83,    84,    85,    86,    87,   224,   225,   226,   227,
      68,    69,    70,    71,    72,    73,    74,     0,    75,    76,
      77,    78,    79,    80,    81,    82,    83,    84,    85,    86,
      87,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,   228,   229,   230,     0,     0,     0,     0,    68,    69,
      70,    71,    72,    73,    74,   159,    75,    76,    77,    78,
      79,    80,    81,    82,    83,    84,    85,    86,    87,     0,
      48,     0,     0,     0,     0,     0,   219,     0,     0,     0,
      55,    56,    57,    58,    60,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,    95,     0,
       0,     0,   208,    96,     0,     0,     0,   102,     0,   109,
     110,   112,   113,   114,   115,   116,   117,     0,     0,     0,
     125,   130,   131,   132,   133,   134,   135,   136,   137,   138,
     139,   140,   141,   143,     0,     0,     0,     0,     0,     0,
       0,   161,   151,     0,   152,     0,     0,     0,     0,     0,
       0,     0,   157,     0,     0,     0,     0,     0,     0,     0,
       0,     0,   235,     0,     0,     0,   166,   167,     0,     0,
     231,   232,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,    59,     0,     0,     0,
       0,     0,   182,   183,     0,     0,     0,     0,   186,     0,
       0,     0,     0,     0,     0,     0,   148,     0,     0,     0,
       0,     0,     0,   195,     0,     0,     0,     0,     0,     0,
       0,   200,   141,   202,     0,     0,     0,     0,     0,   206,
       0,     0,   210,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,   185,     0,     0,     0,     0,     0,
       0,   222,     0,     0,     0,     0,    68,    69,    70,    71,
      72,    73,    74,   233,    75,    76,    77,    78,    79,    80,
      81,    82,    83,    84,    85,    86,    87,    68,    69,    70,
      71,    72,    73,    74,     0,    75,    76,    77,    78,    79,
      80,    81,    82,    83,    84,    85,    86,    87,    68,    69,
      70,    71,    72,    73,    74,     0,    75,    76,    77,    78,
      79,    80,    81,    82,    83,    84,    85,    86,    87,    68,
      69,    70,    71,    72,    73,    74,     0,    75,    76,    77,
      78,    79,    80,    81,    82,    83,    84,    85,    86,    87,
       0,     0,     0,     0,     0,     0,     0,   187,   174,     0,
       0,     0,    68,    69,    70,    71,    72,    73,    74,   184,
      75,    76,    77,    78,    79,    80,    81,    82,    83,    84,
      85,    86,    87,     0,     0,     0,     0,     0,   169,     0,
     205,     0,    68,    69,    70,    71,    72,    73,    74,   175,
      75,    76,    77,    78,    79,    80,    81,    82,    83,    84,
      85,    86,    87,    70,    71,    72,    73,    74,     0,    75,
      76,    77,    78,    79,    80,    81,    82,    83,    84,    85,
      86,    87,    -1,    -1,    -1,    -1,     0,    75,    76,    77,
      78,    79,    80,    81,    82,    83,    84,    85,    86,    87
};

#define yypact_value_is_default(yystate) \
  ((yystate) == (-236))

#define yytable_value_is_error(yytable_value) \
  ((yytable_value) == (-1))

static const yytype_int16 yycheck[] =
{
       3,     4,     5,     6,     7,     8,     3,     6,     6,   182,
     101,   232,   247,   248,    17,    18,   246,     1,   247,    18,
     246,     3,    25,     3,     4,     5,     6,     7,     8,   250,
      33,    15,    16,    90,    37,    92,     3,    17,    18,     0,
      43,    38,   104,    27,     9,    25,   248,    50,   248,   248,
       3,     4,     5,     6,     7,     8,    24,    25,    26,    27,
      28,    29,    30,    43,    17,    18,    46,   248,    13,    14,
      50,    16,    25,     3,     4,     5,     6,     7,     8,     3,
       4,     5,     6,     7,     8,   248,   248,    17,    18,    90,
      43,    92,    47,    17,    18,    25,    26,    50,   105,     3,
     248,    25,     3,     4,     5,     6,     7,     8,   156,     3,
       4,   249,   249,    43,     8,   249,    17,    18,   249,    43,
      50,   249,   248,   248,    25,   248,    50,    19,    20,    21,
      22,    23,    24,    25,    26,    27,    28,    29,    30,   248,
      30,    47,    43,   250,   129,   119,   126,   249,   249,    50,
     149,   149,    10,    11,    12,    13,    14,    15,    16,   250,
      18,    19,    20,    21,    22,    23,    24,    25,    26,    27,
      28,    29,    30,   249,   231,   249,    63,    64,    10,    11,
      12,    13,    14,    15,    16,   188,    18,    19,    20,    21,
      22,    23,    24,    25,    26,    27,    28,    29,    30,    10,
      11,    12,    13,    14,    15,    16,   120,    18,    19,    20,
      21,    22,    23,    24,    25,    26,    27,    28,    29,    30,
     249,   182,   249,   249,   249,   249,    62,   249,     3,   249,
     231,   250,     3,   213,   121,    60,   159,   240,   241,   242,
     243,   244,   245,   101,     2,   248,    20,    21,    22,    23,
      24,    25,    26,    27,    28,    29,    30,   178,   182,   146,
     240,   241,   242,   243,   244,   245,    21,    22,    23,    24,
      25,    26,    27,    28,    29,    30,    16,    27,   231,    -1,
     200,   168,    -1,    -1,    -1,   121,    -1,   240,   241,   242,
     243,   244,   245,   129,    26,    27,    28,    29,    30,    -1,
     187,    22,    23,    24,    25,    26,    27,    28,    29,    30,
     240,   241,   242,   243,   244,   245,   240,   241,   242,   243,
     244,   245,    23,    24,    25,    26,    27,    28,    29,    30,
      -1,    -1,   168,    -1,   170,   171,   172,    -1,    -1,   240,
     241,   242,   243,   244,   245,    10,    11,    12,    13,    14,
      15,    16,    -1,    18,    19,    20,    21,    22,    23,    24,
      25,    26,    27,    28,    29,    30,    10,    11,    12,    13,
      14,    15,    16,    -1,    18,    19,    20,    21,    22,    23,
      24,    25,    26,    27,    28,    29,    30,    -1,    -1,    -1,
      -1,    -1,   250,    10,    11,    12,    13,    14,    15,    16,
      -1,    18,    19,    20,    21,    22,    23,    24,    25,    26,
      27,    28,    29,    30,    13,    14,    15,    16,   250,    18,
      19,    20,    21,    22,    23,    24,    25,    26,    27,    28,
      29,    30,    10,    11,    12,    13,    14,    15,    16,   250,
      18,    19,    20,    21,    22,    23,    24,    25,    26,    27,
      28,    29,    30,    10,    11,    12,    13,    14,    15,    16,
      -1,    18,    19,    20,    21,    22,    23,    24,    25,    26,
      27,    28,    29,    30,    10,    11,    12,    13,    14,    15,
      16,    98,    18,    19,    20,    21,    22,    23,    24,    25,
      26,    27,    28,    29,    30,    10,    11,    12,    13,    14,
      15,    16,    -1,    18,    19,    20,    21,    22,    23,    24,
      25,    26,    27,    28,    29,    30,    11,    12,    13,    14,
      15,    16,    -1,    18,    19,    20,    21,    22,    23,    24,
      25,    26,    27,    28,    29,    30,    72,    73,    74,    75,
      10,    11,    12,    13,    14,    15,    16,    -1,    18,    19,
      20,    21,    22,    23,    24,    25,    26,    27,    28,    29,
      30,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,   107,   108,   109,    -1,    -1,    -1,    -1,    10,    11,
      12,    13,    14,    15,    16,   250,    18,    19,    20,    21,
      22,    23,    24,    25,    26,    27,    28,    29,    30,    -1,
      26,    -1,    -1,    -1,    -1,    -1,   250,    -1,    -1,    -1,
      36,    37,    38,    39,    40,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    54,    -1,
      -1,    -1,   249,    59,    -1,    -1,    -1,    63,    -1,    65,
      66,    67,    68,    69,    70,    71,    72,    -1,    -1,    -1,
      76,    77,    78,    79,    80,    81,    82,    83,    84,    85,
      86,    87,    88,    89,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,   249,    98,    -1,   100,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,   108,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,   249,    -1,    -1,    -1,   122,   123,    -1,    -1,
     236,   237,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,   231,    -1,    -1,    -1,
      -1,    -1,   148,   149,    -1,    -1,    -1,    -1,   154,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,   206,    -1,    -1,    -1,
      -1,    -1,    -1,   169,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,   177,   178,   179,    -1,    -1,    -1,    -1,    -1,   185,
      -1,    -1,   188,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,   206,    -1,    -1,    -1,    -1,    -1,
      -1,   207,    -1,    -1,    -1,    -1,    10,    11,    12,    13,
      14,    15,    16,   219,    18,    19,    20,    21,    22,    23,
      24,    25,    26,    27,    28,    29,    30,    10,    11,    12,
      13,    14,    15,    16,    -1,    18,    19,    20,    21,    22,
      23,    24,    25,    26,    27,    28,    29,    30,    10,    11,
      12,    13,    14,    15,    16,    -1,    18,    19,    20,    21,
      22,    23,    24,    25,    26,    27,    28,    29,    30,    10,
      11,    12,    13,    14,    15,    16,    -1,    18,    19,    20,
      21,    22,    23,    24,    25,    26,    27,    28,    29,    30,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,   101,    39,    -1,
      -1,    -1,    10,    11,    12,    13,    14,    15,    16,    92,
      18,    19,    20,    21,    22,    23,    24,    25,    26,    27,
      28,    29,    30,    -1,    -1,    -1,    -1,    -1,    36,    -1,
      92,    -1,    10,    11,    12,    13,    14,    15,    16,    80,
      18,    19,    20,    21,    22,    23,    24,    25,    26,    27,
      28,    29,    30,    12,    13,    14,    15,    16,    -1,    18,
      19,    20,    21,    22,    23,    24,    25,    26,    27,    28,
      29,    30,    13,    14,    15,    16,    -1,    18,    19,    20,
      21,    22,    23,    24,    25,    26,    27,    28,    29,    30
};

/* YYSTOS[STATE-NUM] -- The (internal number of the) accessing
   symbol of state STATE-NUM.  */
static const yytype_uint16 yystos[] =
{
       0,   182,   252,   260,   261,     3,     4,     8,   253,   271,
     272,     0,   260,   246,   247,   101,   250,   246,     3,   253,
     274,   275,   272,     3,    38,   273,   232,   250,   262,     3,
       3,     4,     5,     6,     7,     8,    17,    18,    25,    43,
      50,   240,   241,   242,   243,   244,   245,   253,   254,   275,
     104,   263,   247,   248,     9,   254,   254,   254,   254,   231,
     254,   259,   248,   248,   248,   248,   248,   248,    10,    11,
      12,    13,    14,    15,    16,    18,    19,    20,    21,    22,
      23,    24,    25,    26,    27,    28,    29,    30,    47,   105,
     266,     3,   254,   255,   256,   254,   254,   259,    90,    92,
     231,   261,   254,   255,    46,   126,   213,   255,   257,   254,
     254,    26,   254,   254,   254,   254,   254,   254,     6,    18,
     149,   248,    13,    14,    16,   254,    33,    37,   188,   248,
     254,   254,   254,   254,   254,   254,   254,   254,   254,   254,
     254,   254,   264,   254,   156,   267,   250,   249,   206,    90,
      92,   254,   254,   249,   101,   249,   249,   254,   250,   250,
     249,   249,     6,   149,   255,   261,   254,   254,   248,    36,
     248,   248,   248,   261,    39,    80,   265,   250,    47,   129,
     268,   255,   254,   254,    92,   206,   254,   101,   119,   258,
     258,   249,   249,   255,   261,   254,   261,   261,   261,   249,
     254,   264,   254,   120,   269,    92,   254,    98,   249,   255,
     254,   249,   249,   249,   249,   249,   249,   249,   265,   250,
       3,   270,   254,   249,    72,    73,    74,    75,   107,   108,
     109,   236,   237,   254,   250,   249,     3
};

#define yyerrok		(yyerrstatus = 0)
#define yyclearin	(yychar = YYEMPTY)
#define YYEMPTY		(-2)
#define YYEOF		0

#define YYACCEPT	goto yyacceptlab
#define YYABORT		goto yyabortlab
#define YYERROR		goto yyerrorlab


/* Like YYERROR except do call yyerror.  This remains here temporarily
   to ease the transition to the new meaning of YYERROR, for GCC.
   Once GCC version 2 has supplanted version 1, this can go.  However,
   YYFAIL appears to be in use.  Nevertheless, it is formally deprecated
   in Bison 2.4.2's NEWS entry, where a plan to phase it out is
   discussed.  */

#define YYFAIL		goto yyerrlab
#if defined YYFAIL
  /* This is here to suppress warnings from the GCC cpp's
     -Wunused-macros.  Normally we don't worry about that warning, but
     some users do, and we want to make it easy for users to remove
     YYFAIL uses, which will produce warnings from Bison 2.5.  */
#endif

#define YYRECOVERING()  (!!yyerrstatus)

#define YYBACKUP(Token, Value)					\
do								\
  if (yychar == YYEMPTY && yylen == 1)				\
    {								\
      yychar = (Token);						\
      yylval = (Value);						\
      YYPOPSTACK (1);						\
      goto yybackup;						\
    }								\
  else								\
    {								\
      yyerror (YY_("syntax error: cannot back up")); \
      YYERROR;							\
    }								\
while (YYID (0))


#define YYTERROR	1
#define YYERRCODE	256


/* YYLLOC_DEFAULT -- Set CURRENT to span from RHS[1] to RHS[N].
   If N is 0, then set CURRENT to the empty location which ends
   the previous symbol: RHS[0] (always defined).  */

#define YYRHSLOC(Rhs, K) ((Rhs)[K])
#ifndef YYLLOC_DEFAULT
# define YYLLOC_DEFAULT(Current, Rhs, N)				\
    do									\
      if (YYID (N))                                                    \
	{								\
	  (Current).first_line   = YYRHSLOC (Rhs, 1).first_line;	\
	  (Current).first_column = YYRHSLOC (Rhs, 1).first_column;	\
	  (Current).last_line    = YYRHSLOC (Rhs, N).last_line;		\
	  (Current).last_column  = YYRHSLOC (Rhs, N).last_column;	\
	}								\
      else								\
	{								\
	  (Current).first_line   = (Current).last_line   =		\
	    YYRHSLOC (Rhs, 0).last_line;				\
	  (Current).first_column = (Current).last_column =		\
	    YYRHSLOC (Rhs, 0).last_column;				\
	}								\
    while (YYID (0))
#endif


/* This macro is provided for backward compatibility. */

#ifndef YY_LOCATION_PRINT
# define YY_LOCATION_PRINT(File, Loc) ((void) 0)
#endif


/* YYLEX -- calling `yylex' with the right arguments.  */

#ifdef YYLEX_PARAM
# define YYLEX yylex (YYLEX_PARAM)
#else
# define YYLEX yylex ()
#endif

/* Enable debugging if requested.  */
#if YYDEBUG

# ifndef YYFPRINTF
#  include <stdio.h> /* INFRINGES ON USER NAME SPACE */
#  define YYFPRINTF fprintf
# endif

# define YYDPRINTF(Args)			\
do {						\
  if (yydebug)					\
    YYFPRINTF Args;				\
} while (YYID (0))

# define YY_SYMBOL_PRINT(Title, Type, Value, Location)			  \
do {									  \
  if (yydebug)								  \
    {									  \
      YYFPRINTF (stderr, "%s ", Title);					  \
      yy_symbol_print (stderr,						  \
		  Type, Value); \
      YYFPRINTF (stderr, "\n");						  \
    }									  \
} while (YYID (0))


/*--------------------------------.
| Print this symbol on YYOUTPUT.  |
`--------------------------------*/

/*ARGSUSED*/
#if (defined __STDC__ || defined __C99__FUNC__ \
     || defined __cplusplus || defined _MSC_VER)
static void
yy_symbol_value_print (FILE *yyoutput, int yytype, YYSTYPE const * const yyvaluep)
#else
static void
yy_symbol_value_print (yyoutput, yytype, yyvaluep)
    FILE *yyoutput;
    int yytype;
    YYSTYPE const * const yyvaluep;
#endif
{
  if (!yyvaluep)
    return;
# ifdef YYPRINT
  if (yytype < YYNTOKENS)
    YYPRINT (yyoutput, yytoknum[yytype], *yyvaluep);
# else
  YYUSE (yyoutput);
# endif
  switch (yytype)
    {
      default:
	break;
    }
}


/*--------------------------------.
| Print this symbol on YYOUTPUT.  |
`--------------------------------*/

#if (defined __STDC__ || defined __C99__FUNC__ \
     || defined __cplusplus || defined _MSC_VER)
static void
yy_symbol_print (FILE *yyoutput, int yytype, YYSTYPE const * const yyvaluep)
#else
static void
yy_symbol_print (yyoutput, yytype, yyvaluep)
    FILE *yyoutput;
    int yytype;
    YYSTYPE const * const yyvaluep;
#endif
{
  if (yytype < YYNTOKENS)
    YYFPRINTF (yyoutput, "token %s (", yytname[yytype]);
  else
    YYFPRINTF (yyoutput, "nterm %s (", yytname[yytype]);

  yy_symbol_value_print (yyoutput, yytype, yyvaluep);
  YYFPRINTF (yyoutput, ")");
}

/*------------------------------------------------------------------.
| yy_stack_print -- Print the state stack from its BOTTOM up to its |
| TOP (included).                                                   |
`------------------------------------------------------------------*/

#if (defined __STDC__ || defined __C99__FUNC__ \
     || defined __cplusplus || defined _MSC_VER)
static void
yy_stack_print (yytype_int16 *yybottom, yytype_int16 *yytop)
#else
static void
yy_stack_print (yybottom, yytop)
    yytype_int16 *yybottom;
    yytype_int16 *yytop;
#endif
{
  YYFPRINTF (stderr, "Stack now");
  for (; yybottom <= yytop; yybottom++)
    {
      int yybot = *yybottom;
      YYFPRINTF (stderr, " %d", yybot);
    }
  YYFPRINTF (stderr, "\n");
}

# define YY_STACK_PRINT(Bottom, Top)				\
do {								\
  if (yydebug)							\
    yy_stack_print ((Bottom), (Top));				\
} while (YYID (0))


/*------------------------------------------------.
| Report that the YYRULE is going to be reduced.  |
`------------------------------------------------*/

#if (defined __STDC__ || defined __C99__FUNC__ \
     || defined __cplusplus || defined _MSC_VER)
static void
yy_reduce_print (YYSTYPE *yyvsp, int yyrule)
#else
static void
yy_reduce_print (yyvsp, yyrule)
    YYSTYPE *yyvsp;
    int yyrule;
#endif
{
  int yynrhs = yyr2[yyrule];
  int yyi;
  unsigned long int yylno = yyrline[yyrule];
  YYFPRINTF (stderr, "Reducing stack by rule %d (line %lu):\n",
	     yyrule - 1, yylno);
  /* The symbols being reduced.  */
  for (yyi = 0; yyi < yynrhs; yyi++)
    {
      YYFPRINTF (stderr, "   $%d = ", yyi + 1);
      yy_symbol_print (stderr, yyrhs[yyprhs[yyrule] + yyi],
		       &(yyvsp[(yyi + 1) - (yynrhs)])
		       		       );
      YYFPRINTF (stderr, "\n");
    }
}

# define YY_REDUCE_PRINT(Rule)		\
do {					\
  if (yydebug)				\
    yy_reduce_print (yyvsp, Rule); \
} while (YYID (0))

/* Nonzero means print parse trace.  It is left uninitialized so that
   multiple parsers can coexist.  */
int yydebug;
#else /* !YYDEBUG */
# define YYDPRINTF(Args)
# define YY_SYMBOL_PRINT(Title, Type, Value, Location)
# define YY_STACK_PRINT(Bottom, Top)
# define YY_REDUCE_PRINT(Rule)
#endif /* !YYDEBUG */


/* YYINITDEPTH -- initial size of the parser's stacks.  */
#ifndef	YYINITDEPTH
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


#if YYERROR_VERBOSE

# ifndef yystrlen
#  if defined __GLIBC__ && defined _STRING_H
#   define yystrlen strlen
#  else
/* Return the length of YYSTR.  */
#if (defined __STDC__ || defined __C99__FUNC__ \
     || defined __cplusplus || defined _MSC_VER)
static YYSIZE_T
yystrlen (const char *yystr)
#else
static YYSIZE_T
yystrlen (yystr)
    const char *yystr;
#endif
{
  YYSIZE_T yylen;
  for (yylen = 0; yystr[yylen]; yylen++)
    continue;
  return yylen;
}
#  endif
# endif

# ifndef yystpcpy
#  if defined __GLIBC__ && defined _STRING_H && defined _GNU_SOURCE
#   define yystpcpy stpcpy
#  else
/* Copy YYSRC to YYDEST, returning the address of the terminating '\0' in
   YYDEST.  */
#if (defined __STDC__ || defined __C99__FUNC__ \
     || defined __cplusplus || defined _MSC_VER)
static char *
yystpcpy (char *yydest, const char *yysrc)
#else
static char *
yystpcpy (yydest, yysrc)
    char *yydest;
    const char *yysrc;
#endif
{
  char *yyd = yydest;
  const char *yys = yysrc;

  while ((*yyd++ = *yys++) != '\0')
    continue;

  return yyd - 1;
}
#  endif
# endif

# ifndef yytnamerr
/* Copy to YYRES the contents of YYSTR after stripping away unnecessary
   quotes and backslashes, so that it's suitable for yyerror.  The
   heuristic is that double-quoting is unnecessary unless the string
   contains an apostrophe, a comma, or backslash (other than
   backslash-backslash).  YYSTR is taken from yytname.  If YYRES is
   null, do not copy; instead, return the length of what the result
   would have been.  */
static YYSIZE_T
yytnamerr (char *yyres, const char *yystr)
{
  if (*yystr == '"')
    {
      YYSIZE_T yyn = 0;
      char const *yyp = yystr;

      for (;;)
	switch (*++yyp)
	  {
	  case '\'':
	  case ',':
	    goto do_not_strip_quotes;

	  case '\\':
	    if (*++yyp != '\\')
	      goto do_not_strip_quotes;
	    /* Fall through.  */
	  default:
	    if (yyres)
	      yyres[yyn] = *yyp;
	    yyn++;
	    break;

	  case '"':
	    if (yyres)
	      yyres[yyn] = '\0';
	    return yyn;
	  }
    do_not_strip_quotes: ;
    }

  if (! yyres)
    return yystrlen (yystr);

  return yystpcpy (yyres, yystr) - yyres;
}
# endif

/* Copy into *YYMSG, which is of size *YYMSG_ALLOC, an error message
   about the unexpected token YYTOKEN for the state stack whose top is
   YYSSP.

   Return 0 if *YYMSG was successfully written.  Return 1 if *YYMSG is
   not large enough to hold the message.  In that case, also set
   *YYMSG_ALLOC to the required number of bytes.  Return 2 if the
   required number of bytes is too large to store.  */
static int
yysyntax_error (YYSIZE_T *yymsg_alloc, char **yymsg,
                yytype_int16 *yyssp, int yytoken)
{
  YYSIZE_T yysize0 = yytnamerr (0, yytname[yytoken]);
  YYSIZE_T yysize = yysize0;
  YYSIZE_T yysize1;
  enum { YYERROR_VERBOSE_ARGS_MAXIMUM = 5 };
  /* Internationalized format string. */
  const char *yyformat = 0;
  /* Arguments of yyformat. */
  char const *yyarg[YYERROR_VERBOSE_ARGS_MAXIMUM];
  /* Number of reported tokens (one for the "unexpected", one per
     "expected"). */
  int yycount = 0;

  /* There are many possibilities here to consider:
     - Assume YYFAIL is not used.  It's too flawed to consider.  See
       <http://lists.gnu.org/archive/html/bison-patches/2009-12/msg00024.html>
       for details.  YYERROR is fine as it does not invoke this
       function.
     - If this state is a consistent state with a default action, then
       the only way this function was invoked is if the default action
       is an error action.  In that case, don't check for expected
       tokens because there are none.
     - The only way there can be no lookahead present (in yychar) is if
       this state is a consistent state with a default action.  Thus,
       detecting the absence of a lookahead is sufficient to determine
       that there is no unexpected or expected token to report.  In that
       case, just report a simple "syntax error".
     - Don't assume there isn't a lookahead just because this state is a
       consistent state with a default action.  There might have been a
       previous inconsistent state, consistent state with a non-default
       action, or user semantic action that manipulated yychar.
     - Of course, the expected token list depends on states to have
       correct lookahead information, and it depends on the parser not
       to perform extra reductions after fetching a lookahead from the
       scanner and before detecting a syntax error.  Thus, state merging
       (from LALR or IELR) and default reductions corrupt the expected
       token list.  However, the list is correct for canonical LR with
       one exception: it will still contain any token that will not be
       accepted due to an error action in a later state.
  */
  if (yytoken != YYEMPTY)
    {
      int yyn = yypact[*yyssp];
      yyarg[yycount++] = yytname[yytoken];
      if (!yypact_value_is_default (yyn))
        {
          /* Start YYX at -YYN if negative to avoid negative indexes in
             YYCHECK.  In other words, skip the first -YYN actions for
             this state because they are default actions.  */
          int yyxbegin = yyn < 0 ? -yyn : 0;
          /* Stay within bounds of both yycheck and yytname.  */
          int yychecklim = YYLAST - yyn + 1;
          int yyxend = yychecklim < YYNTOKENS ? yychecklim : YYNTOKENS;
          int yyx;

          for (yyx = yyxbegin; yyx < yyxend; ++yyx)
            if (yycheck[yyx + yyn] == yyx && yyx != YYTERROR
                && !yytable_value_is_error (yytable[yyx + yyn]))
              {
                if (yycount == YYERROR_VERBOSE_ARGS_MAXIMUM)
                  {
                    yycount = 1;
                    yysize = yysize0;
                    break;
                  }
                yyarg[yycount++] = yytname[yyx];
                yysize1 = yysize + yytnamerr (0, yytname[yyx]);
                if (! (yysize <= yysize1
                       && yysize1 <= YYSTACK_ALLOC_MAXIMUM))
                  return 2;
                yysize = yysize1;
              }
        }
    }

  switch (yycount)
    {
# define YYCASE_(N, S)                      \
      case N:                               \
        yyformat = S;                       \
      break
      YYCASE_(0, YY_("syntax error"));
      YYCASE_(1, YY_("syntax error, unexpected %s"));
      YYCASE_(2, YY_("syntax error, unexpected %s, expecting %s"));
      YYCASE_(3, YY_("syntax error, unexpected %s, expecting %s or %s"));
      YYCASE_(4, YY_("syntax error, unexpected %s, expecting %s or %s or %s"));
      YYCASE_(5, YY_("syntax error, unexpected %s, expecting %s or %s or %s or %s"));
# undef YYCASE_
    }

  yysize1 = yysize + yystrlen (yyformat);
  if (! (yysize <= yysize1 && yysize1 <= YYSTACK_ALLOC_MAXIMUM))
    return 2;
  yysize = yysize1;

  if (*yymsg_alloc < yysize)
    {
      *yymsg_alloc = 2 * yysize;
      if (! (yysize <= *yymsg_alloc
             && *yymsg_alloc <= YYSTACK_ALLOC_MAXIMUM))
        *yymsg_alloc = YYSTACK_ALLOC_MAXIMUM;
      return 1;
    }

  /* Avoid sprintf, as that infringes on the user's name space.
     Don't have undefined behavior even if the translation
     produced a string with the wrong number of "%s"s.  */
  {
    char *yyp = *yymsg;
    int yyi = 0;
    while ((*yyp = *yyformat) != '\0')
      if (*yyp == '%' && yyformat[1] == 's' && yyi < yycount)
        {
          yyp += yytnamerr (yyp, yyarg[yyi++]);
          yyformat += 2;
        }
      else
        {
          yyp++;
          yyformat++;
        }
  }
  return 0;
}
#endif /* YYERROR_VERBOSE */

/*-----------------------------------------------.
| Release the memory associated to this symbol.  |
`-----------------------------------------------*/

/*ARGSUSED*/
#if (defined __STDC__ || defined __C99__FUNC__ \
     || defined __cplusplus || defined _MSC_VER)
static void
yydestruct (const char *yymsg, int yytype, YYSTYPE *yyvaluep)
#else
static void
yydestruct (yymsg, yytype, yyvaluep)
    const char *yymsg;
    int yytype;
    YYSTYPE *yyvaluep;
#endif
{
  YYUSE (yyvaluep);

  if (!yymsg)
    yymsg = "Deleting";
  YY_SYMBOL_PRINT (yymsg, yytype, yyvaluep, yylocationp);

  switch (yytype)
    {

      default:
	break;
    }
}


/* Prevent warnings from -Wmissing-prototypes.  */
#ifdef YYPARSE_PARAM
#if defined __STDC__ || defined __cplusplus
int yyparse (void *YYPARSE_PARAM);
#else
int yyparse ();
#endif
#else /* ! YYPARSE_PARAM */
#if defined __STDC__ || defined __cplusplus
int yyparse (void);
#else
int yyparse ();
#endif
#endif /* ! YYPARSE_PARAM */


/* The lookahead symbol.  */
int yychar;

/* The semantic value of the lookahead symbol.  */
YYSTYPE yylval;

/* Number of syntax errors so far.  */
int yynerrs;


/*----------.
| yyparse.  |
`----------*/

#ifdef YYPARSE_PARAM
#if (defined __STDC__ || defined __C99__FUNC__ \
     || defined __cplusplus || defined _MSC_VER)
int
yyparse (void *YYPARSE_PARAM)
#else
int
yyparse (YYPARSE_PARAM)
    void *YYPARSE_PARAM;
#endif
#else /* ! YYPARSE_PARAM */
#if (defined __STDC__ || defined __C99__FUNC__ \
     || defined __cplusplus || defined _MSC_VER)
int
yyparse (void)
#else
int
yyparse ()

#endif
#endif
{
    int yystate;
    /* Number of tokens to shift before error messages enabled.  */
    int yyerrstatus;

    /* The stacks and their tools:
       `yyss': related to states.
       `yyvs': related to semantic values.

       Refer to the stacks thru separate pointers, to allow yyoverflow
       to reallocate them elsewhere.  */

    /* The state stack.  */
    yytype_int16 yyssa[YYINITDEPTH];
    yytype_int16 *yyss;
    yytype_int16 *yyssp;

    /* The semantic value stack.  */
    YYSTYPE yyvsa[YYINITDEPTH];
    YYSTYPE *yyvs;
    YYSTYPE *yyvsp;

    YYSIZE_T yystacksize;

  int yyn;
  int yyresult;
  /* Lookahead token as an internal (translated) token number.  */
  int yytoken;
  /* The variables used to return semantic value and location from the
     action routines.  */
  YYSTYPE yyval;

#if YYERROR_VERBOSE
  /* Buffer for error messages, and its allocated size.  */
  char yymsgbuf[128];
  char *yymsg = yymsgbuf;
  YYSIZE_T yymsg_alloc = sizeof yymsgbuf;
#endif

#define YYPOPSTACK(N)   (yyvsp -= (N), yyssp -= (N))

  /* The number of symbols on the RHS of the reduced rule.
     Keep to zero when no symbol should be popped.  */
  int yylen = 0;

  yytoken = 0;
  yyss = yyssa;
  yyvs = yyvsa;
  yystacksize = YYINITDEPTH;

  YYDPRINTF ((stderr, "Starting parse\n"));

  yystate = 0;
  yyerrstatus = 0;
  yynerrs = 0;
  yychar = YYEMPTY; /* Cause a token to be read.  */

  /* Initialize stack pointers.
     Waste one element of value and location stack
     so that they stay on the same level as the state stack.
     The wasted elements are never initialized.  */
  yyssp = yyss;
  yyvsp = yyvs;

  goto yysetstate;

/*------------------------------------------------------------.
| yynewstate -- Push a new state, which is found in yystate.  |
`------------------------------------------------------------*/
 yynewstate:
  /* In all cases, when you get here, the value and location stacks
     have just been pushed.  So pushing a state here evens the stacks.  */
  yyssp++;

 yysetstate:
  *yyssp = yystate;

  if (yyss + yystacksize - 1 <= yyssp)
    {
      /* Get the current used size of the three stacks, in elements.  */
      YYSIZE_T yysize = yyssp - yyss + 1;

#ifdef yyoverflow
      {
	/* Give user a chance to reallocate the stack.  Use copies of
	   these so that the &'s don't force the real ones into
	   memory.  */
	YYSTYPE *yyvs1 = yyvs;
	yytype_int16 *yyss1 = yyss;

	/* Each stack pointer address is followed by the size of the
	   data in use in that stack, in bytes.  This used to be a
	   conditional around just the two extra args, but that might
	   be undefined if yyoverflow is a macro.  */
	yyoverflow (YY_("memory exhausted"),
		    &yyss1, yysize * sizeof (*yyssp),
		    &yyvs1, yysize * sizeof (*yyvsp),
		    &yystacksize);

	yyss = yyss1;
	yyvs = yyvs1;
      }
#else /* no yyoverflow */
# ifndef YYSTACK_RELOCATE
      goto yyexhaustedlab;
# else
      /* Extend the stack our own way.  */
      if (YYMAXDEPTH <= yystacksize)
	goto yyexhaustedlab;
      yystacksize *= 2;
      if (YYMAXDEPTH < yystacksize)
	yystacksize = YYMAXDEPTH;

      {
	yytype_int16 *yyss1 = yyss;
	union yyalloc *yyptr =
	  (union yyalloc *) YYSTACK_ALLOC (YYSTACK_BYTES (yystacksize));
	if (! yyptr)
	  goto yyexhaustedlab;
	YYSTACK_RELOCATE (yyss_alloc, yyss);
	YYSTACK_RELOCATE (yyvs_alloc, yyvs);
#  undef YYSTACK_RELOCATE
	if (yyss1 != yyssa)
	  YYSTACK_FREE (yyss1);
      }
# endif
#endif /* no yyoverflow */

      yyssp = yyss + yysize - 1;
      yyvsp = yyvs + yysize - 1;

      YYDPRINTF ((stderr, "Stack size increased to %lu\n",
		  (unsigned long int) yystacksize));

      if (yyss + yystacksize - 1 <= yyssp)
	YYABORT;
    }

  YYDPRINTF ((stderr, "Entering state %d\n", yystate));

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

  /* YYCHAR is either YYEMPTY or YYEOF or a valid lookahead symbol.  */
  if (yychar == YYEMPTY)
    {
      YYDPRINTF ((stderr, "Reading a token: "));
      yychar = YYLEX;
    }

  if (yychar <= YYEOF)
    {
      yychar = yytoken = YYEOF;
      YYDPRINTF ((stderr, "Now at end of input.\n"));
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

  /* Discard the shifted token.  */
  yychar = YYEMPTY;

  yystate = yyn;
  *++yyvsp = yylval;

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
| yyreduce -- Do a reduction.  |
`-----------------------------*/
yyreduce:
  /* yyn is the number of a rule to reduce with.  */
  yylen = yyr2[yyn];

  /* If YYLEN is nonzero, implement the default value of the action:
     `$$ = $1'.

     Otherwise, the following line sets YYVAL to garbage.
     This behavior is undocumented and Bison
     users should not rely upon it.  Assigning to YYVAL
     unconditionally makes the parser a bit smaller, and it avoids a
     GCC warning that YYVAL may be used uninitialized.  */
  yyval = yyvsp[1-yylen];


  YY_REDUCE_PRINT (yyn);
  switch (yyn)
    {
        case 4:

/* Line 1806 of yacc.c  */
#line 320 "sql_parser.y"
    { (yyval.strval)=(yyvsp[(1) - (1)].strval);}
    break;

  case 5:

/* Line 1806 of yacc.c  */
#line 321 "sql_parser.y"
    { (yyval.strval)=strcat(strcat((yyvsp[(1) - (3)].strval),"."), (yyvsp[(3) - (3)].strval)); }
    break;

  case 6:

/* Line 1806 of yacc.c  */
#line 322 "sql_parser.y"
    { (yyval.strval)=(yyvsp[(1) - (1)].strval); }
    break;

  case 7:

/* Line 1806 of yacc.c  */
#line 323 "sql_parser.y"
    { (yyval.strval)=(yyvsp[(1) - (1)].strval); }
    break;

  case 9:

/* Line 1806 of yacc.c  */
#line 329 "sql_parser.y"
    { emit("NAME %s", (yyvsp[(1) - (1)].strval)); free((yyvsp[(1) - (1)].strval)); }
    break;

  case 10:

/* Line 1806 of yacc.c  */
#line 330 "sql_parser.y"
    { emit("FIELDNAME %s.%s", (yyvsp[(1) - (3)].strval), (yyvsp[(3) - (3)].strval)); free((yyvsp[(1) - (3)].strval)); free((yyvsp[(3) - (3)].strval)); }
    break;

  case 11:

/* Line 1806 of yacc.c  */
#line 331 "sql_parser.y"
    { emit("USERVAR %s", (yyvsp[(1) - (1)].strval)); free((yyvsp[(1) - (1)].strval)); }
    break;

  case 12:

/* Line 1806 of yacc.c  */
#line 332 "sql_parser.y"
    { emit("STRING %s", (yyvsp[(1) - (1)].strval)); free((yyvsp[(1) - (1)].strval)); }
    break;

  case 13:

/* Line 1806 of yacc.c  */
#line 333 "sql_parser.y"
    { emit("NUMBER %d", (yyvsp[(1) - (1)].intval)); }
    break;

  case 14:

/* Line 1806 of yacc.c  */
#line 334 "sql_parser.y"
    { emit("FLOAT %g", (yyvsp[(1) - (1)].floatval)); }
    break;

  case 15:

/* Line 1806 of yacc.c  */
#line 335 "sql_parser.y"
    { emit("BOOL %d", (yyvsp[(1) - (1)].intval)); }
    break;

  case 16:

/* Line 1806 of yacc.c  */
#line 338 "sql_parser.y"
    { emit("ADD"); }
    break;

  case 17:

/* Line 1806 of yacc.c  */
#line 339 "sql_parser.y"
    { emit("SUB"); }
    break;

  case 18:

/* Line 1806 of yacc.c  */
#line 340 "sql_parser.y"
    { emit("MUL"); }
    break;

  case 19:

/* Line 1806 of yacc.c  */
#line 341 "sql_parser.y"
    { emit("DIV"); }
    break;

  case 20:

/* Line 1806 of yacc.c  */
#line 342 "sql_parser.y"
    { emit("MOD"); }
    break;

  case 21:

/* Line 1806 of yacc.c  */
#line 343 "sql_parser.y"
    { emit("MOD"); }
    break;

  case 22:

/* Line 1806 of yacc.c  */
#line 344 "sql_parser.y"
    { emit("NEG"); }
    break;

  case 23:

/* Line 1806 of yacc.c  */
#line 345 "sql_parser.y"
    { emit("AND"); }
    break;

  case 24:

/* Line 1806 of yacc.c  */
#line 346 "sql_parser.y"
    { emit("OR"); }
    break;

  case 25:

/* Line 1806 of yacc.c  */
#line 347 "sql_parser.y"
    { emit("XOR"); }
    break;

  case 26:

/* Line 1806 of yacc.c  */
#line 348 "sql_parser.y"
    { emit("BITOR"); }
    break;

  case 27:

/* Line 1806 of yacc.c  */
#line 349 "sql_parser.y"
    { emit("BITAND"); }
    break;

  case 28:

/* Line 1806 of yacc.c  */
#line 350 "sql_parser.y"
    { emit("BITXOR"); }
    break;

  case 29:

/* Line 1806 of yacc.c  */
#line 351 "sql_parser.y"
    { emit("SHIFT %s", (yyvsp[(2) - (3)].subtok)==1?"left":"right"); }
    break;

  case 30:

/* Line 1806 of yacc.c  */
#line 352 "sql_parser.y"
    { emit("NOT"); }
    break;

  case 31:

/* Line 1806 of yacc.c  */
#line 353 "sql_parser.y"
    { emit("NOT"); }
    break;

  case 32:

/* Line 1806 of yacc.c  */
#line 354 "sql_parser.y"
    { emit("CMP %d", (yyvsp[(2) - (3)].subtok)); }
    break;

  case 33:

/* Line 1806 of yacc.c  */
#line 357 "sql_parser.y"
    { emit("CMPSELECT %d", (yyvsp[(2) - (5)].subtok)); }
    break;

  case 34:

/* Line 1806 of yacc.c  */
#line 358 "sql_parser.y"
    { emit("CMPANYSELECT %d", (yyvsp[(2) - (6)].subtok)); }
    break;

  case 35:

/* Line 1806 of yacc.c  */
#line 359 "sql_parser.y"
    { emit("CMPANYSELECT %d", (yyvsp[(2) - (6)].subtok)); }
    break;

  case 36:

/* Line 1806 of yacc.c  */
#line 360 "sql_parser.y"
    { emit("CMPALLSELECT %d", (yyvsp[(2) - (6)].subtok)); }
    break;

  case 37:

/* Line 1806 of yacc.c  */
#line 363 "sql_parser.y"
    { emit("ISNULL"); }
    break;

  case 38:

/* Line 1806 of yacc.c  */
#line 364 "sql_parser.y"
    { emit("ISNULL"); emit("NOT"); }
    break;

  case 39:

/* Line 1806 of yacc.c  */
#line 365 "sql_parser.y"
    { emit("ISBOOL %d", (yyvsp[(3) - (3)].intval)); }
    break;

  case 40:

/* Line 1806 of yacc.c  */
#line 366 "sql_parser.y"
    { emit("ISBOOL %d", (yyvsp[(4) - (4)].intval)); emit("NOT"); }
    break;

  case 41:

/* Line 1806 of yacc.c  */
#line 368 "sql_parser.y"
    { emit("ASSIGN @%s", (yyvsp[(1) - (3)].strval)); free((yyvsp[(1) - (3)].strval)); }
    break;

  case 42:

/* Line 1806 of yacc.c  */
#line 371 "sql_parser.y"
    { emit("BETWEEN"); }
    break;

  case 43:

/* Line 1806 of yacc.c  */
#line 374 "sql_parser.y"
    { (yyval.intval) = 1; }
    break;

  case 44:

/* Line 1806 of yacc.c  */
#line 375 "sql_parser.y"
    { (yyval.intval) = 1 + (yyvsp[(3) - (3)].intval); }
    break;

  case 45:

/* Line 1806 of yacc.c  */
#line 378 "sql_parser.y"
    { (yyval.intval) = 0; }
    break;

  case 47:

/* Line 1806 of yacc.c  */
#line 382 "sql_parser.y"
    { emit("ISIN %d", (yyvsp[(4) - (5)].intval)); }
    break;

  case 48:

/* Line 1806 of yacc.c  */
#line 383 "sql_parser.y"
    { emit("ISIN %d", (yyvsp[(5) - (6)].intval)); emit("NOT"); }
    break;

  case 49:

/* Line 1806 of yacc.c  */
#line 384 "sql_parser.y"
    { emit("CMPANYSELECT 4"); }
    break;

  case 50:

/* Line 1806 of yacc.c  */
#line 385 "sql_parser.y"
    { emit("CMPALLSELECT 3"); }
    break;

  case 51:

/* Line 1806 of yacc.c  */
#line 386 "sql_parser.y"
    { emit("EXISTSSELECT"); if((yyvsp[(1) - (4)].subtok))emit("NOT"); }
    break;

  case 52:

/* Line 1806 of yacc.c  */
#line 390 "sql_parser.y"
    {  emit("CALL %d %s", (yyvsp[(3) - (4)].intval), (yyvsp[(1) - (4)].strval)); free((yyvsp[(1) - (4)].strval)); }
    break;

  case 53:

/* Line 1806 of yacc.c  */
#line 394 "sql_parser.y"
    { emit("COUNTALL"); }
    break;

  case 54:

/* Line 1806 of yacc.c  */
#line 395 "sql_parser.y"
    { emit(" CALL 1 COUNT"); }
    break;

  case 55:

/* Line 1806 of yacc.c  */
#line 397 "sql_parser.y"
    {  emit("CALL %d SUBSTR", (yyvsp[(3) - (4)].intval)); }
    break;

  case 56:

/* Line 1806 of yacc.c  */
#line 398 "sql_parser.y"
    {  emit("CALL 2 SUBSTR"); }
    break;

  case 57:

/* Line 1806 of yacc.c  */
#line 399 "sql_parser.y"
    {  emit("CALL 3 SUBSTR"); }
    break;

  case 58:

/* Line 1806 of yacc.c  */
#line 401 "sql_parser.y"
    { emit("CALL %d TRIM", (yyvsp[(3) - (4)].intval)); }
    break;

  case 59:

/* Line 1806 of yacc.c  */
#line 402 "sql_parser.y"
    { emit("CALL 3 TRIM"); }
    break;

  case 60:

/* Line 1806 of yacc.c  */
#line 405 "sql_parser.y"
    { emit("NUMBER 1"); }
    break;

  case 61:

/* Line 1806 of yacc.c  */
#line 406 "sql_parser.y"
    { emit("NUMBER 2"); }
    break;

  case 62:

/* Line 1806 of yacc.c  */
#line 407 "sql_parser.y"
    { emit("NUMBER 3"); }
    break;

  case 63:

/* Line 1806 of yacc.c  */
#line 410 "sql_parser.y"
    { emit("CALL 3 DATE_ADD"); }
    break;

  case 64:

/* Line 1806 of yacc.c  */
#line 411 "sql_parser.y"
    { emit("CALL 3 DATE_SUB"); }
    break;

  case 65:

/* Line 1806 of yacc.c  */
#line 414 "sql_parser.y"
    { emit("NUMBER 1"); }
    break;

  case 66:

/* Line 1806 of yacc.c  */
#line 415 "sql_parser.y"
    { emit("NUMBER 2"); }
    break;

  case 67:

/* Line 1806 of yacc.c  */
#line 416 "sql_parser.y"
    { emit("NUMBER 3"); }
    break;

  case 68:

/* Line 1806 of yacc.c  */
#line 417 "sql_parser.y"
    { emit("NUMBER 4"); }
    break;

  case 69:

/* Line 1806 of yacc.c  */
#line 418 "sql_parser.y"
    { emit("NUMBER 5"); }
    break;

  case 70:

/* Line 1806 of yacc.c  */
#line 419 "sql_parser.y"
    { emit("NUMBER 6"); }
    break;

  case 71:

/* Line 1806 of yacc.c  */
#line 420 "sql_parser.y"
    { emit("NUMBER 7"); }
    break;

  case 72:

/* Line 1806 of yacc.c  */
#line 421 "sql_parser.y"
    { emit("NUMBER 8"); }
    break;

  case 73:

/* Line 1806 of yacc.c  */
#line 422 "sql_parser.y"
    { emit("NUMBER 9"); }
    break;

  case 74:

/* Line 1806 of yacc.c  */
#line 424 "sql_parser.y"
    { emit("CASEVAL %d 0", (yyvsp[(3) - (4)].intval)); }
    break;

  case 75:

/* Line 1806 of yacc.c  */
#line 425 "sql_parser.y"
    { emit("CASEVAL %d 1", (yyvsp[(3) - (6)].intval)); }
    break;

  case 76:

/* Line 1806 of yacc.c  */
#line 426 "sql_parser.y"
    { emit("CASE %d 0", (yyvsp[(2) - (3)].intval)); }
    break;

  case 77:

/* Line 1806 of yacc.c  */
#line 427 "sql_parser.y"
    { emit("CASE %d 1", (yyvsp[(2) - (5)].intval)); }
    break;

  case 78:

/* Line 1806 of yacc.c  */
#line 430 "sql_parser.y"
    { (yyval.intval) = 1; }
    break;

  case 79:

/* Line 1806 of yacc.c  */
#line 431 "sql_parser.y"
    { (yyval.intval) = (yyvsp[(1) - (5)].intval)+1; }
    break;

  case 80:

/* Line 1806 of yacc.c  */
#line 433 "sql_parser.y"
    { emit("LIKE"); }
    break;

  case 81:

/* Line 1806 of yacc.c  */
#line 434 "sql_parser.y"
    { emit("LIKE"); emit("NOT"); }
    break;

  case 82:

/* Line 1806 of yacc.c  */
#line 437 "sql_parser.y"
    { emit("REGEXP"); }
    break;

  case 83:

/* Line 1806 of yacc.c  */
#line 438 "sql_parser.y"
    { emit("REGEXP"); emit("NOT"); }
    break;

  case 84:

/* Line 1806 of yacc.c  */
#line 441 "sql_parser.y"
    { emit("STRTOBIN"); }
    break;

  case 85:

/* Line 1806 of yacc.c  */
#line 445 "sql_parser.y"
    {}
    break;

  case 86:

/* Line 1806 of yacc.c  */
#line 449 "sql_parser.y"
    { selection((yyvsp[(2) - (10)].sl), (yyvsp[(4) - (10)].tables)); }
    break;

  case 88:

/* Line 1806 of yacc.c  */
#line 453 "sql_parser.y"
    { emit("WHERE"); }
    break;

  case 90:

/* Line 1806 of yacc.c  */
#line 456 "sql_parser.y"
    { emit("GROUPBYLIST %d", (yyvsp[(3) - (3)].intval)); }
    break;

  case 91:

/* Line 1806 of yacc.c  */
#line 460 "sql_parser.y"
    { emit("GROUPBY %d",  (yyvsp[(2) - (2)].intval)); (yyval.intval) = 1; }
    break;

  case 92:

/* Line 1806 of yacc.c  */
#line 462 "sql_parser.y"
    { emit("GROUPBY %d",  (yyvsp[(4) - (4)].intval)); (yyval.intval) = (yyvsp[(1) - (4)].intval) + 1; }
    break;

  case 93:

/* Line 1806 of yacc.c  */
#line 465 "sql_parser.y"
    { (yyval.intval) = 0; }
    break;

  case 94:

/* Line 1806 of yacc.c  */
#line 466 "sql_parser.y"
    { (yyval.intval) = 0; }
    break;

  case 95:

/* Line 1806 of yacc.c  */
#line 467 "sql_parser.y"
    { (yyval.intval) = 1; }
    break;

  case 97:

/* Line 1806 of yacc.c  */
#line 471 "sql_parser.y"
    { emit("HAVING"); }
    break;

  case 99:

/* Line 1806 of yacc.c  */
#line 474 "sql_parser.y"
    { emit("ORDERBY %d", (yyvsp[(3) - (3)].intval)); }
    break;

  case 101:

/* Line 1806 of yacc.c  */
#line 477 "sql_parser.y"
    { emit("LIMIT 1"); }
    break;

  case 102:

/* Line 1806 of yacc.c  */
#line 478 "sql_parser.y"
    { emit("LIMIT 2"); }
    break;

  case 104:

/* Line 1806 of yacc.c  */
#line 482 "sql_parser.y"
    { emit("INTO %d", (yyvsp[(2) - (2)].intval)); }
    break;

  case 105:

/* Line 1806 of yacc.c  */
#line 485 "sql_parser.y"
    { emit("COLUMN %s", (yyvsp[(1) - (1)].strval)); free((yyvsp[(1) - (1)].strval)); (yyval.intval) = 1; }
    break;

  case 106:

/* Line 1806 of yacc.c  */
#line 486 "sql_parser.y"
    { emit("COLUMN %s", (yyvsp[(3) - (3)].strval)); free((yyvsp[(3) - (3)].strval)); (yyval.intval) = (yyvsp[(1) - (3)].intval) + 1; }
    break;

  case 107:

/* Line 1806 of yacc.c  */
#line 489 "sql_parser.y"
    { (yyval.sl)  = new std::vector<Query::Expression*>(); (yyval.sl)->push_back((yyvsp[(1) - (1)].e));}
    break;

  case 108:

/* Line 1806 of yacc.c  */
#line 490 "sql_parser.y"
    {(yyvsp[(1) - (3)].sl)->push_back((yyvsp[(3) - (3)].e)); (yyval.sl) = (yyvsp[(1) - (3)].sl); }
    break;

  case 109:

/* Line 1806 of yacc.c  */
#line 493 "sql_parser.y"
    { (yyval.e) = new Query::UnaryExpression((yyvsp[(1) - (1)].strval)); }
    break;

  case 110:

/* Line 1806 of yacc.c  */
#line 495 "sql_parser.y"
    { (yyval.strval) = (yyvsp[(2) - (2)].strval); }
    break;

  case 111:

/* Line 1806 of yacc.c  */
#line 496 "sql_parser.y"
    { (yyval.strval) = (yyvsp[(1) - (1)].strval); }
    break;

  case 112:

/* Line 1806 of yacc.c  */
#line 497 "sql_parser.y"
    {(yyval.strval)=""; }
    break;

  case 113:

/* Line 1806 of yacc.c  */
#line 500 "sql_parser.y"
    { (yyval.tables) = new Query::Tables((yyvsp[(1) - (1)].strval)) ;}
    break;

  case 114:

/* Line 1806 of yacc.c  */
#line 501 "sql_parser.y"
    { (yyvsp[(1) - (3)].tables)->addTable((yyvsp[(3) - (3)].strval)); (yyval.tables) = (yyvsp[(1) - (3)].tables); }
    break;

  case 115:

/* Line 1806 of yacc.c  */
#line 504 "sql_parser.y"
    {(yyval.strval) = (yyvsp[(1) - (1)].strval); emit("table keyword");}
    break;

  case 116:

/* Line 1806 of yacc.c  */
#line 505 "sql_parser.y"
    { (yyval.strval) = strcat((yyvsp[(1) - (2)].strval), (yyvsp[(2) - (2)].strval)); }
    break;



/* Line 1806 of yacc.c  */
#line 2782 "sql_parser.tab.c"
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
  YY_SYMBOL_PRINT ("-> $$ =", yyr1[yyn], &yyval, &yyloc);

  YYPOPSTACK (yylen);
  yylen = 0;
  YY_STACK_PRINT (yyss, yyssp);

  *++yyvsp = yyval;

  /* Now `shift' the result of the reduction.  Determine what state
     that goes to, based on the state we popped back to and the rule
     number reduced by.  */

  yyn = yyr1[yyn];

  yystate = yypgoto[yyn - YYNTOKENS] + *yyssp;
  if (0 <= yystate && yystate <= YYLAST && yycheck[yystate] == *yyssp)
    yystate = yytable[yystate];
  else
    yystate = yydefgoto[yyn - YYNTOKENS];

  goto yynewstate;


/*------------------------------------.
| yyerrlab -- here on detecting error |
`------------------------------------*/
yyerrlab:
  /* Make sure we have latest lookahead translation.  See comments at
     user semantic actions for why this is necessary.  */
  yytoken = yychar == YYEMPTY ? YYEMPTY : YYTRANSLATE (yychar);

  /* If not already recovering from an error, report this error.  */
  if (!yyerrstatus)
    {
      ++yynerrs;
#if ! YYERROR_VERBOSE
      yyerror (YY_("syntax error"));
#else
# define YYSYNTAX_ERROR yysyntax_error (&yymsg_alloc, &yymsg, \
                                        yyssp, yytoken)
      {
        char const *yymsgp = YY_("syntax error");
        int yysyntax_error_status;
        yysyntax_error_status = YYSYNTAX_ERROR;
        if (yysyntax_error_status == 0)
          yymsgp = yymsg;
        else if (yysyntax_error_status == 1)
          {
            if (yymsg != yymsgbuf)
              YYSTACK_FREE (yymsg);
            yymsg = (char *) YYSTACK_ALLOC (yymsg_alloc);
            if (!yymsg)
              {
                yymsg = yymsgbuf;
                yymsg_alloc = sizeof yymsgbuf;
                yysyntax_error_status = 2;
              }
            else
              {
                yysyntax_error_status = YYSYNTAX_ERROR;
                yymsgp = yymsg;
              }
          }
        yyerror (yymsgp);
        if (yysyntax_error_status == 2)
          goto yyexhaustedlab;
      }
# undef YYSYNTAX_ERROR
#endif
    }



  if (yyerrstatus == 3)
    {
      /* If just tried and failed to reuse lookahead token after an
	 error, discard it.  */

      if (yychar <= YYEOF)
	{
	  /* Return failure if at end of input.  */
	  if (yychar == YYEOF)
	    YYABORT;
	}
      else
	{
	  yydestruct ("Error: discarding",
		      yytoken, &yylval);
	  yychar = YYEMPTY;
	}
    }

  /* Else will try to reuse lookahead token after shifting the error
     token.  */
  goto yyerrlab1;


/*---------------------------------------------------.
| yyerrorlab -- error raised explicitly by YYERROR.  |
`---------------------------------------------------*/
yyerrorlab:

  /* Pacify compilers like GCC when the user code never invokes
     YYERROR and the label yyerrorlab therefore never appears in user
     code.  */
  if (/*CONSTCOND*/ 0)
     goto yyerrorlab;

  /* Do not reclaim the symbols of the rule which action triggered
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
  yyerrstatus = 3;	/* Each real token shifted decrements this.  */

  for (;;)
    {
      yyn = yypact[yystate];
      if (!yypact_value_is_default (yyn))
	{
	  yyn += YYTERROR;
	  if (0 <= yyn && yyn <= YYLAST && yycheck[yyn] == YYTERROR)
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
		  yystos[yystate], yyvsp);
      YYPOPSTACK (1);
      yystate = *yyssp;
      YY_STACK_PRINT (yyss, yyssp);
    }

  *++yyvsp = yylval;


  /* Shift the error token.  */
  YY_SYMBOL_PRINT ("Shifting", yystos[yyn], yyvsp, yylsp);

  yystate = yyn;
  goto yynewstate;


/*-------------------------------------.
| yyacceptlab -- YYACCEPT comes here.  |
`-------------------------------------*/
yyacceptlab:
  yyresult = 0;
  goto yyreturn;

/*-----------------------------------.
| yyabortlab -- YYABORT comes here.  |
`-----------------------------------*/
yyabortlab:
  yyresult = 1;
  goto yyreturn;

#if !defined(yyoverflow) || YYERROR_VERBOSE
/*-------------------------------------------------.
| yyexhaustedlab -- memory exhaustion comes here.  |
`-------------------------------------------------*/
yyexhaustedlab:
  yyerror (YY_("memory exhausted"));
  yyresult = 2;
  /* Fall through.  */
#endif

yyreturn:
  if (yychar != YYEMPTY)
    {
      /* Make sure we have latest lookahead translation.  See comments at
         user semantic actions for why this is necessary.  */
      yytoken = YYTRANSLATE (yychar);
      yydestruct ("Cleanup: discarding lookahead",
                  yytoken, &yylval);
    }
  /* Do not reclaim the symbols of the rule which action triggered
     this YYABORT or YYACCEPT.  */
  YYPOPSTACK (yylen);
  YY_STACK_PRINT (yyss, yyssp);
  while (yyssp != yyss)
    {
      yydestruct ("Cleanup: popping",
		  yystos[*yyssp], yyvsp);
      YYPOPSTACK (1);
    }
#ifndef yyoverflow
  if (yyss != yyssa)
    YYSTACK_FREE (yyss);
#endif
#if YYERROR_VERBOSE
  if (yymsg != yymsgbuf)
    YYSTACK_FREE (yymsg);
#endif
  /* Make sure YYID is used.  */
  return YYID (yyresult);
}



/* Line 2067 of yacc.c  */
#line 587 "sql_parser.y"

void
emit(const char *s, ...)
{
  va_list ap;
  va_start(ap, s);

  printf("rpn: ");
  vfprintf(stdout, s, ap);
  printf("\n");
}


void selection(std::vector<Query::Expression*> *sl, Query::Tables *tab)
{
    Query::Selection s;
    s._toSelect = *sl;
    s._selectFrom = tab;
    std::vector<Extraction::Extraction> extracts = s.getExtractions();

    *extract_archive << extracts;
}


void
yyerror(const char *s, ...)
{
  va_list ap;
  va_start(ap, s);

  fprintf(stderr, "%d: error: ", yylineno);
  vfprintf(stderr, s, ap);
  fprintf(stderr, "\n");
}


void format()
{
    printf("sql_parser <query>\n");
}

int main(int argc, char **argv)
{
    //yydebug = 1;
  extern FILE *yyin;
  if (argc < 2)
  {
      format();
      return -1;
  }

  extract_archive = new boost::archive::text_oarchive(std::cout);

  if((yyin = fopen(argv[1], "r")) == NULL) {
    perror(argv[1]);
    exit(1);
  }

  yyparse();
}

