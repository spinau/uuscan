// uuscan.h - light-weight helper functions for recursive descent parsing
// uses _Generic selector, requires C11 or later
// github.com/spinau/uuscan

/*{{{ uuscan.h exports; names beginning with underscore not meant for app use

Macros
  accept(t)                             return true if t scan succeeds
  accept(t, &val)                       return true if t scan succeeds, result in val
  acceptall(t1, t2, ...)                return true if all terms succeed
  expect(t)                             "expected" uuerror if t fails
  expect(t, &val)                       if t succeeds, result in val
  expect(t, &val, char *failmsg)        if t fails, uuerror is failmsg
  uuerror(char *fmt, ...)               jump out of parse with an error msg
  on_uuerror                            following statement or block is uuerror target
  fail(char *lp)                        fail out of scan with lp at fail point
  success(char *lp)                     return succesful scan, update uu.lp with lp
  UUTERMINALS X(t1) X(t2) ...           declare terminals
  UUDEFINE(t)                           define scan function to terminal t
  UUDEFINE(t, <type> *v)                with return value ptr
  CHAR(x)                               same as (char)x for use in accept/expect

Functions
  uudebug(char *fmt, ...)               stderr messages if UUDEBUG defined
  char *trimspace(char *)               removes space front and back
  char *skipspace(char *)               advances over front space

Struct
  uu                                    uuscan internals; app must set uu.line and uu.lp
}}}*/
/*{{{ notes
To set up for uu scanning:

    #define UUTERMINALS X(T1) X(T2) ...      // app-specific scan terminals
    #include "uuscan.h"

To initialise input for uuscanning set the following two pointers:

     uu.lp = uu.line = <input string>

If uuerror() is used then define the error longjmp target with:

     on_uuerror {
         ... // e.g. puts(uu.errmsg);
     }

The two main functions provided for scanning elements are:

     accept(x) 

Scans for terminal x: returns true if x is correctly scanned, false if the
next element is not x.

     expect(x)

Scans for terminal x: if the next input is not x then a scanning error is 
raised in the form "x expected at [pos]". If a more application-specific error
message is desired then use a third argument to override the default message.

     expect(integer, &i, "address or unit number");

will produce "expected address or unit number at [pos]" on failure to scan integer.

x is a string literal, char *, char, or char literal, or an application-defined 
terminal name (like integer in the above example). Literal matching is provided 
here for char * and char. The application must provide scanning functions for
app-defined terminals in the same file as uuscan.h is included.

The mechanism to return converted values from a scan back to the caller
uses either an appropriately typed address-of as the second argument, or,
assignment to a UUVAL (union {...} uu) element.

Second argument to accept and expect:

     int i;
     accept(integer, &i)        // pass address-of variable

     float *f;
     expect(floatingpoint, &f)  // pass address-of pointer; derefence in scanner

Define UUVAL union:
    UUVAL { int i; }

Use UUVAL:
    accept(integer);
    if (uu.i > 0)
        ...

A successful scan would typically assign the result to either the dereferenced
address-of argument or the uu element then return sucess(lp) to update uu.lp.

To define app-specific terminals, define UUTERMINALS before including this header,
as shown above. At least one terminal must be defined.

An associated scanning function for each T must also be defined. A convenience
macro UUDEFINE(T) supplies the standard function header that names the scanning
function and provides the necessary arguments.

    UUDEFINE(T)                 // set up fn header for scan terminal T
or
    UUDEFINE(T, <type> *res)    // optional result return ptr
    {
        // initial whitespace has been skipped unless matching on space specifically
        // char *lp is predefined as local ptr to next input
        ...
        return fail(lp); // return false and set uu.lpfail ptr
        ...
        *res = <result of scan>
        return success(lp); // return true and update uu.lp to next char of input
    }

"terminal" is loosely defined. Scanning for a terminal usually means scanning a 
single lexical element, but there is nothing preventing a scanner from processing
more complex forms.

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

If compiled with -DUUDEBUG then uudebugf() output is activated when environment
variabe UUDEBUG is defined.

Sep22-SP simplified from a previous version
Dec23-SP 2nd arg method of value returns; uu.val retired
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

#pragma clang diagnostic ignored "-Wformat-extra-args"
#pragma clang diagnostic ignored "-Wparentheses"
#pragma clang diagnostic ignored "-Wdeprecated-non-prototype"
#pragma clang diagnostic ignored "-Wmain-return-type"
// UUDEFINE(t)/UUDEFINE(t,&v) optional 2nd arg triggers warning:
#pragma clang diagnostic ignored "-Wc2x-extensions"
//}}}

// *** declare terminals in application prior to including uuscan.h: ***
#ifndef UUTERMINALS
#error define UUTERMINALS with 1 or more terminal names using X(..)
#endif

#ifndef inline
#define inline __always_inline
#endif

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
//{{{ UUDEBUG
#ifdef UUDEBUG
#define uudebugf(...) do{                                      \
        if (getenv("UUDEBUG") == NULL) break;                  \
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
    char *msg;          // ptr to message for on_error target; usually local _uumsgbuf
    char *failmsg;      // unreported scanning message back to caller
    void (*callback)(); // if non NULL, callback is called before uuerror() jump is made
                        // allows for clean-up code prior to uuerror message
                        // uuerror() will reset to NULL
    jmp_buf errjmp;     // uuerror() jump target: on_uuerror
#ifdef UUDEBUG
    const char *fn;
    int linenum;
#endif
#ifdef UUVAL
    struct UUVAL;       // converted terminal value temporaries
#endif
} uu;
static char _uumsgbuf[80];

#define UUDEFINE(...)      _uudefine(VA_COUNT(__VA_ARGS__), __VA_ARGS__)
#define _uudefine(n,...)   CONCAT(_uudefine,n)(__VA_ARGS__)
#define _uudefine0(...)    ;
#define _uudefine1(x)      static bool _scan_##x(char *lp, void *)
#define _uudefine2(x,res)  static bool _scan_##x(char *lp, res)

// autobuild terminal enum constants:
#define X(t)  t=__COUNTER__,
static enum { UUTERMINALS } terms;
#undef X

// enum must be used to save current __COUNTER__ value
enum { UUTERMCOUNT = __COUNTER__ };

// autobuild forward decl of _scan_T_() functions:
#define X(t)  static bool _scan_##t();
UUTERMINALS
#undef X

// autobuild list of ptrs to scanning functions:
static struct uuterm {
    bool (*fn)();
    char *name;
} uuterms[UUTERMCOUNT] = {
#define X(t)  [t]={_scan_##t, #t},
    UUTERMINALS
};
#undef X

// accept() does nothing
// accept(t) call scanner t depending on type selection
// accept(t, &res) call scanner t with appropriate ptr to save successful result

#define accept(...)      _accept(VA_COUNT(__VA_ARGS__), __VA_ARGS__)
#define _accept(n,...)   CONCAT(_accept,n)(__VA_ARGS__)
#define _accept0(...)    ;
#define _accept1(x)      __accept(x, NULL)
#define _accept2(x,res)  __accept(x, res)

#if UUDEBUG
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

#define CONCAT(a,b)         a ## b
// macros for supporting acceptall(...) with up to 5 terminals
#define _ACCEPT4(t,...)     __accept(t,NULL) && _ACCEPT3(__VA_ARGS__)
#define _ACCEPT3(t,...)     __accept(t,NULL) && _ACCEPT2(__VA_ARGS__)
#define _ACCEPT2(t,...)     __accept(t,NULL) && _ACCEPT1(__VA_ARGS__)
#define _ACCEPT1(t,...)     __accept(t,NULL) && _ACCEPT0(__VA_ARGS__)
#define _ACCEPT0(t,...)     __accept(t,NULL)
#define _ACCEPTALL(n,t,...) CONCAT(_ACCEPT,n)(t, __VA_ARGS__)

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
#define _fail(n,...)        CONCAT(_fail_, n)

#define fail(...)           _fail(VA_COUNT(__VA_ARGS__))(__VA_ARGS__)
#define success(x)          (uu.lp=(x), true)
#define expect(...)         _expect(VA_COUNT(__VA_ARGS__), __VA_ARGS__)

#define _expect(n,...)      CONCAT(_expect,n)(__VA_ARGS__)
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

static inline char *
skipspace(char *cp)
{
    while (isspace(*cp))
        ++cp;
    return cp;
}

static inline char *
trimspace(char *cp)
{
    cp = skipspace(cp);
    char *end = cp + strlen(cp) - 1;
    while (cp < end && isspace(*end))
        --end;
    *(end+1) = '\0';
    return cp;
}

// scan for a single char
static inline bool
__scan_char(char wanted, char *lp, void *res)
{
#if UUDEBUG
    if (isprint(wanted))
        uudebugf("scan_char '%c'", wanted);
    else
        uudebugf("scan_char '\\%03o'", wanted);
#endif

    if (isspace(wanted) && isspace(*lp)) {
        ++lp;
        if (res) *(char *)res = wanted;
        return success(lp);
    }

    lp = skipspace(lp);

    if (*lp == wanted) {
        if (*lp) // don't incr past null char
            ++lp;
        if (res) *(char *)res = wanted;
        return success(lp);
    }

    return fail(lp);
}

// scan for an app-defined terminal index x
static inline bool
__scan_term(int x, char *lp, void *res) 
{
    lp = skipspace(lp);
    uu.lpfail = uu.lpstart = lp;
    uu.failmsg = NULL;
    uu.len = 0;
#if UUDEBUG
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
    if (msg == NULL)
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
}

// accept('x') -- a char constant is promoted to int and would select
// __scan_term in _Generic, so casting to char is required for char literals:
// accept((char)'x'), or use convenience macros:

#define CHAR(x) (char)x

// e.g.
// #define EQ  CHAR('=')
// ...
// accept(CHAR('*'));
// expect(EQ);
