### #include "uuscan.h"

uuscan.h provides a lightweight, single-header set of scan-as-you-parse helpers
for recognizing lexical elements in recursive descent parsers without the need
for a separate tokenizer pass.

It could be useful for small command-languages, DSL's, or other ad-hoc
parsing jobs where integrating lex and yacc or other tools is too expensive
and using straight strcmp's too tedious.

The two main macro functions provided are the self-documenting accept() and expect().
Errors are typically handled by a non-local goto for instant unwinding of deeply
nested parsing.

Any number of application-defined terminals can be used without lookup penalty.
Each terminal is processed in its own scanning function.

Functions for literal matching of strings and characters are provided in uuscan.h; 
terminal scanning functions must be provided by the application.

An example application is in example.c
Read the notes in uuscan.h for more information

### Usage example

```c
// define terminal names and create scanning function template (example):
#define UUTERMINALS X(term1) X(term2)

// if terminal scan functions return a converted value such values can be
// assigned to application-defined identifiers in the uu struct or union:
#define UUVAL { int i; char *s; }

#include "uuscan.h"

// scanning function for term1
// defines a function signature: bool _scan_term1(char *lp, void *res)
UUDEFINE(term1)
{
    // scanner code for term1

    // lp is a char *ptr to the current scan position (skipspace already applied).
    // *res is a ptr to store the scan result, or NULL if unused.

    // If the text at lp matches term1's rules, 'return success(lp)' advances
    // uu.lp past the match -- this is the normal match/no-match path used for
    // backtracking, NOT an error.

    // If the text doesn't match, 'return fail(lp)' leaves uu.lp unchanged and
    // records uu.lpfail; the caller (accept/expect) decides whether that's a
    // backtrack or a hard error. Use fail(lp, "msg") instead if the text is
    // recognized but malformed (e.g. a bad number literal) -- pass the detail
    // up without forcing an immediate uuerror().

    // For unrecoverable errors, call uuerror(...) to format a message and jump
    // to on_uuerror {...}.

    // Store converted values in uu.i, uu.s, etc. as your application requires
    // (see UUVAL above), or via the second ptr-to-variable argument to accept()/expect().
}

UUDEFINE(term2)
{
    // scanner code for term2
}

// ...

main()
{
    // declare uuerror() target:

    on_uuerror {
        // report error message, e.g.
        puts(uu.msg);
        // either exit with error or fall through to read next line
    }

    // read loop:

    while (uu.line = read_next_line()) {
        uu.lp = uu.line;

        //...

        if (accept(term1)) {

            expect(term2);

            // ...

        } else
            uuerror("term1 missing");

    }
}
```
