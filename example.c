// uuscan exercise - parse an arithmetic expression grammar
// compile: cc example.c 

// expr: 
//      term { "," term } eol
// term: 
//      factor 
//      | term "+" factor 
//      | term "-" factor
// factor: 
//      primary
//      | factor "*" expr
//      | factor "/" expr
// primary:
//      identifier
//      | identifier "(" expr-list ")"
//      | constant
//      | "-" primary | "+" primary
//      | "(" expr ")"
// expr-list:
//      | <empty>
//      | expr
//      | expr "," expr
// constant:
//      integer
//      

#include <stdio.h>
#include <stdlib.h>
#include <limits.h>

#define UUTERMINALS X(_ident_) X(_int_) X(_eol_)

// example uses uuval to return values:
#define UUVAL struct { int i; }

#include "uuscan.h"

#define EOL    CHAR('\0')
#define LPAREN CHAR('(')
#define RPAREN CHAR(')')
#define MINUS  CHAR('-')
#define PLUS   CHAR('+')
#define COMMA  CHAR(',')
#define MUL    CHAR('*')
#define DIV1   CHAR('/')
#define DIV2   "÷" // unicode example

// terminal scanners:

UUDEFINE(_ident_)
{
    // on entry to all terminal scanners:
    // uu.len == 0, uu.start == uu.lp, uu.faillp == NULL
    // and char *lp is local copy of uu.lp: ptr to next non-blank char in uu.line
    // uu.lp is only updated after successful scan

    if (isalpha(*lp) || *lp == '_') {
        do {
            ++uu.len;
            ++lp;
        } while (*lp && (isalnum(*lp) || *lp=='_'));
    } else 
        return fail(lp); // sets uu.lpfail with failure position in uu.line

    return success(lp); // uu.lp is updated with lp on return
}

UUDEFINE(_int_)
{
    if (isdigit(*lp)) { // + and - scanned separately
        int d, limit = INT_MAX % 10; // last digit of max long
        int max = INT_MAX / 10; // for overflow check without overflowing
        int val = 0;

        while (*lp && isdigit(*lp)) {
            d = *lp - '0';
            if (val > max || (val == max && d > limit))
                uuerror("integer overflow");
            val *= 10;
            val += d;
            ++lp;
        }

        uu.i = val;
        return success(lp);

    } else
        return fail(lp);
}

UUDEFINE(_eol_)
{
    return *lp == '\0';
}

// calculator:

// the base type for calculations; this exercise uses int
typedef int calc_t;

// define some built-in functions:

calc_t
fn_min(int ac, calc_t av[])
{
    calc_t min = av[0];
    for (int i = 1; i < ac; ++i)
        if (av[i] < min)
            min = av[i];
    return min;
}

calc_t
fn_max(int ac, calc_t av[])
{
    int max = av[0];
    for (int i = 1; i < ac; ++i)
        if (av[i] > max)
            max = av[i];
    return max;
}

calc_t
fn_rand(int ac, calc_t av[])
{
    if (ac != 0)
        puts("arguments in rand() ignored");
    return (calc_t) rand();
}

// table of built in functions

struct fn_list {
    char *name;
    calc_t (*exec)(int, calc_t *);
} builtin[] = {
    "min", fn_min,
    "max", fn_max,
    "rand", fn_rand,
};

#define NELEMS(x) (sizeof(x)/sizeof(x[0]))

int
lookup_fn(char *name)
{
    for (int i = 0; i < NELEMS(builtin); ++i)
        if (strcmp(builtin[i].name, name) == 0)
            return i;

    return -1;
}
    
#define MAXARGS 10 // max args for function calls
#define MAXIDENTLEN 20

calc_t primary(), factor(), term(), expr();

calc_t
primary()
{
    calc_t n, fn_args[MAXARGS];
    int fn, fn_argc = 0, i;
    char id[MAXIDENTLEN+1];

    if (accept(_ident_)) {
        strncpy(id, uu.lpstart, i = uu.len>MAXIDENTLEN? MAXIDENTLEN : uu.len);
        id[i] = '\0';

        if (accept(LPAREN)) { // a builtin function call

            if ((fn = lookup_fn(id)) == -1)
                uuerror("unknown function %s", id);

            while (1) {
                if (accept(RPAREN))
                    break;

                if (fn_argc < MAXARGS)
                    fn_args[fn_argc++] = term();
                else 
                    uuerror("function %s: too many args", id);

                if (accept(COMMA))
                    continue;

                if (accept(EOL))
                    uuerror("unclosed paren on function call %s", id);
            }

            return (builtin[fn].exec)(fn_argc, fn_args);

        } else {
            // typically, we would look up a symbol table
            // but for this exercise, just look in env:
            char *cp = getenv(id);
            if (cp)
                return (calc_t) atoi(cp);
            else
                uuerror("%s not found in environment", id);
        }
    }

    if (accept(LPAREN)) {
        n = term();
        expect(RPAREN);
        return n;
    }

    if (accept(MINUS))
        return -term();

    if (accept(PLUS))
        return term();

    if (accept(_int_))
        return uu.i;

    uuerror("syntax error at pos %d", uuerrorpos());
}

calc_t
factor()
{
    calc_t n = primary();

    while (1) {
        if (accept(MUL))
            n *= primary();
        else if (accept(DIV1) || accept(DIV2))
            n /= primary();
        else
            return n;
    }
}

calc_t
term()
{
    calc_t n = factor();

    while (1) {
        if (accept(PLUS))
            n += factor();
        else if (accept(MINUS))
            n -= factor();
        else
            return n;
    }
}

calc_t
expr()
{
    calc_t n = term();
    expect(_eol_);
    return n;
}

void
main()
{
    size_t linesz = 0;
    int len;

    uuterms[_eol_].name = "end of line";

    on_uuerror // uuerror() target
        puts(uu.msg);
        // drop through and keep reading input...

    while ((len = getline(&uu.line, &linesz, stdin)) > 0) {
        uu.line[len-1] = '\0'; // set up line to parse
        uu.lp = uu.line; // initialise line ptr

        printf(" = %d\n", expr());
    }
}
