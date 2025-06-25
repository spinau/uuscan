// uuscan.h - light-weight helper functions for recursive descent parsing
// uses _Generic selector, requires C11 or later

/*{{{ uuscan.h exported names
  (names beginning with underscore are not meant for app use)

Macros
  accept(t)                             return true if t scan succeeds
  accept(t, &val)                       return true if t scan succeeds, result in val
  acceptall(t1, t2, ...)                return true if all terms succeed
  expect(t)                             "expected" uuerror if t fails
  expect(t, &val)                       if t succeeds, result in val
  expect(t, &val, char *msg)            if t fails, uuerror reports msg string
  uuerror(char *fmt, ...)               jump out of parse with an error msg
  on_uuerror                            following statement or block is uuerror target
  fail(char *lp)                        fail out of scan with lp at fail point
  success(char *lp)                     return succesful scan, update uu.lp with lp
  UUTERMINALS X(t1,typ) ...             declare terminals with a result type
  UUTERMINALS X(t1) ...                 declare terminals with no result type
  UUDEFINE(t)                           define scan function to terminal t
  UUDEFINE(t, typ)                      scan function for t with result type
  CHAR(x)                               same as (char)x for use in accept/expect

Functions
  uudebug(char *fmt, ...)               stderr messages if DEBUG defined

Struct
  uu                                    uuscan internals; app must set uu.line and uu.lp
}}}*/
/*{{{ usage notes
To set up for uu scanning:

    #define UUTERMINALS X(T1) X(T2) ... // app-specific terminal names T1, T2
    #include "uuscan.h"
    UUDEFINE(T1)
    {
        // scanning function for T1
    }
    UUDEFINE(T2)
    {
        // scanning function for T2
    }

To initialise input for scanning set the following two pointers to the char string
to be scanned:

     uu.lp = uu.line = ...

If uuerror() or expect() is used then define the longjmp error target with:

     on_uuerror {
         ... // e.g. puts(uu.msg);
     }

Scanning can now happen: the two main functions provided for scanning are:

     accept(x) 

Scans for terminal x: returns true if x is correctly scanned (uu.lp is updated),
false if the next element in the input string is not x (uu.lp not updated).

     expect(x)

Scans for terminal x: if the next element is not x then a scanning error is 
raised in the form "x expected at [pos]" (uu.lp is not updated). If a more 
application-specific error message is desired then use a third argument to 
override the default message:

     expect(integer, &i, "address or unit number");
or
     expect(integer, NULL, "address or unit number");

will produce "expected address or unit number at [pos]" on failure to scan integer.

accept(x) and expect(x):

x can be a string literal, char *, char, or char literal, or an application-defined 
terminal name (like integer in the above example). Literal matching is provided 
here for char * and char.

The application must provide scanning functions for app-defined terminals in the
same compile-unit or file as uuscan.h is included.

Return scanned values:

The mechanism to return converted values from a scanning function back to the caller
uses either an appropriately typed address-of as the second argument to accept()
and expect(), or, assignment to a UUVAL (union or struct) element if the second 
argument is omitted.

Using a a second argument to accept() and expect():

    #define UUTERMINALS X(integer, int *) X(floatingpoint, float *)
    ...
    UUDEFINE(integer, int *)
    ...
    int i;
    accept(integer, &i);

    float *f = malloc(sizeof(float));
    expect(floatingpoint, f);

Alternatively (or in addition), define a global UUVAL union or struct before
uuscan.h is included:

    UUVAL struct { int i; }

Using global UUVAL struct:

    accept(integer);            // the scanner for integer will assign uu.i
    if (uu.i > 0)
        ...

A successful scan would typically assign the result to either the dereferenced
address-of argument (if present) or the UUVAL element (if defined) then return 
success(lp) to update the line pointer uu.lp.

Defining terminal names:

To define app-specific terminals, define UUTERMINALS *before* including this header.
At least one terminal must be defined.

An associated scanning function for each T must also be defined. A convenience
macro UUDEFINE(T) or UUDEFINE(T, result_type) supplies the standard function header
that names the scanning function and provides the necessary arguments.

    UUDEFINE(T)              // set fn header for scan terminal T
or
    UUDEFINE(T, res_type)    // optional result return ptr
    {
        // the following variables are predefined:
        char *lp = uu.lp;    // and lp positioned to first non-space char
        res_type res;        // ptr to where result can be copied
        ...
        return fail(lp);    // return false (scan failed)
                            // set uu.lpfail = lp
                            // and uu.lp is left unchanged
        ...
        *res = scanned_value; // assign result value if res_type is used
        return success(lp);   // return true
                              // and uu.lp updated to next char of input
    }

"terminal" is loosely defined. Scanning for a terminal usually means scanning a 
single lexical element, but there is nothing preventing a scanner from processing
more complex forms. Multiple scan values can be returned in a struct.

Error handling:

Syntax and conversion error handling is done with uuerror() with normal printf()
style formatting. uuerror() formats the message string and does a longjmp to the
on_uuerror { ... } block where the error message can be printed or dealt with.

uuerror() allows errors to be raised even in deeply nested or recursed parsing without
having to unwind the calls programmatically. The on_uuerror { ... } block can print
an error message and either exit(1) or drop through to collect the next input line.

Because uuscan.h sets up all terminals statically at compile-time this method
is best suited to a single-source file for a particular parsing job, at least
the part requiring the accept/expect's. This file-separation also allows multiple
parsing each with different terminal sets to coexist within one executable.

If compiled with -DDEBUG then uudebugf() output is activated when environment
variabe DEBUG=uu is defined.

Change Log

Sep22-SP simplified from a previous version
Dec23-SP 2nd arg method of value returns; uu.val retired
May25-SP refactor UUTERMINALS and UUDEFINE to comply with c23 function declarations 
         without parameters as "undetermined" no longer supported. this now requires
         a second argument to X and UUDEFINE to specify the ptr-to-result-value type
         if that method of scan value return is used.

}}}*/
//{{{ includes & clang silencers
#ifndef _STDIO_H
#include <stdio.h>
#endif
#ifndef _STDBOOL_H
#include <stdbool.h>
#endif
#ifndef _SETJMP_H
#include <setjmp.h>
#endif
#ifndef _CTYPE_H
#include <ctype.h>
#endif
#ifndef _STRING_H
#include <string.h>
#endif
#ifndef _STDARG_H
#include <stdarg.h>
#endif

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
static char _uumsgbuf[80];

// generate scanning function header
//      UUDEFINE(terminal_name [, ptr_to_result_value])
// if ptr_to_result_value is omitted, the scanning function either does not
// return a scanned value, or uses the global UUVAL struct (or union)
#define UUDEFINE(...)     _uudefine(VA_COUNT(__VA_ARGS__), __VA_ARGS__)
#define _uudefine(n,...)  _CONCAT(_uudefine,n)(__VA_ARGS__)
#define _uudefine1(T)     static bool _scan_##T(char *lp, void *res)
#define _uudefine2(T,typ) static bool _scan_##T(char *lp, typ res)

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

// accept() does nothing
// accept(t) call scanner t depending on type selection
// accept(t, &res) call scanner t with appropriate ptr to save successful result
#define accept(...)      _accept(VA_COUNT(__VA_ARGS__), __VA_ARGS__)
#define _accept(n,...)   _CONCAT(_accept,n)(__VA_ARGS__)
#define _accept0(...)    ;
#define _accept1(x)      __accept(x, NULL)
#define _accept2(x,res)  __accept(x, res)

#if DEBUG
#define __accept(x,res)                                   \
    (uu.fn=__FUNCTION__, uu.linenum=__LINE__, _Generic(x, \
    const char*: __scan_literal,                          \
    char*: __scan_literal,                                \
    char: __scan_char,                                    \
    int: __scan_term,                                     \
    default: __unknown3) (x, uu.lp, res))
#else
#define __accept(x,res)                    \
    _Generic(x,                            \
    const char*: __scan_literal,           \
    char*: __scan_literal,                 \
    char: __scan_char,                     \
    int: __scan_term,                      \
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
#define acceptall(t,...)                                               \
        ({ char *savelp = uu.lp; bool r=false;                         \
        if (_ACCEPTALL(VA_COUNT(__VA_ARGS__), t, __VA_ARGS__)) r=true; \
        else uu.lp = savelp;                                           \
        r; })

#define uuerrorpos()        (int)((uu.lpfail - uu.line) + 1)

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

#define __expect(x,res,msg) do {        \
    if (accept(x,res)==false) {         \
        _expect_msg(x,msg);             \
        longjmp(uu.errjmp,1); }         \
    }while(0)

#define _expect_msg(x, msg) _Generic(x, \
    const char*: _msg_str,              \
    char*: _msg_str,                    \
    char: _msg_char,                    \
    int: _msg_term,                     \
    default: __unknown2)(x, msg)

#define on_uuerror  uu.msg = _uumsgbuf; if (setjmp(uu.errjmp))

#define uuerror(...) do{                                  \
    sprintf(uu.msg,## __VA_ARGS__, "");                   \
    if (uu.callback) { uu.callback(); uu.callback=NULL; } \
    longjmp(uu.errjmp,1);                                 \
    } while(0)

#ifndef skipspace 
// this skipspace skips all space chars including newline
#define skipspace(s) ({ char *cp = s; while (isspace(*cp)) ++cp; cp; })
#endif

// scan for a single char
inline static inline bool
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

    int l = strlen(wanted);
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

// literal, char, and user-term messages for failed expect()

static void 
_msg_str(char *s, char *msg)
{
    if (msg == NULL)
        sprintf(uu.msg, "expected \"%s\" at pos %d", s, uuerrorpos());
    else
        sprintf(uu.msg, "%s at pos %d", msg, uuerrorpos());
}

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

static void 
_msg_term(int t, char *msg)
{
    sprintf(uu.msg, "%s%s at pos %d", 
            msg==NULL? "expected " : "",
            msg==NULL? uuterms[t].name : msg, uuerrorpos());

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
// e.g.
// #define EQ  CHAR('=')
// ...
// accept(CHAR('*'));
// expect(EQ);
