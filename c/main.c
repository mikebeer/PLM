/* main.c -- plmc: compile a PL/M-80 program and run it
   usage: plmc [-c] program.plm
   -c  compile only (check the program) */
#include "plm.h"

/* read the file; line ends (LF, CRLF, CR) become LF, and the last
   line gets one too */
static char *readsource(const char *fn)
{
    FILE *f = fopen(fn, "rb");
    char *buf = NULL;
    long n = 0, cap = 0;
    int c, prev = 0;
    if (f == NULL) return NULL;
    for (;;) {
        c = getc(f);
        if (c == EOF) break;
        if (c == 0) c = 1;
        if (c == '\n' && prev == '\r') {
            prev = c;
            continue;
        }
        prev = c;
        if (c == '\r') c = '\n';
        buf = (char *)xgrow(buf, &cap, n + 3, 1);
        buf[n++] = (char)c;
    }
    fclose(f);
    buf = (char *)xgrow(buf, &cap, n + 3, 1);
    if (n > 0 && buf[n - 1] != '\n') buf[n++] = '\n';
    buf[n] = 0;
    return buf;
}

int main(int argc, char **argv)
{
    int only = 0;
    char *src;
    if (argc > 1 && !strcmp(argv[1], "-c")) {
        only = 1;
        argc--;
        argv++;
    }
    if (argc < 2) {
        fprintf(stderr, "usage: plmc [-c] program.plm\n");
        return 2;
    }
    src = readsource(argv[1]);
    if (src == NULL) {
        fprintf(stderr, "usage: plmc [-c] program.plm\n");
        return 2;
    }
    srcfile = argv[1];
    lexinto(src, 1, 1);
    initsyms();
    compilemodule();
    if (only) return 0;
    return vm_run();
}
