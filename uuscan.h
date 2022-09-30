// uuscan.h

// provides low-overhead, tokenless scanning of lexical elements for use in
// predictive or recursive descent parsing.

// the two main functions provided are:

// accept(x) -- scan for terminal x: return
// true if x is correctly scanned, false if the next element is not x

// expect(x) -- scan for terminal x: if the next
// element is not x then a scanning error is raised of the form "x expected at [pos]"

// (if compiled with -DUUDEBUG a file name and line number is captured to help
// locate where the last accept()/expect() occurred in source)

// in both cases, x is a string literal (or char *), a char or char literal, 
// or an application-defined terminal name literal matching is provided here, 
// terminal matching is supplied by the app

// struct uuscan uu is the interface between accept() expect() and an application

// to define app-specific terminals, define UUTERMINALS before including this header. 
// at least one terminal must be defined

// UUTERMINALS for one or more terminals T are defined in this form:
// #define UUTERMINALS X(_T_) ...

// an associated scan_T_() function must also be defined that will return
// true or false according to a suucessful or failed scan -- see example.c
// the underscores are optional but serve to identify a terminal type.

// "terminal" is loosely defined. scanning for a terminal usually means
// scanning a single lexical element, but there is nothing preventing a scanner
// from processing more complex forms.

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

#ifndef UUTERMINALS
#error define UUTERMINALS with 1 or more X(..) terminal names
#endif

#ifdef UUDEBUG
#define debugf(...) printf(__VA_ARGS__);putchar('\n');
#else
#define debugf(...) /**/
#endif

static struct uuscan {
    char *line;     // current line being scanned
    char *lp;       // advancing ptr into line updated after scan by accept(),expect()
    char *startlp;  // start of matched element
    int len;        // length of match element
    char *faillp;   // scan failed ptr into line
    char msg[80];   // message str for on_error target to print
    jmp_buf errjmp; // longjmp target for uuerror()
#ifdef UUDEBUG
    char *file;
    int linenum;
#endif
    union {         // converted element from successful scan
        int i;
        char c;
        char *s;
        // add app-specific types here:
        //decimal_t *dec;
        //serial_t date;
        //float f;
    };
} uu;

// terminal enum constants:
#define X(t) t=__COUNTER__,
enum { UUTERMINALS } terms;
#undef X

// enum must be used to save current __COUNTER__ value
enum { MAX_TERMS = __COUNTER__ };

// forward decl of scan_T_() functions:
#define X(t) bool scan##t();
UUTERMINALS
#undef X

// list of ptrs to scanning functions:
static struct uuterm {
    bool (*fn)();
    char *name;
} uuterms[MAX_TERMS] = {
#define X(t) [t]={scan##t, #t},
    UUTERMINALS
};
#undef X

#ifdef UUDEBUG
#define accept(x) (uu.file=__FILE__, uu.linenum=__LINE__, _Generic(x, \
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

#define uuerrorpos() (int)(uu.faillp - uu.line + 1)

static inline void msg_str(char *s)
{
    sprintf(uu.msg, "expected \"%s\" at pos %d", s, uuerrorpos());
}

static inline void msg_char(char c)
{
    if (isprint(c))
        sprintf(uu.msg, "expected '%c' at pos %d", c, uuerrorpos());
    else
        sprintf(uu.msg, "expected %x at pos %d", c, uuerrorpos());
}

static inline void msg_term(int t)
{
    sprintf(uu.msg, "expected %s at pos %d", uuterms[t].name, uuerrorpos());
}

#define expect_msg(x) _Generic(x, \
    const char*: msg_str, \
    char*: msg_str, \
    char: msg_char, \
    int: msg_term, \
    default: _unknown)(x)

#define expect(x) do{ \
    if (accept(x)==false) { \
        expect_msg(x); \
        longjmp(uu.errjmp,1); } \
    }while(0)

#define on_uuerror  if (setjmp(uu.errjmp))

#define uuerror(...) do{ \
    sprintf(uu.msg,## __VA_ARGS__, ""); \
    longjmp(uu.errjmp,1); \
    } while(0)

#define skipspace(x) while (isspace(*x)) ++x
#define fail(x) (uu.faillp=x, false)
#define success(x) (uu.lp=x, true)

// scan for a single char
static inline bool
scan_char(char wanted, char *lp)
{
    debugf("scan_char '%c' 0x%02x, lp=\"%s\"", wanted, wanted, lp);

    if (isspace(wanted) && isspace(*lp)) {
        ++lp;
        return success(lp);
    }

    skipspace(lp);

    if (*lp == wanted)
        return success(++lp);

    return fail(lp);
}

// scan for an app-defined terminal x
static inline bool
scan_term(int x, char *lp) 
{
    debugf("scan_term %s, lp=\"%s\"", uuterms[x].name, lp);

    skipspace(lp);
    uu.startlp = lp;
    uu.faillp = NULL;
    uu.len = 0;
    return (uuterms[x].fn)(lp);
}

// scan for literal text
// [if uuscan.h is included in multiple files of your project
// then this function could be moved out of this header to save
// duplicated code]
static bool
scan_literal(const char *wanted, char *lp)
{
    debugf("scan_literal \"%s\", lp=\"%s\"", wanted, lp);

    if (*lp == '\0')
        return *wanted=='\0'? success(lp) : fail(lp); // allows for accept("")

    if (!isspace(*wanted)) // if not looking for space, skip over it
        skipspace(lp);

    int l = strlen(wanted);
    uu.startlp = lp;

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
#ifdef UUDEBUG
    printf("[%s line %d] ", uu.file, uu.linenum);
#endif
    puts("uuscan unknown scan type");
}

// accept('x') char constant is promoted to int
// and would trigger scan_term in _Generic
// so casting to char is required for char literals:
// accept((char)'x') or use convenience macros:
#define CHAR(x) (char)x
//#define EQ  CHAR('=')
// e.g. accept(CHAR('*'))  expect(EQ) 

#define UUDEFINE(x) bool scan##x(char *lp)
