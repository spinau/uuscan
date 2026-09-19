// uutest.c - unit tests exercising every user-facing macro/name in uuscan.h
// compile: cc -o uutest uutest.c && ./uutest
//
// Covers: UUTERMINALS/X(), UUDEFINE, UUVAL, accept(), expect(), acceptall(),
// fail(), success(), uuerror(), on_uuerror, CHAR(), EOL, uuerrorpos(),
// skipspace(), uu.* state fields, UUTERMCOUNT/uuterms[].

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

#define UUTERMINALS \
    X(ident) \
    X(number, int *)

// exercises the UUVAL global-return mechanism alongside direct-assignment
#define UUVAL struct { char *s; int i; }

#include "uuscan.h"

//{{{ minimal test harness
static int tests_run = 0, tests_failed = 0;

#define CHECK(cond, desc) do { \
        tests_run++; \
        if (cond) \
            printf("ok   - %s\n", desc); \
        else { \
            printf("FAIL - %s (%s:%d)\n", desc, __FILE__, __LINE__); \
            tests_failed++; \
        } \
    } while (0)
//}}}

//{{{ terminal scan functions
UUDEFINE(ident)
{
    if (!(isalpha(*lp) || *lp == '_'))
        return fail(lp);

    char *start = lp;
    do { ++lp; } while (isalnum(*lp) || *lp == '_');

    uu.len = (int)(lp - start);
    uu.s = start;                 // UUVAL mechanism
    if (res) *(char **)res = start; // direct-assignment mechanism

    return success(lp);
}

UUDEFINE(number, int *val)
{
    if (!isdigit(*lp))
        return fail(lp);

    char *start = lp;
    int n = 0, ndigits = 0;

    while (isdigit(*lp)) {
        n = n * 10 + (*lp - '0');
        ++lp;
        if (++ndigits > 6)
            uuerror("number too long");
    }

    if (*lp == '.')
        return fail(lp, "decimals not supported");

    uu.len = (int)(lp - start);
    uu.i = n;                     // UUVAL mechanism
    if (val) *val = n;            // direct-assignment mechanism

    return success(lp);
}
//}}}

//{{{ accept()/acceptall()/fail()/success() - no error path involved
static void test_ident_capture(void)
{
    char *val;

    uu.line = uu.lp = "  foo_bar2 rest";
    CHECK(accept(ident, &val) == true, "accept(ident,&val) matches an identifier");
    CHECK(val == uu.line + 2, "res set to start of match (after implicit skipspace)");
    CHECK(uu.len == 8, "uu.len records matched length");
    CHECK(uu.s == val, "UUVAL uu.s mirrors the direct-assignment result");

    char buf[16];
    memcpy(buf, val, uu.len);
    buf[uu.len] = '\0';
    CHECK(strcmp(buf, "foo_bar2") == 0, "captured text matches expected identifier");
}

static void test_ident_rejects_leading_digit(void)
{
    uu.line = uu.lp = "123abc";
    CHECK(accept(ident) == false, "ident does not match input starting with a digit");
    CHECK(uu.lp == uu.line, "uu.lp unchanged (fail() contract) after failed accept()");
}

static void test_fail_sets_lpfail_and_uuerrorpos(void)
{
    uu.line = uu.lp = "   123abc";
    CHECK(accept(ident) == false, "ident fails on digit-leading input after skipspace");
    CHECK(uu.lpfail == uu.line + 3, "fail() records the post-skipspace failure position");
    CHECK(uuerrorpos() == 4, "uuerrorpos() reports a 1-based column of the failure");
}

static void test_char_literal_basic(void)
{
    int v = 0;

    uu.line = uu.lp = "+5";
    CHECK(accept(CHAR('+')) == true, "CHAR('+') matches a literal plus sign");
    CHECK(uu.ch == '+', "uu.ch records the matched char");
    CHECK(accept(number, &v) == true && v == 5, "remainder parses as number 5");
}

static void test_whitespace_char_literal(void)
{
    uu.line = uu.lp = "  hi";

    // whitespace literals compare raw uu.lp directly (no implicit skipspace),
    // so each space must be matched/consumed one at a time
    CHECK(accept(CHAR(' ')) == true, "CHAR(' ') matches a raw leading space");
    CHECK(uu.ch == ' ', "uu.ch set correctly after whitespace-literal match");
    CHECK(uu.lp == uu.line + 1, "only one space consumed, not skipspace()'d wholesale");

    CHECK(accept(CHAR(' ')) == true, "second space also matches");
    CHECK(uu.lp == uu.line + 2, "two spaces consumed in total");

    CHECK(accept(ident) == true, "ident now matches 'hi'");
}

static void test_eol(void)
{
    uu.line = uu.lp = "end";
    CHECK(accept(ident) == true, "ident matches 'end'");
    CHECK(accept(EOL) == true, "EOL matches the terminating null");
    CHECK(uu.lp == uu.line + 3, "uu.lp stays put at the null terminator after EOL match");
}

static void test_string_and_charptr_literal_boundaries(void)
{
    char *cp = "int";

    uu.line = uu.lp = "integer x";
    CHECK(accept("int") == false, "\"int\" must not match as a prefix of \"integer\"");

    uu.line = uu.lp = "int x";
    CHECK(accept(cp) == true, "char* literal \"int\" matches when followed by non-alpha");
    CHECK(uu.len == 3, "uu.len records the matched literal length");

    uu.line = uu.lp = "421";
    CHECK(accept("42") == false, "\"42\" must not match as a prefix of \"421\"");

    uu.line = uu.lp = "42+3";
    CHECK(accept("42") == true, "\"42\" matches when followed by a non-digit");
}

static void test_empty_literal(void)
{
    char *matched = NULL;

    uu.line = uu.lp = "abc";
    CHECK(accept("", &matched) == true, "accept(\"\") trivially succeeds mid-string");
    CHECK(uu.lp == uu.line, "accept(\"\") does not consume any input");
    CHECK(matched == uu.line, "accept(\"\") reports the current position via res");

    uu.line = uu.lp = "";
    CHECK(accept("") == true, "accept(\"\") also succeeds at end of input");
}

static void test_accept_zero_args_skips_space(void)
{
    uu.line = uu.lp = "   x";
    accept();
    CHECK(uu.lp == uu.line + 3, "accept() with no arguments just skips leading whitespace");
}

static void test_bounds_check_regression(void)
{
    uu.line = uu.lp = "xyz";
    // 'Z' is a bare char constant -> promotes to int -> _Generic picks
    // _uuscan_term, using 90 as a terminal index. Must fail safely instead
    // of reading past the end of uuterms[].
    bool r = accept('Z');
    CHECK(r == false, "accept('Z') without CHAR() fails safely (no OOB uuterms[] read)");
    CHECK(uu.lp == uu.line, "uu.lp unchanged after the safe failure");
}

static void test_acceptall_success(void)
{
    int val = 0;

    uu.line = uu.lp = "x=5 rest";
    CHECK(acceptall(ident, CHAR('=')) == true, "acceptall() succeeds when every term matches");
    CHECK(accept(number, &val) == true && val == 5, "input continues to parse after acceptall()");
}

static void test_acceptall_backtracks_on_failure(void)
{
    uu.line = uu.lp = "x=abc";
    char *save = uu.lp;

    CHECK(acceptall(ident, CHAR('='), number) == false, "acceptall() fails when a later term fails");
    CHECK(uu.lp == save, "acceptall() restores uu.lp on failure (all-or-nothing)");
    CHECK(accept(ident) == true, "input is fully available again after the backtrack");
}

static void test_skipspace(void)
{
    char buf[] = "   \t\nhello";
    CHECK(strcmp(skipspace(buf), "hello") == 0, "skipspace() advances past leading whitespace");

    char buf2[] = "noleadingspace";
    CHECK(skipspace(buf2) == buf2, "skipspace() is a no-op with no leading whitespace");
}

static void test_term_table(void)
{
    CHECK(UUTERMCOUNT == 2, "UUTERMCOUNT matches the number of declared terminals");
    CHECK(strcmp(uuterms[ident].name, "ident") == 0, "uuterms[] records the name \"ident\"");
    CHECK(strcmp(uuterms[number].name, "number") == 0, "uuterms[] records the name \"number\"");
}
//}}}

//{{{ expect()/uuerror()/on_uuerror - error path, one on_uuerror per test
static void test_expect_success(void)
{
    char *val;
    int n = 0;

    on_uuerror {
        CHECK(false, "expect() unexpectedly raised an error");
        return;
    }

    uu.line = uu.lp = "count 42";
    expect(ident, &val);
    expect(number, &n);

    CHECK(strncmp(val, "count", 5) == 0, "expect(ident,&val) captured the identifier");
    CHECK(n == 42, "expect(number,&n) captured the number");
}

static void test_expect_default_message(void)
{
    on_uuerror {
        CHECK(strstr(uu.msg, "ident") != NULL, "default expect() message names the terminal");
        CHECK(uuerrorpos() == 1, "expect() failure message position is at the failure point");
        return;
    }

    uu.line = uu.lp = "123";
    expect(ident);
    CHECK(false, "unreachable: expect(ident) should have failed and jumped");
}

static void test_expect_custom_message(void)
{
    on_uuerror {
        CHECK(strcmp(uu.msg, "need a name here") == 0,
              "expect(t,&v,msg) overrides the default failure message");
        return;
    }

    char *val;
    uu.line = uu.lp = "123";
    expect(ident, &val, "need a name here");
    CHECK(false, "unreachable: expect(ident,&val,msg) should have failed and jumped");
}

static void test_expect_failmsg_appended(void)
{
    on_uuerror {
        CHECK(strstr(uu.msg, "decimals not supported") != NULL,
              "fail(cp,msg) failmsg is appended to expect()'s default message");
        return;
    }

    uu.line = uu.lp = "12.5";
    expect(number);
    CHECK(false, "unreachable: expect(number) should have failed and jumped");
}

static void test_uuerror_from_scan_function(void)
{
    on_uuerror {
        CHECK(strcmp(uu.msg, "number too long") == 0,
              "uuerror() called from inside a scan function sets uu.msg and jumps");
        return;
    }

    uu.line = uu.lp = "1234567"; // 7 digits, over the 6-digit limit in UUDEFINE(number)
    accept(number);
    CHECK(false, "unreachable: uuerror() should have jumped mid-scan");
}

static void test_uuerror_no_args(void)
{
    on_uuerror {
        CHECK(uu.msg[0] == '\0', "uuerror() with no arguments produces an empty message");
        return;
    }

    uuerror();
    CHECK(false, "unreachable");
}

static void test_uuerror_format_args(void)
{
    on_uuerror {
        CHECK(strcmp(uu.msg, "custom error 42") == 0, "uuerror(fmt,...) formats its message");
        return;
    }

    uuerror("custom error %d", 42);
    CHECK(false, "unreachable");
}

static bool callback_fired;
static void my_callback(void) { callback_fired = true; }

static void test_uuerror_callback(void)
{
    callback_fired = false;
    uu.callback = my_callback;

    on_uuerror {
        CHECK(callback_fired == true, "uu.callback is invoked before the uuerror() jump");
        CHECK(uu.callback == NULL, "uu.callback is reset to NULL once fired");
        return;
    }

    uuerror("boom");
    CHECK(false, "unreachable");
}

static void test_message_is_never_overrun(void)
{
    static char huge[2000];
    memset(huge, 'A', sizeof(huge) - 1);
    huge[sizeof(huge) - 1] = '\0';

    on_uuerror {
        CHECK(strlen(uu.msg) < UUMSGLEN, "oversized uuerror() message is truncated, not overrun");
        return;
    }

    uuerror("%s", huge);
    CHECK(false, "unreachable");
}
//}}}

int
main(void)
{
    test_ident_capture();
    test_ident_rejects_leading_digit();
    test_fail_sets_lpfail_and_uuerrorpos();
    test_char_literal_basic();
    test_whitespace_char_literal();
    test_eol();
    test_string_and_charptr_literal_boundaries();
    test_empty_literal();
    test_accept_zero_args_skips_space();
    test_bounds_check_regression();
    test_acceptall_success();
    test_acceptall_backtracks_on_failure();
    test_skipspace();
    test_term_table();

    test_expect_success();
    test_expect_default_message();
    test_expect_custom_message();
    test_expect_failmsg_appended();
    test_uuerror_from_scan_function();
    test_uuerror_no_args();
    test_uuerror_format_args();
    test_uuerror_callback();
    test_message_is_never_overrun();

    printf("\n%d/%d tests passed\n", tests_run - tests_failed, tests_run);
    return tests_failed != 0;
}
