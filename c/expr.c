/* expr.c -- expressions: constant folding and code generation */
#include "plm.h"

char rk = 'R';            /* result kind: C constant, R register */
long rv = 0;              /* constant value */
long rr = 0;              /* register */
char rt = 'B';            /* type: B or A */
int rbool = 0;            /* 1 = register holds 0 or 1 (comparison) */
long drb = -1;            /* designator: base register, -1 for none */
long dro = 0;             /* designator: offset */
char dty = 'B';           /* designator: type of the element */
long dvar = 0;            /* designator: symbol */
long clitems = 0;         /* number of elements in the last list */
static long xb = -1, xo = 0;  /* result of applyindex */
static long *patchv;      /* PSAVE/PREST that need the frame size */
static long npatch = 0, patchcap = 0;

static char *lc(const char *s)
{
    return lowerc(s);
}

static void err3(const char *a, const char *b, const char *c)
{
    error(xcat(a, b, c));
}

/* ------------------------------------------------------ results */
long mask(long v, char t)
{
    long m = t == 'B' ? 256L : 65536L;
    v = v % m;
    if (v < 0) v += m;
    return v;
}

static char tyof(long v)
{
    return v > 255 ? 'A' : 'B';
}

void setconst(long v)
{
    rk = 'C';
    rv = v;
    rt = tyof(v);
    rbool = 0;
}

void toreg(void)
{
    if (rk == 'C') {
        rr = loadc(rv);
        rk = 'R';
    }
}

/* turn a comparison result 0/1 into the PL/M truth value 0/255 */
void normbool(void)
{
    long d;
    if (rbool == 1) {
        d = nreg();
        ebini(K_MUL, d, rr, 255);
        rr = d;
        rbool = 0;
        rt = 'B';
    }
}

void coerce(char t)
{
    long d;
    normbool();
    if (rk == 'C') {
        rv = mask(rv, t);
        if (t == 'B') rt = 'B';
        else rt = tyof(rv);
        return;
    }
    if (t == 'B' && rt == 'A') {
        d = nreg();
        ebini(K_AND, d, rr, 255);
        rr = d;
    }
    rt = t;
}

/* ------------------------------------------------------ memory */
long rdloc(char t, long b, long o)
{
    long d = nreg();
    if (t == 'A') {
        if (b == -1) emit(O_LD16A, d, o, 0);
        else emit(O_LD16, d, b, o);
        nreg();
    } else {
        if (b == -1) emit(O_LD8A, d, o, 0);
        else emit(O_LD8, d, b, o);
    }
    return d;
}

void wrloc(char t, long b, long o, long v)
{
    int op;
    if (t == 'B') {
        if (b == -1) emit(O_ST8A, o, v, 0);
        else emit(O_ST8, b, o, v);
        return;
    }
    nreg();
    nreg();
    op = b == -1 ? O_ST16A : O_ST16;
    if (b == -1) emit(op, o, v, 0);
    else emit(op, b, o, v);
}

/* store the current result into the location (b, o) of type t */
void storeval(char t, long b, long o)
{
    long r;
    if (rk == 'C') {
        r = loadc(mask(rv, t));
        wrloc(t, b, o, r);
        return;
    }
    normbool();
    r = rr;
    if (t == 'B' && rt == 'A') {
        r = nreg();
        ebini(K_AND, r, rr, 255);
    }
    wrloc(t, b, o, r);
}

/* ------------------------------------------------------ constants */
static void putel(unsigned char **p, long *n, long *cap, long v)
{
    *p = (unsigned char *)xgrow(*p, cap, *n + 1, 1);
    (*p)[(*n)++] = (unsigned char)(v & 255);
}

static void elem(unsigned char **p, long *n, long *cap, long v,
                 char et)
{
    if (et == 'A' || (et != 'B' && v > 255)) {
        putel(p, n, cap, v % 256);
        putel(p, n, cap, (v / 256) & 255);
    } else putel(p, n, cap, v % 256);
}

/* ( item, item, ... ): numbers and strings, as bytes */
unsigned char *constlist(char et, long *nbytes)
{
    unsigned char *hx = NULL;
    long n = 0, cap = 0, i, len;
    int done, c1, c2;
    const char *s;
    expectsym("(");
    clitems = 0;
    for (;;) {
        done = 0;
        if (curk() == 'S') {
            c1 = peekis(1, 'Y', ",");
            c2 = peekis(1, 'Y', ")");
            if (c1 || c2) {
                s = cur();
                nexttok();
                len = utf8len(s);
                for (i = 0; i < len; i++) {
                    elem(&hx, &n, &cap, utf8at(s, (int)i), et);
                    clitems++;
                }
                done = 1;
            }
        }
        if (!done) {
            expr();
            if (rk != 'C') error("constant expected in the list");
            elem(&hx, &n, &cap, rv, et);
            clitems++;
        }
        if (!acceptsym(",")) break;
    }
    expectsym(")");
    *nbytes = n;
    return hx;
}

/* ------------------------------------------------- designators */
/* add the index in the current result to the address (b, o) with
   element size esz */
static void applyindex(long b, long o, long esz)
{
    long idx, t, nb;
    if (rk == 'C') {
        xb = b;
        xo = o + rv * esz;
        return;
    }
    normbool();
    idx = rr;
    if (esz > 1) {
        t = nreg();
        ebini(K_MUL, t, idx, esz);
        idx = t;
    }
    if (b == -1) xb = idx;
    else {
        nb = nreg();
        ebin(K_ADD, nb, b, idx);
        xb = nb;
    }
    xo = o;
}

static void notmember(const char *mnm, const char *nm)
{
    error(xcat(xcat("'", lc(mnm), "' is not a member of '"),
               lc(nm), "'"));
}

static long getvar(const char *nm)
{
    long x = lookup(nm);
    if (x == 0) err3("'", lc(nm), "' is not declared");
    if (sym[x].kind != 'V') err3("'", lc(nm), "' is not a variable");
    return x;
}

static void subscript(long *b, long *o, long esz)
{
    nexttok();
    expr();
    expectsym(")");
    applyindex(*b, *o, esz);
    *b = xb;
    *o = xo;
}

/* variable at the current token: leaves the location in drb, dro,
   dty.  ctx = 1: address context, a missing subscript or member
   means the start */
void designator(int ctx)
{
    const char *nm = cur();
    long x = getvar(nm), b = -1, o, stp, m;
    char t;
    char *mnm;
    nexttok();
    o = sym[x].adr;
    if (sym[x].bp >= 0) b = rdloc('A', -1, sym[x].bp);
    t = sym[x].type;
    stp = sym[x].st;
    if (sym[x].cnt >= 0) {
        if (issym("(")) subscript(&b, &o, elsize(t, stp));
        else if (ctx == 0)
            err3("'", lc(nm), "' is an array and needs a subscript");
    }
    if (t == 'S') {
        if (issym(".")) {
            nexttok();
            mnm = getident();
            m = findmember(stp, mnm);
            if (m == 0)
                notmember(mnm, nm);
            o += memb[m].off;
            t = memb[m].type;
            if (memb[m].cnt >= 0) {
                if (issym("(")) subscript(&b, &o, elsize(t, 0));
                else if (ctx == 0)
                    err3("member '", lc(mnm),
                         "' is an array and needs a subscript");
            }
        } else if (ctx == 0)
            err3("'", lc(nm), "' is a structure and needs a member");
        else t = 'B';
    }
    drb = b;
    dro = o;
    dty = t;
    dvar = x;
}

/* the location of a scalar variable, from its symbol alone */
void symdesig(long x)
{
    long b = -1;
    if (sym[x].bp >= 0) b = rdloc('A', -1, sym[x].bp);
    drb = b;
    dro = sym[x].adr;
    dty = sym[x].type;
    dvar = x;
}

/* element count and size, for LENGTH, LAST and SIZE */
void varinfo(long *pcnt, long *pesz)
{
    const char *nm;
    long x, m, vcnt, vesz;
    char *mnm;
    expectsym("(");
    nm = cur();
    x = getvar(nm);
    nexttok();
    vcnt = 1;
    if (sym[x].cnt >= 0) vcnt = sym[x].cnt;
    vesz = elsize(sym[x].type, sym[x].st);
    if (sym[x].type == 'S' && issym(".")) {
        nexttok();
        mnm = getident();
        m = findmember(sym[x].st, mnm);
        if (m == 0) notmember(mnm, nm);
        vcnt = 1;
        if (memb[m].cnt >= 0) vcnt = memb[m].cnt;
        vesz = elsize(memb[m].type, 0);
    }
    expectsym(")");
    *pcnt = vcnt;
    *pesz = vesz;
}

/* ------------------------------------------------------ calls */
long rtcall(int op, long a1, long a2, long a3)
{
    emit(op, a1, a2, a3);
    return 0;
}

void savepatch(long pid)
{
    long i;
    for (i = 0; i < npatch; i++)
        cc->v[patchv[i]].b = proc[pid].hi - proc[pid].lo;
    npatch = 0;
}

/* PSAVE or PREST for the cells of a REENTRANT procedure */
static void savecode(long pid, int op)
{
    long i;
    if (curproc == pid) {
        i = emit(op, proc[pid].lo, 0, 0);
        patchv = (long *)xgrow(patchv, &patchcap, npatch + 1,
                               sizeof(long));
        patchv[npatch++] = i;
    } else
        emit(op, proc[pid].lo, proc[pid].hi - proc[pid].lo, 0);
}

/* the procedure x has been read; the arguments follow */
void callproc(long x)
{
    long pid = sym[x].pid, n = proc[pid].np, i, p, d;
    long *areg = (long *)xmalloc((size_t)(n + 1) * sizeof(long));
    const char *pnm = lc(proc[pid].name);
    if (n > 0) {
        expectsym("(");
        for (i = 1; i <= n; i++) {
            if (i > 1) {
                if (!issym(","))
                    err3("too few arguments in the call of '", pnm,
                         "'");
                nexttok();
            }
            expr();
            p = proc[pid].p0 + i - 1;
            coerce(par[p].type);
            toreg();
            areg[i] = rr;
        }
        if (issym(","))
            err3("too many arguments in the call of '", pnm, "'");
        expectsym(")");
    } else if (issym("(")) {
        nexttok();
        expectsym(")");
    }
    if (proc[pid].reent) savecode(pid, O_PSAVE);
    for (i = 1; i <= n; i++) {
        p = proc[pid].p0 + i - 1;
        wrloc(par[p].type, -1, par[p].adr, areg[i]);
    }
    if (proc[pid].res == 0) {
        emit(O_CALL, pid, -1, 0);
        rk = 'R';
        rr = 0;
        rt = 'B';
    } else {
        d = nreg();
        emit(O_CALL, pid, d, 0);
        rk = 'R';
        rr = d;
        rt = proc[pid].res;
    }
    rbool = 0;
    if (proc[pid].reent) savecode(pid, O_PREST);
    free(areg);
}
