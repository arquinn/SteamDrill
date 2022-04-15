/* A Bison parser, made by GNU Bison 2.5.  */

/* Bison interface for Yacc-like parsers in C
   
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

/* Line 2068 of yacc.c  */
#line 32 "sql_parser.y"

    int intval;
    double floatval;
    char *strval;
    int subtok;

    std::vector<Query::Expression*> *sl;
    Query::Expression *e;
    Query::Tables *tables;



/* Line 2068 of yacc.c  */
#line 299 "sql_parser.tab.h"
} YYSTYPE;
# define YYSTYPE_IS_TRIVIAL 1
# define yystype YYSTYPE /* obsolescent; will be withdrawn */
# define YYSTYPE_IS_DECLARED 1
#endif

extern YYSTYPE yylval;


