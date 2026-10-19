/* lex.c -- tokens, macros (LITERALLY) and error messages */
#include "plm.h"

Tok *tok;                 /* tokens, 1 based */
long np = 0;              /* number of tokens */
long ti = 1;              /* current token */
const char *srcfile = "";
static long tcap = 0;

static long mret[64];     /* return points of macro expansions */
long nmstk = 0;
static char **mn;         /* macro names */
static long *mfirst;      /* first token of the replacement text */
static long *mmark;       /* macro count at the start of each scope */
long nmac = 0;
static long mcap = 0, fcap = 0, scap = 0;

void errorat(const char *msg, long line)
{
    printf("%s(%ld): error: %s\n", srcfile, line, msg);
    fflush(stdout);
    exit(1);
}

void error(const char *msg)
{
    long line = curline;
    if (ti >= 1 && ti <= np) line = tok[ti].ln;
    errorat(msg, line);
}

static int isletter(int c)
{
    if (c >= 'a' && c <= 'z') return 1;
    if (c >= 'A' && c <= 'Z') return 1;
    return c == '_' || c == '?' || c == '@';
}

static int isdigit1(int c)
{
    return c >= '0' && c <= '9';
}

static void addtok(char k, char *v, long line)
{
    np++;
    tok = (Tok *)xgrow(tok, &tcap, np + 1, sizeof(Tok));
    tok[np].k = k;
    tok[np].v = v;
    tok[np].ln = line;
}

/* the value of a number such as 0FFH, 377Q, 1010B or 99 */
static long numval(const char *t, long line)
{
    char *u = upperc(t);
    long n = (long)strlen(u), bl = n, i, v = 0;
    int base = 10, d;
    char last = u[n - 1];
    const char *digs = "0123456789ABCDEF";
    const char *p;
    if (last == 'H') {
        base = 16;
        bl = n - 1;
    } else if (last == 'Q' || last == 'O') {
        base = 8;
        bl = n - 1;
    } else if (last == 'B' && strspn(u, "01") >= (size_t)(n - 1)) {
        base = 2;
        bl = n - 1;
    } else if (last == 'D') bl = n - 1;
    if (bl == 0) errorat(xcat("bad number '", t, "'"), line);
    for (i = 0; i < bl; i++) {
        p = strchr(digs, u[i]);
        d = p == NULL ? -1 : (int)(p - digs);
        if (d < 0 || d >= base)
            errorat(xcat("bad number '", t, "'"), line);
        v = v * base + d;
        if (v > 65535L)
            errorat(xcat(xcat("number '", t, "' is larger than "),
                         "65535", ""), line);
    }
    return v;
}

/* a growable text buffer */
typedef struct { char *s; long n, cap; } Buf;

static void bput(Buf *b, int c)
{
    b->s = (char *)xgrow(b->s, &b->cap, b->n + 2, 1);
    b->s[b->n++] = (char)c;
    b->s[b->n] = 0;
}

static char *bdone(Buf *b)
{
    if (b->s == NULL) return xstrdup("");
    return b->s;
}

static char *word(const char *src, long *pi)
{
    Buf b;
    long j = *pi;
    int d;
    b.s = NULL;
    b.n = b.cap = 0;
    for (;;) {
        d = (unsigned char)src[j];
        if (d == 0) break;
        if (isletter(d) || isdigit1(d)) bput(&b, d);
        else if (d != '$') break;
        j++;
    }
    *pi = j;
    return bdone(&b);
}

static char *sepsym(const char *src, long i)
{
    char two[3];
    two[0] = src[i];
    two[1] = src[i + 1];
    two[2] = 0;
    if (!strcmp(two, ":=") || !strcmp(two, "<>") ||
        !strcmp(two, "<=") || !strcmp(two, ">="))
        return xstrdup(two);
    if (src[i] && strchr("+-*/=<>().,;:", src[i])) {
        two[1] = 0;
        return xstrdup(two);
    }
    return NULL;
}

/* tokenize src and append the tokens; isend = 1 for the program,
   else the text of a macro */
void lexinto(const char *src, long ln, int isend)
{
    long i = 0, j;
    int bol = 1;
    int c, d;
    Buf b;
    char *s;
    char one[8];
    while (src[i]) {
        c = (unsigned char)src[i];
        if (c == '\n') {
            ln++;
            i++;
            bol = 1;
            continue;
        }
        if (c == ' ' || c == '\t' || c == '\r' || c == '\f') {
            i++;
            continue;
        }
        if (c == '$' && bol == 1 && isend == 1) {
            while (src[i] && src[i] != '\n') i++;
            continue;
        }
        bol = 0;
        if (c == '/' && src[i + 1] == '*') {
            i += 2;
            while (src[i] && !(src[i] == '*' && src[i + 1] == '/')) {
                if (src[i] == '\n') ln++;
                i++;
            }
            if (!src[i]) errorat("comment is not closed", ln);
            i += 2;
            continue;
        }
        if (isletter(c)) {
            s = upperc(word(src, &i));
            addtok('I', s, ln);
            continue;
        }
        if (isdigit1(c)) {
            s = word(src, &i);
            addtok('N', numstr(numval(s, ln)), ln);
            continue;
        }
        if (c == '\'') {
            b.s = NULL;
            b.n = b.cap = 0;
            i++;
            for (;;) {
                if (!src[i]) errorat("string is not closed", ln);
                d = (unsigned char)src[i];
                if (d == '\'') {
                    if (src[i + 1] == '\'') {
                        bput(&b, '\'');
                        i += 2;
                    } else {
                        i++;
                        break;
                    }
                } else {
                    if (d == '\n') ln++;
                    bput(&b, d);
                    i++;
                }
            }
            addtok('S', bdone(&b), ln);
            continue;
        }
        s = sepsym(src, i);
        if (s != NULL) {
            addtok('Y', s, ln);
            i += (long)strlen(s);
            continue;
        }
        j = utf8size(src + i);
        memcpy(one, src + i, (size_t)j);
        one[j] = 0;
        errorat(xcat("unexpected character '", one, "'"), ln);
    }
    if (isend == 1) addtok('E', xstrdup("<end of file>"), ln);
    else addtok('R', xstrdup(""), ln);
}

/* ------------------------------------------------------ macros */
static long findmacro(const char *nm)
{
    long i;
    for (i = nmac; i >= 1; i--)
        if (!strcmp(mn[i], nm)) return i;
    return 0;
}

/* leave the macro expansions and enter new ones until ti is an
   ordinary token */
void settle(void)
{
    long m;
    for (;;) {
        if (tok[ti].k == 'R') {
            ti = mret[nmstk];
            nmstk--;
            continue;
        }
        if (tok[ti].k == 'I' && nmac > 0) {
            m = findmacro(tok[ti].v);
            if (m > 0) {
                if (nmstk >= 50)
                    error(xcat("macro '", lowerc(tok[ti].v),
                               "' is nested too deeply"));
                nmstk++;
                mret[nmstk] = ti + 1;
                ti = mfirst[m];
                continue;
            }
        }
        break;
    }
}

void nexttok(void)
{
    if (tok[ti].k != 'E') ti++;
    settle();
}

/* is the token k places ahead of the given kind and text? */
int peekis(int k, char kind, const char *text)
{
    long sti = ti, snm = nmstk, i;
    long save[64];
    int j, r;
    for (i = 1; i <= nmstk; i++) save[i] = mret[i];
    for (j = 1; j <= k; j++) {
        if (tok[ti].k != 'E') ti++;
        settle();
    }
    r = tok[ti].k == kind && !strcmp(tok[ti].v, text);
    ti = sti;
    nmstk = snm;
    for (i = 1; i <= snm; i++) mret[i] = save[i];
    return r;
}

char curk(void)
{
    return tok[ti].k;
}

const char *cur(void)
{
    return tok[ti].v;
}

int isid(const char *w)
{
    return tok[ti].k == 'I' && !strcmp(tok[ti].v, w);
}

int issym(const char *w)
{
    return tok[ti].k == 'Y' && !strcmp(tok[ti].v, w);
}

int acceptid(const char *w)
{
    if (isid(w)) {
        nexttok();
        return 1;
    }
    return 0;
}

int acceptsym(const char *w)
{
    if (issym(w)) {
        nexttok();
        return 1;
    }
    return 0;
}

static void expected(const char *w)
{
    error(xcat(xcat("'", w, "' expected, found '"), cur(), "'"));
}

void expectid(const char *w)
{
    if (!isid(w)) expected(w);
    nexttok();
}

void expectsym(const char *w)
{
    if (!issym(w)) expected(w);
    nexttok();
}

char *getident(void)
{
    char *s;
    if (curk() != 'I')
        error(xcat("identifier expected, found '", cur(), "'"));
    s = tok[ti].v;
    nexttok();
    return s;
}

/* define the macro nm with the replacement text txt */
void defmacro(const char *nm, const char *txt)
{
    long first = np + 1;
    lexinto(txt, tok[ti].ln, 0);
    nmac++;
    mn = (char **)xgrow(mn, &mcap, nmac + 1, sizeof(char *));
    mfirst = (long *)xgrow(mfirst, &fcap, nmac + 1, sizeof(long));
    mn[nmac] = xstrdup(nm);
    mfirst[nmac] = first;
}

/* scope handling for the macros: remember or restore nmac */
void macroscope(int sc, int save)
{
    mmark = (long *)xgrow(mmark, &scap, sc + 1, sizeof(long));
    if (save) mmark[sc] = nmac;
    else nmac = mmark[sc];
}
