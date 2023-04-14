// uuscan.h - light-weight helper functions for recursive descent parsing
//{{{ notes
// the two main functions provided are:

// accept(x) -- scan for terminal x: return true if x is correctly scanned, 
// false if the next element is not x.

// expect(x) -- scan for terminal x: if the next element is not x then a 
// scanning error is raised of the form "x expected at [pos]". if a more
// application-specific error message is desired then use a second argument to override
// the default message. for example expect(integer, "address or unit number");
// will produce "expected address or unit number at [pos]" on failure to scan integer.

// in accept(x) and expect(x), x is a string literal, char *, char, or char literal, 
// or an application-defined terminal name. literal matching is provided here, 
// terminal matching functions are statically defined in the same file as 
// uuscan.h is included.

// struct uuscan uu is the interface between accept() expect() and an application
// holding ptrs to the line being scanned, conversion values, error message, etc.

// to define app-specific terminals, define UUTERMINALS before including this header. 
// at least one terminal must be defined.

// UUTERMINALS for one or more terminals T are defined in this form:
// #define UUTERMINALS X(_T_) ...

// an associated 'bool scan_T_(char *lp)' function must also be defined that will
// return true or false according to a successful or failed scan -- see example.c
// underscores are optional but serve to identify a terminal type.

// "terminal" is loosely defined. scanning for a terminal usually means
// scanning a single lexical element, but there is nothing preventing a scanner
// from processing more complex forms.

// syntax and conversion error handling is done with uuerror() formatted error 
// messaging which also executes a longjmp to the on_uuerror { ... } block where 
// the error message can be printed and dealt with. this allows errors to be raised
// even in deeply nested or recursed parsing without having to unwind the calls 
// programmatically. the on_uuerror {..} block can print a helpful error message 
// and either exit(1) or drop through to read the next input line.

// because uuscan.h sets up all terminals statically at compile-time this method
// is best suited to a single-source file for a particular parsing job, at least
// the part requiring the accept/expect's. this file-separation also allows multiple
// parsing each with different terminal sets to coexist within one executable.

// (this could of course be redesigned to allow for dynamic terminal creation
// and requiring an init function--but that is "extra weight" for this light-weight
// approach)

// if compiled with -DUUDEBUG then uudebugf() output is activated

// Sep22-SP simplified from a previous version
//}}}
//{{{ includes
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
//}}}

// define these in application source prior to including uuscan.h:
// UUTERMINALS must have at least 1 terminal name declared
// UUVAL is not required if the scanning functions do not convert values
#ifndef UUTERMINALS
#error define UUTERMINALS with 1 or more terminal names using X(..)
#endif

#ifndef inline
#define inline __always_inline
#endif

#ifdef UUDEBUG
void
printesc(char *s, FILE *f)
{
    for ( ; *s; ++s) {
        if (isprint(*s))
            fputc(*s, f);
        else {
            fputc('\\', f);
            char in[]  = { '\a', '\b', '\f', '\n', '\r', '\t', '\v', '\0' };
            char out[] = {  'a',  'b',  'f',  'n',  'r',  't',  'v' };
            char *cp;
            if (cp = strchr(in, *s))
                fputc(out[cp-in], f);
            else
                fprintf(f, "%03d", *s);
        }
    }
}

#define uudebugf(...) fprintf(stderr, "%s %d: ", uu.fn, uu.linenum);\
                      fprintf(stderr, __VA_ARGS__); \
                      fprintf(stderr, " lp=["); \
                      printesc(uu.lp, stderr); \
                      fputc(']', stderr); \
                      fputc('\n', stderr); 
#else
#define uudebugf(...) /**/
#endif

static struct uuscan {
    char *line;     // ptr to current line being scanned
    char *lp;       // advancing ptr into line updated after scan by accept(),expect()
    char *lpstart;  // start of matched element
    char *lpfail;   // scan failed ptr into line
    int len;        // length of successfully scanned element
    char *msg;      // ptr to message for on_error target to print; usually local uumsgbuf
    jmp_buf errjmp; // longjmp target for uuerror()
#ifdef UUDEBUG
    const char *fn;
    int linenum;
#endif
#ifdef UUVAL
    struct UUVAL;    // app defined converted terminal values
#endif
} uu;
static char uumsgbuf[80];

#define UUDEFINE(x) static bool scan##x(char *lp)

// autobuild terminal enum constants:
#define X(t) t=__COUNTER__,
static enum { UUTERMINALS } terms;
#undef X

// enum must be used to save current __COUNTER__ value
enum { UUTERMCOUNT = __COUNTER__ };

// autobuild forward decl of scan_T_() functions:
#define X(t) static bool scan##t();
UUTERMINALS
#undef X

// autobuild list of ptrs to scanning functions:
static struct uuterm {
    bool (*fn)();
    char *name;
} uuterms[UUTERMCOUNT] = {
#define X(t) [t]={scan##t, #t},
    UUTERMINALS
};
#undef X

#ifdef UUDEBUG
#define accept(x) (uu.fn=__FUNCTION__, uu.linenum=__LINE__, _Generic(x, \
    const char*: scan_literal, \
    char*: scan_literal, \
    char: scan_char, \
    int: scan_term, \
    default: _unknown)(x, uu.lp))
#else
#define accept(x) _Generic(x, \
    const char*: scan_literal, \
    char*: scan_literal, \
    char: scan_char, \
    int: scan_term, \
    default: _unknown)(x, uu.lp)
#endif

#define uuerrorpos() (int)((uu.lpfail - uu.line) + 1)
#define fail(x) (uu.lpfail=(x), false)
#define success(x) (uu.lp=(x), true)

static inline char *
skipspace(char *cp)
{
    while (isspace(*cp))
        ++cp;
    return cp;
}

static inline void msg_str(char *s, int vacout, ...)
{
    sprintf(uu.msg, "expected \"%s\" at pos %d", s, uuerrorpos());
}

static inline void msg_char(char c, int vacout, ...)
{
    if (isprint(c))
        sprintf(uu.msg, "expected '%c' at pos %d", c, uuerrorpos());
    else
        sprintf(uu.msg, "expected %x at pos %d", c, uuerrorpos());
}

static void msg_term(int t, int vacount, ...)
{
    char *cp;
    if (vacount) {
        va_list ap;
        va_start(ap, vacount);
        cp = va_arg(ap, char *);
        va_end(ap);
    } else
        cp = uuterms[t].name;
    sprintf(uu.msg, "expected %s at pos %d", cp, uuerrorpos());
}

//{{{ var_arg arg counting
// (gnu c should have built-in __VA_ARGC__ but doesn't...why?)
// this is a hack to count number of arguments in a variadic macro
// works for up to 10 args (last number in _argc_n - 1)
// (note that __VA_ARGC__ must be able to detect zero arguments as well)
#ifndef __VA_ARGC__
#define _argc_n( _1, _2, _3, _4, _5, _6, _7, _8, _9, _10, _11, N, ...) N
#define _argseq 10, 9, 8, 7, 6, 5, 4, 3, 2, 1, 0
#define _argc(...) _argc_n(__VA_ARGS__)
// count the number of arguments:
#define __VA_ARGC__(...) _argc(_, ##__VA_ARGS__, _argseq)
#endif
//}}}

#define expect_msg(x, ...) _Generic(x, \
    const char*: msg_str, \
    char*: msg_str, \
    char: msg_char, \
    int: msg_term, \
    default: _unknown)(x, __VA_ARGC__(__VA_ARGS__),## __VA_ARGS__)

#define expect(x, ...) do{ \
    if (accept(x)==false) { \
        expect_msg(x,## __VA_ARGS__); \
        longjmp(uu.errjmp,1); } \
    }while(0)

#define on_uuerror  uu.msg = uumsgbuf; if (setjmp(uu.errjmp))

#define uuerror(...) do{ \
    sprintf(uu.msg,## __VA_ARGS__, ""); \
    longjmp(uu.errjmp,1); \
    } while(0)

// scan for a single char
static inline bool
scan_char(char wanted, char *lp)
{
    uudebugf("scan_char '%c' 0x%02x", wanted, wanted);

    if (isspace(wanted) && isspace(*lp)) {
        ++lp;
        return success(lp);
    }

    lp = skipspace(lp);

    if (*lp == wanted) {
        ++lp;
        return success(lp);
    }

    return fail(lp);
}

// scan for an app-defined terminal x
static inline bool
scan_term(int x, char *lp) 
{
    uudebugf("scan_term %s", uuterms[x].name);

    lp = skipspace(lp);
    uu.lpfail = uu.lpstart = lp;
    uu.len = 0;
    return (uuterms[x].fn)(lp);
}

// scan for literal text
static bool
scan_literal(const char *wanted, char *lp)
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
    return success(lp);
}

static inline void
_unknown(void *x, char *lp)
{
    uudebugf("unknown scan type");
}

// accept('x') -- a char constant is promoted to int and would trigger 
// scan_term in _Generic, so casting to char is required for char literals:
// accept((char)'x'), or use convenience macros:

#define CHAR(x) (char)x
//#define EQ  CHAR('=')
// e.g. accept(CHAR('*')), expect(EQ), etc. 

