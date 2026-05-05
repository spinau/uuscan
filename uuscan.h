// uuscan.h - light-weight helper functions scanning and parsing
// uses _Generic selector, requires C11 or later
// 30-09-2022-SP
//{{{ uuscan.h notes
/**

Overview
--------

uuscan provides a set of macros to define and use application-specific
“terminals” (tokens) useful where hand-written scanners and parsing is required.

The model is:

  • The application defines a set of terminal names.
  • For each terminal, the application supplies a scan function.
  • Parsing code uses accept() and expect() to test or require terminals.
  • On failure, control transfers via longjmp to an on_uuerror block.

The framework maintains a single (static) global scan state in struct uu.

Basic Use
---------

1. Declare terminals (before including this header):

    #define UUTERMINALS \
        X(integer, int *) \
        X(ident)

    #include "uuscan.h"

2. Define a scan function for each terminal:

    UUDEFINE(integer, int *val)
    {
        // char *lp = uu.lp;  is pre-declared 
        // int *res = result;  is predeclared if using direct assignment

        ... scan logic ...

        if (scan_failed)
            return fail(lp);

        if (val) *val = value; // where caller may or may not use direct assignment
		uu.i = value; // if caller does not use direct assignment
        return success(lp);
    }

    UUDEFINE(ident)
    {
        // char *lp = uu.lp;  is pre-declared 
        ...
        return success(lp);
    }

3. Initialise input before calling accept() or expect():

    uu.line = uu.lp = input_string;

4. Define error target (if using expect() or uuerror()):

    on_uuerror {
        fprintf(stderr, "%s\n", uu.msg);
    }

5. Parse:

    if (accept(integer, &i)) { ... }

    expect(ident);

Core Parsing Macros
-------------------

accept(t)
    Attempt to scan terminal t.
    Returns true on success (uu.lp advances).
    Returns false on failure (uu.lp unchanged).

accept(t, &val)
    As above, storing result in *val.

expect(t)
    Like accept(t), but raises a parse error if t is not found.

expect(t, &val)
expect(t, &val, msg)
    As above, optionally overriding the default “t expected” message.

acceptall(t1, t2, ...)
    Returns true only if all terms succeed in sequence.

Literal Matching
----------------

In accept(t)/expect(t), t may be:

    • an application-defined terminal (UUTERMINALS)
    • char literal
    • char *
    • string literal

Literal matching is provided for char, char* and string literals.

Use CHAR(x) when passing character expressions to prevent char-to-int promotion:
    accept(CHAR(c));

Returning Values
----------------

There are two mechanisms:

1. Via argument:

    accept(integer, &i);

2. Via global UUVAL:

    #define UUVAL struct { int i; }
    #include "uuscan.h"

    accept(integer);
    printf("%d\n", uu.i);

Scan Function Contract
----------------------

Each UUDEFINE(T) expands to a function with:

    char *lp = skipspace(uu.lp);   // starting position

On failure:
    return fail(lp);
        • uu.lp unchanged
        • uu.lpfail set

On success:
    *res = value; // if applicable
	if (res) *res = value; // if caller uses both direct and UUVAL return mechanism
    return success(lp);
        • uu.lp updated

A “terminal” is not restricted to a single lexical token. A scan function may 
recognise arbitrarily complex forms.

Error Handling
--------------

uuerror(fmt, ...)
    Formats a message and longjmp’s to on_uuerror.

expect() uses uuerror() internally.

This allows immediate exit from deeply nested parsing logic.

Internal State
--------------

struct uu
    Static global scan state. The application must set:

        uu.line  // start of input
        uu.lp    // current position within uu.line

[There are other elements in struct uu that could be of use to an application
not documented here: see the struct definition below for more.]

Names in uuscan.h beginning with '_' are meant as internal use only.

Debugging
---------

If compiled with -DDEBUG and environment variable DEBUG=uu,
uudebug() emits diagnostic output to stderr.

Notes
-----

• Terminals are defined statically at compile time.
• Assumes a single compilation unit per grammar.
• Multiple independent scanners may coexist in one executable
  by separating terminal sets into different source files.
**/
//}}}
//{{{ history
// Sep22 Simplified from first version
// Dec23 Second-arg method of value returns; uu.val retired
// May25 Refactor UUTERMINALS and UUDEFINE to comply with c23 function declarations 
// without parameters as "undetermined" no longer supported. This now requires
// a second argument to X and UUDEFINE to specify the ptr-to-result-value type
// if that method of scan value return is used.
//}}}
//{{{ includes & clang silencers
#include <stdio.h>
#include <stdbool.h>
#include <setjmp.h>
#include <ctype.h>
#include <string.h>
#include <stdarg.h>
#include <stddef.h>

#if defined(__clang__)
#pragma clang diagnostic ignored "-Wformat-extra-args"
#pragma clang diagnostic ignored "-Wparentheses"
#pragma clang diagnostic ignored "-Wdeprecated-non-prototype"
#pragma clang diagnostic ignored "-Wmain-return-type"
// UUDEFINE(t)/UUDEFINE(t,&v) optional 2nd arg triggers warning:
#pragma clang diagnostic ignored "-Wc2x-extensions"
#elif defined(__GNUC__)
#pragma GCC diagnostic ignored "-Wformat-extra-args"
#pragma GCC diagnostic ignored "-Wparentheses"
#endif
//}}}

// *** declare terminals in application prior to including uuscan.h: ***
#ifndef UUTERMINALS
#error define UUTERMINALS with 1 or more terminal names using X(...)
#endif

#ifndef inline
#define inline __always_inline
#endif

#define _CONCAT(a,b)         a ## b

//{{{ VA_COUNT macro
// this is a hack to count number of arguments in a variadic macro
// works for up to 10 args (last number in _argc_n - 1)
// VA_COUNT must be able to detect zero arguments
#ifndef VA_COUNT
#define _ARGC_N( _1, _2, _3, _4, _5, _6, _7, _8, _9, _10, _11, N, ...) N
#define _ARGSEQ 10, 9, 8, 7, 6, 5, 4, 3, 2, 1, 0
#define _ARGC(...) _ARGC_N(__VA_ARGS__)
// count the number of arguments:
#define VA_COUNT(...) _ARGC(_, ##__VA_ARGS__, _ARGSEQ)
#endif
//}}}
//{{{ DEBUG
#ifdef DEBUG
#define uudebugf(...) do{                                      \
        if (getenv("DEBUG") == NULL) break;                    \
        if (strcmp(getenv("DEBUG"), "uu")) break;              \
        fprintf(stderr, "uuscan: %s %d: ", uu.fn, uu.linenum); \
        fprintf(stderr, __VA_ARGS__);                          \
        fprintf(stderr, " lp=[");                              \
        for (char *cp = uu.lp; *cp; ++cp)                      \
            if (isprint(*cp))                                  \
                fputc(*cp, stderr);                            \
            else                                               \
                fprintf(stderr, "\\%03o", *cp);                \
        fputc(']', stderr);                                    \
        fputc('\n', stderr); }while(0)
#else
#define uudebugf(...) /**/
#endif
//}}}

static struct uuscan {
    char *line;         // ptr to current line being scanned
    char *lp;           // advancing ptr into line updated after scan by accept(),expect()
    char *eol;          // ptr to terminating null (quick way to check length)
    char *lpstart;      // start of current input scan
    char *lpfail;       // scan failed ptr into line
    int len;            // length of successfully scanned element
    char ch;            // saves last char literal scanned
    char *msg;          // ptr to message for on_error target; usually local _uumsgbuf
    char *failmsg;      // additional fail message:
                        // appended to expect() fail uuerror message
                        // could also be used after failed accept() by caller
    void (*callback)(); // if non NULL, callback is called before uuerror() jump is made
                        // allows for clean-up code prior to uuerror message
                        // uuerror() will reset to NULL
    jmp_buf errjmp;     // uuerror() jump target: on_uuerror
#ifdef DEBUG
    const char *fn;
    int linenum;
#endif
#ifdef UUVAL
    UUVAL;              // converted terminal value temporaries, examples:
                        // #define UUVAL struct { int i; char *str; }
                        // #define UUVAL union { int i; char *str; }
#endif
} uu;

static char _uumsgbuf[250];

// generate scanning function header
//      UUDEFINE(terminal_name [, ptr_to_result_value])
// if ptr_to_result_value is omitted, the scanning function either does not
// return a scanned value, or uses the global UUVAL struct (or union)
#define UUDEFINE(...)     _uudefine(VA_COUNT(__VA_ARGS__), __VA_ARGS__)
#define _uudefine(n,...)  _CONCAT(_uudefine,n)(__VA_ARGS__)
#define _uudefine1(T)     static bool _scan_##T(char *lp, void *res)
#define _uudefine2(T,typ) static bool _scan_##T(char *lp, typ)

// autobuild terminal enum constants:
#define X(T,...)  T=__COUNTER__,
static enum { UUTERMINALS } terms;
#undef X

// enum must be used to save current __COUNTER__ value
enum { UUTERMCOUNT = __COUNTER__ };

// autobuild forward decl of _scan_T_() functions:
#define X(...)     _x(VA_COUNT(__VA_ARGS__), __VA_ARGS__)
#define _x(n,...)  _CONCAT(_x,n)(__VA_ARGS__)
#define _x1(T)     static bool _scan_##T(char *, void *);
#define _x2(T,typ) static bool _scan_##T(char *, typ);
UUTERMINALS

#undef _x1
#undef _x2

// autobuild list of ptrs to scanning functions for __scan_term() to use:
#define _x1(T)     [T]={ _scan_##T, #T },
#define _x2(T,typ) [T]={ (bool (*)(char *, void *)) _scan_##T, #T },

static struct uuterm {
    bool (*fn)(char *, void *);
    char *name;
} uuterms[UUTERMCOUNT] = {
    UUTERMINALS
};

#undef X
#undef _x
#undef _x1
#undef _x2

// accept() skips over isspace chars only
// accept(t) call scanner t depending on type selection
// accept(t, &res) call scanner t with appropriate ptr to save successful result
#define accept(...)      _accept(VA_COUNT(__VA_ARGS__), __VA_ARGS__)
#define _accept(n,...)   _CONCAT(_accept,n)(__VA_ARGS__)
#define _accept0(...)    uu.lp = skipspace(uu.lp);
#define _accept1(x)      __accept(x, NULL)
#define _accept2(x,res)  __accept(x, res)

#if DEBUG
#define __accept(x,res) \
    (uu.fn=__FUNCTION__, uu.linenum=__LINE__, _Generic(x, \
    const char*: __scan_literal, \
    char*: __scan_literal, \
    char: __scan_char, \
    int: __scan_term, \
    default: __unknown3) (x, uu.lp, res))
#else
#define __accept(x,res) \
    _Generic(x, \
    const char*: __scan_literal, \
    char*: __scan_literal, \
    char: __scan_char, \
    int: __scan_term, \
    default: __unknown3) (x, uu.lp, res)
#endif

// macros for supporting acceptall(...) with up to 5 terminals
#define _ACCEPT4(t,...)     __accept(t,NULL) && _ACCEPT3(__VA_ARGS__)
#define _ACCEPT3(t,...)     __accept(t,NULL) && _ACCEPT2(__VA_ARGS__)
#define _ACCEPT2(t,...)     __accept(t,NULL) && _ACCEPT1(__VA_ARGS__)
#define _ACCEPT1(t,...)     __accept(t,NULL) && _ACCEPT0(__VA_ARGS__)
#define _ACCEPT0(t,...)     __accept(t,NULL)
#define _ACCEPTALL(n,t,...) _CONCAT(_ACCEPT,n)(t, __VA_ARGS__)

// acceptall will call accept() on each argument until failure or all accepted
// acceptall scans only, does not save scan result (result 2nd arg is null)
// if any term fails then uu.lp is unchanged
#define acceptall(t,...) ({ \
        char *savelp = uu.lp; bool r=false; \
        if (_ACCEPTALL(VA_COUNT(__VA_ARGS__), t, __VA_ARGS__)) r=true; \
        else uu.lp = savelp; \
        r; \
})

#define uuerrorpos() (ptrdiff_t)(uu.lpfail - uu.line)

// fail(cp) will return false from scanner with cp pointing to fail position
//
// fail(cp,msg) same as fail(cp), also sets uu.failmsg=msg
// the optional msg is to return to caller a lexical error that has
// been detected but not reported. the caller deals with the error
// either by ignoring, reporting, or recovering as appropriate.
//
// [in general, a fail from a scanner indicates the lexical element
// was not recognised. however, in some cases a lexical error should be caught
// (e.g., an almost well-formed float format) and this is a way of returning 
// an error indication without having to resort to uuerror() immediately]

#define _fail_1(x)          (uu.lpfail=(x), uu.failmsg=NULL, false) // VA_COUNT 1
#define _fail_2(x,y)        (uu.lpfail=(x), uu.failmsg=(y), false)  // VA_COUNT 2
#define _fail(n,...)        _CONCAT(_fail_, n)

#define fail(...)           _fail(VA_COUNT(__VA_ARGS__))(__VA_ARGS__)
#define success(x)          (uu.lp=(x), true)
#define expect(...)         _expect(VA_COUNT(__VA_ARGS__), __VA_ARGS__)

#define _expect(n,...)      _CONCAT(_expect,n)(__VA_ARGS__)
#define _expect1(x)         __expect(x, NULL, NULL)
#define _expect2(x,res)     __expect(x, res, NULL)
#define _expect3(x,res,msg) __expect(x, res, msg)

#define __expect(x,res,msg) \
    do { \
        if (accept(x,res)==false) { \
            _expect_msg(x,msg); \
            longjmp(uu.errjmp,1); } \
    }while(0)

#define _expect_msg(x, msg) \
    _Generic(x, \
    const char*: _msg_str, \
    char*: _msg_str, \
    char: _msg_char, \
    int: _msg_term, \
    default: __unknown2)(x, msg)

#define on_uuerror  uu.msg = _uumsgbuf; if (setjmp(uu.errjmp))

#define uuerror(...) \
    do{ \
        sprintf(uu.msg,## __VA_ARGS__, ""); \
        if (uu.callback) { uu.callback(); uu.callback=NULL; } \
        longjmp(uu.errjmp,1); \
    } while(0)

#ifndef skipspace 
// this skipspace skips all space chars including newline
#define skipspace(s) ({ char *cp = s; while (isspace(*cp)) ++cp; cp; })
#endif

// scan for a single char
static inline bool
__scan_char(char wanted, char *lp, void *res)
{
#if DEBUG
    if (isprint(wanted))
        uudebugf("scan_char '%c'", wanted);
    else
        uudebugf("scan_char '\\%03o'", wanted);
#endif

    if (isspace(wanted) && isspace(*lp)) {
        ++lp;
        if (res)
            *(char *)res = wanted;
        return success(lp);
    }

    lp = skipspace(lp);

    if (*lp == wanted) {
        if (*lp) // don't incr past null char
            ++lp;
        if (res)
            *(char *)res = wanted;
        uu.ch = wanted;
        return success(lp);
    }

    return fail(lp);
}

// scan for an app-defined terminal at index x
static inline bool
__scan_term(int x, char *lp, void *res) 
{
    lp = skipspace(lp);
    uu.lpfail = uu.lpstart = lp;
    uu.failmsg = NULL;
    uu.len = 0;
#if DEBUG
    bool ret = (uuterms[x].fn)(lp, res);
    uudebugf("scan_term %s: %s\n", uuterms[x].name, ret? "success" : "fail");
    return ret;
#else
    return (uuterms[x].fn)(lp, res);
#endif
}

// scan for literal text
static bool
__scan_literal(const char *wanted, char *lp, void *res)
{
    uudebugf("scan_literal \"%s\"", wanted);

    if (*lp == '\0')
        return *wanted=='\0'? success(lp) : fail(lp); // allows for accept("")

    if (!isspace(*wanted)) // if not looking for space, skip over it
        lp = skipspace(lp);

    size_t l = strlen(wanted);
    uu.lpstart = lp;

    if (strlen(lp) < l)
        return fail(lp);

    if (strncmp(wanted, lp, l) == 0) {
        if (isalpha(wanted[l-1]) && isalpha(lp[l]))
            return fail(lp);
        else if (isdigit(wanted[l-1]) && isdigit(lp[l]))
            return fail(lp);
        else // punctuation (not alpha or digit) is a single char match
            lp += l;
    } else
        return fail(lp);

    uu.len = l;
    if (res) {
        // this will be cause of core dumps if res not a ptr to ptr
        char **cp = (char **)res;
        *cp = uu.lpstart;
    }

    return success(lp);
}

// these are never called, they catch unknown type selector in the _Generic(..)
// accept() with unknown type is a compile error
static void __unknown3(void *a, void *b, void *c) {}
static void __unknown2(void *a, void *b) {}

// expect() literal fail message
static void 
_msg_str(char *s, char *msg)
{
    if (msg == NULL)
        sprintf(uu.msg, "expected \"%s\" at pos %d", s, uuerrorpos());
    else
        sprintf(uu.msg, "%s at pos %d", msg, uuerrorpos());
}

// expect() char fail message
static void 
_msg_char(char c, char *msg)
{
    if (msg)
        sprintf(uu.msg, "%s at pos %d", msg, uuerrorpos());
    else {
        if (isprint(c))
            sprintf(uu.msg, "expected '%c' at pos %d", c, uuerrorpos());
        else
            sprintf(uu.msg, "expected '\\%03o' at pos %d", c, uuerrorpos());
    }
}

// expect() user-terminal fail message
static void 
_msg_term(int t, char *msg)
{
	if (msg)
		sprintf(uu.msg, "%s", msg);
	else
		sprintf(uu.msg, "expected %s at pos %d", uuterms[t].name, uuerrorpos());

    if (uu.failmsg) {
        strcat(uu.msg, " (");
        strcat(uu.msg, uu.failmsg);
        strcat(uu.msg, ")");
    }
}

// accept('x') -- a char constant is promoted to int and would select
// __scan_term in _Generic, so casting to char is required for char literals:
// accept((char)'x'), or use convenience macros:

#define CHAR(x) (char)x
#define EOL (char)'\0'
/* e.g.
   #define EQ  CHAR('=')
   ...
   accept(CHAR('*'));
   expect(EQ);
*/
