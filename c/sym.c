/* sym.c -- symbol table, structures, procedures, memory layout */
#include "plm.h"

Sym *sym;                 /* symbols, 1 based */
long nsym = 0, nscope = 0;
static long symcap = 0, markcap = 0;
static long *scopemark;
long memtop = 256;        /* next free address */

Stype *stype;
Memb *memb;
long nstruct = 0, nmem = 0;
static long stcap = 0, mecap = 0;

Proc *proc;
long nproc = 0;
static long prcap = 0;
Par *par;
long npar = 0;
static long parcap = 0;

Init *inits;
long ninit = 0;
static long inicap = 0;

long newstruct(void)
{
    nstruct++;
    stype = (Stype *)xgrow(stype, &stcap, nstruct + 1, sizeof(Stype));
    return nstruct;
}

long newmember(void)
{
    nmem++;
    memb = (Memb *)xgrow(memb, &mecap, nmem + 1, sizeof(Memb));
    return nmem;
}

long newproc(void)
{
    nproc++;
    proc = (Proc *)xgrow(proc, &prcap, nproc + 1, sizeof(Proc));
    memset(&proc[nproc], 0, sizeof(Proc));
    return nproc;
}

long newpar(void)
{
    npar++;
    par = (Par *)xgrow(par, &parcap, npar + 1, sizeof(Par));
    return npar;
}

long addsym(const char *nm, char k, char t, long cnt, long adr)
{
    long lo = 1, i;
    Sym *s;
    if (nscope > 0) lo = scopemark[nscope] + 1;
    for (i = nsym; i >= lo; i--)
        if (!strcmp(sym[i].name, nm))
            error(xcat(xcat("'", lowerc(nm), "'"),
                       " is already declared", ""));
    nsym++;
    sym = (Sym *)xgrow(sym, &symcap, nsym + 1, sizeof(Sym));
    s = &sym[nsym];
    s->name = xstrdup(nm);
    s->kind = k;
    s->type = t;
    s->cnt = cnt;
    s->adr = adr;
    s->bp = -1;
    s->st = 0;
    s->aux = "";
    s->pid = 0;
    return nsym;
}

long lookup(const char *nm)
{
    long i;
    for (i = nsym; i >= 1; i--)
        if (!strcmp(sym[i].name, nm)) return i;
    return 0;
}

void pushscope(void)
{
    nscope++;
    scopemark = (long *)xgrow(scopemark, &markcap, nscope + 1,
                              sizeof(long));
    scopemark[nscope] = nsym;
    macroscope((int)nscope, 1);
}

void popscope(void)
{
    nsym = scopemark[nscope];
    macroscope((int)nscope, 0);
    nscope--;
}

static void addbuiltin(const char *nm, const char *aux)
{
    long k = addsym(nm, 'B', 'B', -1, 0);
    sym[k].aux = xstrdup(aux);
}

void initsyms(void)
{
    static const char *names[] = {
        "LENGTH", "LAST", "SIZE", "SHL", "SHR", "ROL", "ROR",
        "HIGH", "LOW", "DOUBLE", "INPUT", "OUTPUT", "CI", "CO",
        "PRINT", "PRNUM", "PRHEX", "CRLF", "MOVE", "TIME", "EXIT",
        NULL
    };
    static const char *unsup[] = {
        "CARRY", "ZERO", "SIGN", "PARITY", "SCL", "SCR", "DEC",
        "STACKPTR", NULL
    };
    int i;
    addsym("MEMORY", 'V', 'B', 65536L, 0);
    for (i = 0; names[i] != NULL; i++) addbuiltin(names[i], names[i]);
    for (i = 0; unsup[i] != NULL; i++)
        addbuiltin(unsup[i], "UNSUPPORTED");
}

/* allocate n bytes of memory */
long alloc(long n)
{
    long a = memtop;
    memtop += n;
    if (memtop > 65536L)
        error("out of memory: the program needs more than 64K");
    return a;
}

/* put the bytes b at addr when the program starts */
void initbytes(long adr, const unsigned char *b, long n)
{
    if (n == 0) return;
    ninit++;
    inits = (Init *)xgrow(inits, &inicap, ninit + 1, sizeof(Init));
    inits[ninit].adr = adr;
    inits[ninit].b = (unsigned char *)xmalloc((size_t)n);
    memcpy(inits[ninit].b, b, (size_t)n);
    inits[ninit].n = n;
}

long findmember(long stp, const char *nm)
{
    long i, last = stype[stp].m0 + stype[stp].nm - 1;
    for (i = stype[stp].m0; i <= last; i++)
        if (!strcmp(memb[i].name, nm)) return i;
    return 0;
}

long elsize(char t, long stp)
{
    if (t == 'S') return stype[stp].size;
    if (t == 'A') return 2;
    return 1;
}
