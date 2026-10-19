/* util.c -- memory, strings and character helpers */
#include "plm.h"

void *xmalloc(size_t n)
{
    void *p = malloc(n ? n : 1);
    if (p == NULL) {
        fprintf(stderr, "plmc: out of memory\n");
        exit(3);
    }
    return p;
}

/* make room for need elements of size sz in an array */
void *xgrow(void *p, long *cap, long need, size_t sz)
{
    long nc;
    void *q;
    if (need <= *cap) return p;
    nc = *cap < 16 ? 16 : *cap * 2;
    while (nc < need) nc *= 2;
    q = p == NULL ? malloc(nc * sz) : realloc(p, nc * sz);
    if (q == NULL) {
        fprintf(stderr, "plmc: out of memory\n");
        exit(3);
    }
    *cap = nc;
    return q;
}

char *xstrdup(const char *s)
{
    char *p = (char *)xmalloc(strlen(s) + 1);
    strcpy(p, s);
    return p;
}

char *xcat(const char *a, const char *b, const char *c)
{
    char *p = (char *)xmalloc(strlen(a) + strlen(b) + strlen(c) + 1);
    strcpy(p, a);
    strcat(p, b);
    strcat(p, c);
    return p;
}

char *lowerc(const char *s)
{
    char *p = xstrdup(s);
    char *q;
    for (q = p; *q; q++)
        if (*q >= 'A' && *q <= 'Z') *q = (char)(*q - 'A' + 'a');
    return p;
}

char *upperc(const char *s)
{
    char *p = xstrdup(s);
    char *q;
    for (q = p; *q; q++)
        if (*q >= 'a' && *q <= 'z') *q = (char)(*q - 'a' + 'A');
    return p;
}

char *numstr(long v)
{
    char buf[40];
    sprintf(buf, "%ld", v);
    return xstrdup(buf);
}

/* ---- UTF-8: cREXX strings hold characters, not bytes ---- */
static int seqlen(const char *s)
{
    unsigned char c = (unsigned char)s[0];
    int n, i;
    if (c < 0xC2) return 1;
    n = c < 0xE0 ? 2 : (c < 0xF0 ? 3 : (c < 0xF5 ? 4 : 1));
    for (i = 1; i < n; i++)
        if (((unsigned char)s[i] & 0xC0) != 0x80) return 1;
    return n;
}

static long decode(const char *s, int n)
{
    unsigned char c = (unsigned char)s[0];
    long v;
    int i;
    if (n == 1) return c;
    v = n == 2 ? (c & 0x1F) : (n == 3 ? (c & 0x0F) : (c & 0x07));
    for (i = 1; i < n; i++) v = v * 64 + ((unsigned char)s[i] & 0x3F);
    return v;
}

/* number of characters in s */
int utf8len(const char *s)
{
    int n = 0;
    while (*s) {
        s += seqlen(s);
        n++;
    }
    return n;
}

/* code point of character number i (0 based) of s */
long utf8at(const char *s, int i)
{
    int n;
    while (i > 0 && *s) {
        s += seqlen(s);
        i--;
    }
    n = seqlen(s);
    return *s ? decode(s, n) : 0;
}

/* byte length of the character at s */
int utf8size(const char *s)
{
    return *s ? seqlen(s) : 0;
}

/* write a character code to the console, as UTF-8 */
void putcode(long c)
{
    if (c < 128) putchar((int)c);
    else if (c < 2048) {
        putchar((int)(0xC0 | (c >> 6)));
        putchar((int)(0x80 | (c & 0x3F)));
    } else if (c < 65536L) {
        putchar((int)(0xE0 | (c >> 12)));
        putchar((int)(0x80 | ((c >> 6) & 0x3F)));
        putchar((int)(0x80 | (c & 0x3F)));
    } else {
        putchar((int)(0xF0 | (c >> 18)));
        putchar((int)(0x80 | ((c >> 12) & 0x3F)));
        putchar((int)(0x80 | ((c >> 6) & 0x3F)));
        putchar((int)(0x80 | (c & 0x3F)));
    }
}

/* next character of n bytes at s (lenient: a bad byte stands for
   itself); len is the number of bytes used */
long u8get(const unsigned char *s, long left, int *len)
{
    unsigned char c = s[0];
    int n, i;
    long v;
    *len = 1;
    if (c < 0xC2) return c;
    n = c < 0xE0 ? 2 : (c < 0xF0 ? 3 : (c < 0xF5 ? 4 : 1));
    if (n == 1 || n > left) return c;
    for (i = 1; i < n; i++)
        if ((s[i] & 0xC0) != 0x80) return c;
    v = n == 2 ? (c & 0x1F) : (n == 3 ? (c & 0x0F) : (c & 0x07));
    for (i = 1; i < n; i++) v = v * 64 + (s[i] & 0x3F);
    *len = n;
    return v;
}
