/* vm.c -- the register machine and the run time library */
#include "plm.h"

#define MAXMEM 16777216L        /* limit for addresses above 64K */
#define MAXDEPTH 400000L        /* limit for nested calls */
#define MAXSTK 4000000L         /* limit for the REENTRANT save area */

static long *mem;               /* simulated memory, one cell a byte */
static long memsz = 65536L;
static long *stk;               /* save area, REENTRANT calls */
static long ssp = 0, stkcap = 0;
static long ports[256];

/* console input: the line being read */
static long *ibuf;
static long ilen = 0, ipos = 0, icap = 0;
static int ihave = 0, ieof = 0;

typedef struct {
    const Ins *pc, *cb;
    long fp, regs, dst;
} Frame;

static void finish(int code)
{
    fflush(stdout);
    exit(code);
}

static void runerror(const char *msg, int code)
{
    printf("runtime error: %s\n", msg);
    finish(code);
}

static void growmem(long a)
{
    long n;
    if (a < 0 || a >= MAXMEM)
        runerror("address out of range", 204);
    n = memsz;
    while (n <= a) n *= 2;
    mem = (long *)realloc(mem, (size_t)n * sizeof(long));
    if (mem == NULL) runerror("out of memory", 204);
    while (memsz < n) mem[memsz++] = 0;
}

#define LD(a) ((unsigned long)(a) < (unsigned long)memsz ? mem[a] : 0)
#define ST(a, v) do { \
    if ((unsigned long)(a) >= (unsigned long)memsz) growmem(a); \
    mem[a] = (v); } while (0)

/* ------------------------------------------------------ console */
static void pco(long c)
{
    if (c == 10) putchar('\n');
    else if (c != 13 && c != 7 && c != 0) putcode(c);
}

static void readline(void)
{
    unsigned char *b = NULL;
    long n = 0, cap = 0, i;
    int c, len;
    for (;;) {
        c = getchar();
        if (c == EOF) {
            if (n == 0) {
                ieof = 1;
                return;
            }
            break;
        }
        if (c == '\n' || c == '\r') break;
        b = (unsigned char *)xgrow(b, &cap, n + 1, 1);
        b[n++] = (unsigned char)c;
    }
    ilen = 0;
    ipos = 0;
    for (i = 0; i < n; i += len) {
        ibuf = (long *)xgrow(ibuf, &icap, ilen + 1, sizeof(long));
        ibuf[ilen++] = u8get(b + i, n - i, &len);
    }
    free(b);
    ihave = 1;
}

/* one character from the console; a line end is 10, the end of
   input is 26 */
static long pci(void)
{
    fflush(stdout);
    if (!ihave) readline();
    if (ieof) return 26;
    if (ipos >= ilen) {
        ihave = 0;
        return 10;
    }
    return ibuf[ipos++];
}

static void prhex(long n, long w)
{
    char buf[40], out[300];
    long len, i, k = 0;
    sprintf(buf, "%lX", n);
    len = (long)strlen(buf);
    if (w > 256) w = 256;
    for (i = len; i < w; i++) out[k++] = '0';
    for (i = len > w ? len - w : 0; i < len; i++) out[k++] = buf[i];
    out[k] = 0;
    fputs(out, stdout);
}

static void pmove(long n, long src, long dst)
{
    long i;
    for (i = 0; i < n; i++)
        mem[(dst + i) % 65536L] = mem[(src + i) % 65536L];
}

static void psave(long lo, long n)
{
    long i;
    stk = (long *)xgrow(stk, &stkcap, ssp + n + 1, sizeof(long));
    for (i = 0; i < n; i++) stk[ssp + i] = LD(lo + i);
    ssp += n;
    if (ssp > MAXSTK) runerror("stack overflow", 202);
}

static void prest(long lo, long n)
{
    long i;
    ssp -= n;
    for (i = 0; i < n; i++) ST(lo + i, stk[ssp + i]);
}

/* ------------------------------------------------------ the loop */
#define A (i->a)
#define B (i->b)
#define C (i->c)
#define MULV(x, y) ((long)((unsigned long)(x) * (unsigned long)(y)))
#define SHLV(x, n) (((n) & 63) > 31 ? 0L : \
    (long)((unsigned long)(x) << ((n) & 63)))
#define SHRV(x, n) (((n) & 63) > 31 ? 0L : (long)(x) >> ((n) & 63))

static void initmem(void)
{
    long k, j;
    mem = (long *)xmalloc(65536L * sizeof(long));
    for (k = 0; k < 65536L; k++) mem[k] = 0;
    for (k = 1; k <= ninit; k++)
        for (j = 0; j < inits[k].n; j++) {
            ST(inits[k].adr + j, (long)inits[k].b[j]);
        }
}

int vm_run(void)
{
    Frame *fs = NULL;
    long nf = 0, fcap = 0, rcap = 0, fp = 0, regs, a, b, x;
    long *rs = NULL, *R;
    const Ins *pc, *cb, *i;
    const Proc *pr;
    initmem();
    regs = mmax + 2;
    rs = (long *)xgrow(rs, &rcap, regs + 1, sizeof(long));
    for (a = 0; a < regs; a++) rs[a] = 0;
    R = rs;
    cb = maincode.v;
    pc = cb;
    for (;;) {
        i = pc++;
        switch (i->op) {
        case O_NOP: break;
        case O_LOADI: R[A] = B; break;
        case O_MOV: R[A] = R[B]; break;
        case O_ADD: R[A] = R[B] + R[C]; break;
        case O_SUB: R[A] = R[B] - R[C]; break;
        case O_MUL: R[A] = MULV(R[B], R[C]); break;
        case O_DIV:
            if (R[C] == 0) runerror("division by zero", 203);
            R[A] = R[B] / R[C];
            break;
        case O_MOD:
            if (R[C] == 0) runerror("division by zero", 203);
            R[A] = R[B] % R[C];
            break;
        case O_AND: R[A] = R[B] & R[C]; break;
        case O_OR: R[A] = R[B] | R[C]; break;
        case O_XOR: R[A] = R[B] ^ R[C]; break;
        case O_EQ: R[A] = R[B] == R[C]; break;
        case O_NE: R[A] = R[B] != R[C]; break;
        case O_LT: R[A] = R[B] < R[C]; break;
        case O_LE: R[A] = R[B] <= R[C]; break;
        case O_GT: R[A] = R[B] > R[C]; break;
        case O_GE: R[A] = R[B] >= R[C]; break;
        case O_ADDI: R[A] = R[B] + C; break;
        case O_SUBI: R[A] = R[B] - C; break;
        case O_MULI: R[A] = MULV(R[B], C); break;
        case O_DIVI: R[A] = R[B] / C; break;
        case O_MODI: R[A] = R[B] % C; break;
        case O_ANDI: R[A] = R[B] & C; break;
        case O_ORI: R[A] = R[B] | C; break;
        case O_XORI: R[A] = R[B] ^ C; break;
        case O_EQI: R[A] = R[B] == C; break;
        case O_NEI: R[A] = R[B] != C; break;
        case O_LTI: R[A] = R[B] < C; break;
        case O_LEI: R[A] = R[B] <= C; break;
        case O_GTI: R[A] = R[B] > C; break;
        case O_GEI: R[A] = R[B] >= C; break;
        case O_SHL: x = R[C]; R[A] = SHLV(R[B], x); break;
        case O_SHR: x = R[C]; R[A] = SHRV(R[B], x); break;
        case O_SHLI: R[A] = SHLV(R[B], C); break;
        case O_SHRI: R[A] = SHRV(R[B], C); break;
        case O_LD8: x = R[B] + C; R[A] = LD(x); break;
        case O_LD16:
            x = R[B] + C;
            R[A] = LD(x) | (LD(x + 1) << 8);
            break;
        case O_ST8: x = R[A] + B; ST(x, R[C]); break;
        case O_ST16:
            x = R[A] + B;
            b = R[C];
            ST(x, b & 255);
            ST(x + 1, b >> 8);
            break;
        case O_LD8A: R[A] = LD(B); break;
        case O_LD16A: R[A] = LD(B) | (LD(B + 1) << 8); break;
        case O_ST8A: ST(A, R[B]); break;
        case O_ST16A:
            b = R[B];
            ST(A, b & 255);
            ST(A + 1, b >> 8);
            break;
        case O_BR: pc = cb + A; break;
        case O_BRF: if (R[A] == 0) pc = cb + B; break;
        case O_BRT: if (R[A] != 0) pc = cb + B; break;
        case O_BEQ: if (R[A] == R[B]) pc = cb + C; break;
        case O_BNE: if (R[A] != R[B]) pc = cb + C; break;
        case O_BLT: if (R[A] < R[B]) pc = cb + C; break;
        case O_BLE: if (R[A] <= R[B]) pc = cb + C; break;
        case O_BGT: if (R[A] > R[B]) pc = cb + C; break;
        case O_BGE: if (R[A] >= R[B]) pc = cb + C; break;
        case O_BEQI: if (R[A] == B) pc = cb + C; break;
        case O_BNEI: if (R[A] != B) pc = cb + C; break;
        case O_BLTI: if (R[A] < B) pc = cb + C; break;
        case O_BLEI: if (R[A] <= B) pc = cb + C; break;
        case O_BGTI: if (R[A] > B) pc = cb + C; break;
        case O_BGEI: if (R[A] >= B) pc = cb + C; break;
        case O_CASE:
            x = R[A];
            if (x >= 0 && x < B) pc = cb + pc[x].a;
            else pc += B;
            break;
        case O_CALL:
            if (nf >= MAXDEPTH) runerror("stack overflow", 202);
            fs = (Frame *)xgrow(fs, &fcap, nf + 1, sizeof(Frame));
            fs[nf].pc = pc;
            fs[nf].cb = cb;
            fs[nf].fp = fp;
            fs[nf].regs = regs;
            fs[nf].dst = B;
            nf++;
            pr = &proc[A];
            fp += regs;
            regs = pr->nregs;
            rs = (long *)xgrow(rs, &rcap, fp + regs + 1, sizeof(long));
            R = rs + fp;
            for (a = 0; a < regs; a++) R[a] = 0;
            cb = pr->code.v;
            pc = cb;
            break;
        case O_RETR: x = R[A]; goto doret;
        case O_RET:
            x = 0;
        doret:
            if (nf == 0) finish(0);
            nf--;
            pc = fs[nf].pc;
            cb = fs[nf].cb;
            fp = fs[nf].fp;
            regs = fs[nf].regs;
            R = rs + fp;
            if (fs[nf].dst >= 0) R[fs[nf].dst] = x;
            break;
        case O_PCO: pco(R[A]); break;
        case O_PPRINT:
            x = R[A];
            for (;;) {
                b = LD(x);
                if (b == 36) break;
                pco(b);
                x++;
                if (x > 65535L) break;
            }
            break;
        case O_PPRNUM: printf("%ld", R[A]); break;
        case O_PRHEX: prhex(R[A], R[B]); break;
        case O_PCRLF: putchar('\n'); break;
        case O_PMOVE: pmove(R[A], R[B], R[C]); break;
        case O_PEXIT:
            finish((int)R[A]);
            break;
        case O_PCI: R[A] = pci(); break;
        case O_PINPUT:
            x = R[B];
            if (x == 1) R[A] = pci();
            else R[A] = (x >= 0 && x < 256) ? ports[x] : 0;
            break;
        case O_POUTPUT:
            x = R[A];
            if (x == 1) pco(R[B]);
            else if (x >= 0 && x < 256) ports[x] = R[B];
            break;
        case O_PSAVE: psave(A, B); break;
        case O_PREST: prest(A, B); break;
        case O_PHALT:
            finish(0);
            break;
        case O_PFLUSH: break;
        default:
            runerror("bad instruction", 205);
        }
    }
}
