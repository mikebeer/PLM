/* plm.h -- PL/M-80 compiler and virtual machine, C89 version.
   C port of plm.crexx and plmrt.crexx */
#ifndef PLM_H
#define PLM_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* ------------------------------------------------------ opcodes */
/* the binary operators come in two forms: register-register
   (O_ADD + k) and register-immediate (O_ADDI + k), k = 0..13 */
enum {
    O_NOP, O_LOADI, O_MOV,
    O_ADD, O_SUB, O_MUL, O_DIV, O_MOD, O_AND, O_OR, O_XOR,
    O_EQ, O_NE, O_LT, O_LE, O_GT, O_GE,
    O_ADDI, O_SUBI, O_MULI, O_DIVI, O_MODI, O_ANDI, O_ORI,
    O_XORI, O_EQI, O_NEI, O_LTI, O_LEI, O_GTI, O_GEI,
    O_SHL, O_SHR, O_SHLI, O_SHRI,
    O_LD8, O_LD16, O_ST8, O_ST16,
    O_LD8A, O_LD16A, O_ST8A, O_ST16A,
    O_BR, O_BRF, O_BRT,
    O_BEQ, O_BNE, O_BLT, O_BLE, O_BGT, O_BGE,
    O_BEQI, O_BNEI, O_BLTI, O_BLEI, O_BGTI, O_BGEI,
    O_CASE, O_CALL, O_RET, O_RETR,
    O_PCO, O_PPRINT, O_PPRNUM, O_PRHEX, O_PCRLF, O_PMOVE,
    O_PEXIT, O_PCI, O_PINPUT, O_POUTPUT, O_PSAVE, O_PREST,
    O_PHALT, O_PFLUSH
};
#define NBIN 14
#define K_ADD 0
#define K_SUB 1
#define K_MUL 2
#define K_DIV 3
#define K_MOD 4
#define K_AND 5
#define K_OR 6
#define K_XOR 7
#define K_EQ 8
#define K_NE 9
#define K_LT 10
#define K_LE 11
#define K_GT 12
#define K_GE 13

typedef struct { int op; long a, b, c; } Ins;
typedef struct { Ins *v; long n, cap; } Code;

/* ------------------------------------------------------ tables */
typedef struct { char k; char *v; long ln; } Tok;
typedef struct {
    char *name; char kind, type; long cnt, adr, bp;
    int st; char *aux; int pid;
} Sym;
typedef struct { long size; int m0, nm; } Stype;
typedef struct { char *name; char type; long cnt, off; } Memb;
typedef struct {
    char *name; char res; int np, p0, reent;
    long lo, hi; Code code; long nregs;
} Proc;
typedef struct { char *name; long adr; char type; } Par;
typedef struct { long adr; unsigned char *b; long n; } Init;

/* ------------------------------------------------------ util.c */
void *xmalloc(size_t n);
void *xgrow(void *p, long *cap, long need, size_t sz);
char *xstrdup(const char *s);
char *xcat(const char *a, const char *b, const char *c);
char *lowerc(const char *s);
char *upperc(const char *s);
char *numstr(long v);
int utf8len(const char *s);
long utf8at(const char *s, int i);
int utf8size(const char *s);
void putcode(long c);
long u8get(const unsigned char *s, long left, int *len);

/* ------------------------------------------------------ lex.c */
extern Tok *tok;
extern long np, ti, nmstk, nmac;
extern const char *srcfile;
void error(const char *msg);
void errorat(const char *msg, long line);
void lexinto(const char *src, long ln, int isend);
void settle(void);
void nexttok(void);
int peekis(int k, char kind, const char *text);
char curk(void);
const char *cur(void);
int isid(const char *w);
int issym(const char *w);
int acceptid(const char *w);
int acceptsym(const char *w);
void expectid(const char *w);
void expectsym(const char *w);
char *getident(void);
void defmacro(const char *nm, const char *txt);
void macroscope(int sc, int save);
long macromark(int sc);

/* ------------------------------------------------------ sym.c */
extern Sym *sym;
extern long nsym, nscope, memtop;
extern Stype *stype;
extern Memb *memb;
extern long nstruct, nmem;
extern Proc *proc;
extern long nproc;
extern Par *par;
extern long npar;
extern Init *inits;
extern long ninit;
void initsyms(void);
long addsym(const char *nm, char k, char t, long cnt, long adr);
long lookup(const char *nm);
void pushscope(void);
void popscope(void);
long alloc(long n);
void initbytes(long adr, const unsigned char *b, long n);
long findmember(long stp, const char *nm);
long elsize(char t, long stp);
long newproc(void);
long newstruct(void);
long newmember(void);
long newpar(void);

/* ------------------------------------------------------ emit.c */
extern Code maincode;
extern Code *cc;
extern long nr, nrbase, maxr, mmax;
long emit(int op, long a, long b, long c);
long curn(void);
int newlab(void);
void deflab(int lab);
long nreg(void);
long holdreg(void);
void finishcode(Code *c);
void branchop(int op, long reg, int lab);
long loadc(long v);
void ebin(int k, long d, long x, long y);
void ebini(int k, long d, long x, long y);

/* ------------------------------------------------------ expr.c */
extern char rk, rt;
extern long rv, rr;
extern int rbool;
extern long drb, dro;
extern char dty;
extern long dvar;
void setconst(long v);
void toreg(void);
void normbool(void);
void coerce(char t);
long mask(long v, char t);
void expr(void);
long constexpr(void);
long condreg(void);
void designator(int ctx);
void symdesig(long x);
void storeval(char t, long b, long o);
long rdloc(char t, long b, long o);
void wrloc(char t, long b, long o, long v);
void callproc(long x);
long rtcall(int op, long a1, long a2, long a3);
unsigned char *constlist(char et, long *nbytes);
void savepatch(long pid);
void varinfo(long *pcnt, long *pesz);
extern long clitems;

/* ------------------------------------------------------ stmt.c */
void blockbody(void);
void statement(void);
void declstmt(void);
void procdecl(void);
void compilemodule(void);
extern int curproc;
extern long curline;
extern long nlb, lbbase;
void checklabels(void);
void endblock(void);

/* ------------------------------------------------------ vm.c */
int vm_run(void);

#endif
