/* expr2.c -- primaries and operators */
#include "plm.h"

static char *lc(const char *s)
{
    return lowerc(s);
}

static void err3(const char *a, const char *b, const char *c)
{
    error(xcat(a, b, c));
}

/* 2 to the power n, with a ceiling */
static unsigned long pow2(long n)
{
    if (n >= 17) return 131072UL;
    return 1UL << n;
}

/* the operations AND, OR, XOR on 16 bit values */
static long bitop(int k, long a, long b)
{
    a &= 65535L;
    b &= 65535L;
    if (k == K_AND) return a & b;
    if (k == K_OR) return a | b;
    return a ^ b;
}

/* ------------------------------------------------------ primary */
static void primary(void);

/* the address of a variable, or of a list of constants */
static void addrof(void)
{
    unsigned char *hx;
    long n, a, i;
    const char *s;
    if (issym("(")) {
        hx = constlist('?', &n);
        a = alloc(n);
        initbytes(a, hx, n);
        setconst(a);
        rt = 'A';
        return;
    }
    if (curk() == 'S') {
        s = cur();
        nexttok();
        n = utf8len(s);
        hx = (unsigned char *)xmalloc((size_t)n + 1);
        for (i = 0; i < n; i++)
            hx[i] = (unsigned char)(utf8at(s, (int)i) & 255);
        a = alloc(n);
        initbytes(a, hx, n);
        setconst(a);
        rt = 'A';
        return;
    }
    if (curk() != 'I') error("variable expected after '.'");
    designator(1);
    if (drb == -1) setconst(dro);
    else {
        if (dro == 0) rr = drb;
        else {
            rr = nreg();
            ebini(K_ADD, rr, drb, dro);
        }
        rk = 'R';
    }
    rt = 'A';
    rbool = 0;
}

/* SHL SHR ROL ROR of the left operand (kept in lk lv lr lt) by the
   current result; op is 0 SHL, 1 SHR, 2 ROL, 3 ROR */
static void shiftop(int op, char lk, long lv, long lr, char lt)
{
    long w = lt == 'B' ? 8 : 16;
    long m = lt == 'B' ? 255 : 65535L;
    long n, v, d, n1, n2, a, c, e, t;
    if (lk == 'C' && rk == 'C') {
        n = rv;
        if (op == 0)
            v = (long)(((unsigned long)lv * pow2(n)) % (m + 1));
        else if (op == 1) v = (long)((unsigned long)lv / pow2(n));
        else {
            n = n % w;
            if (op == 3) n = (w - n) % w;
            if (n == 0) v = lv;
            else
                v = (long)(((unsigned long)lv * pow2(n)) % (m + 1)
                           + (unsigned long)lv / pow2(w - n));
        }
        setconst(v);
        rt = lt;
        return;
    }
    if (lk == 'C') lr = loadc(lv);
    d = nreg();
    if (op < 2) {
        if (rk == 'C') emit(op == 0 ? O_SHLI : O_SHRI, d, lr, rv);
        else emit(op == 0 ? O_SHL : O_SHR, d, lr, rr);
        if (op == 0) ebini(K_AND, d, d, m);
        rr = d;
        rk = 'R';
        rt = lt;
        return;
    }
    /* rotate: ((x << n) | (x >> (w - n))) & m, n taken modulo w */
    toreg();
    n1 = nreg();
    ebini(K_MOD, n1, rr, w);
    if (op == 3) {
        n2 = nreg();
        t = loadc(w);
        ebin(K_SUB, n2, t, n1);
        ebini(K_MOD, n1, n2, w);
    }
    a = nreg();
    emit(O_SHL, a, lr, n1);
    c = nreg();
    t = loadc(w);
    ebin(K_SUB, c, t, n1);
    e = nreg();
    emit(O_SHR, e, lr, c);
    ebin(K_OR, d, a, e);
    ebini(K_AND, d, d, m);
    rr = d;
    rk = 'R';
    rt = lt;
}

static void builtinfn(const char *aux, const char *nm)
{
    long d, lv, lr;
    char lk, lt;
    if (!strcmp(aux, "LENGTH") || !strcmp(aux, "LAST") ||
        !strcmp(aux, "SIZE")) {
        long cnt, esz;
        varinfo(&cnt, &esz);
        if (!strcmp(aux, "LENGTH")) setconst(cnt);
        else if (!strcmp(aux, "LAST")) setconst(cnt - 1);
        else setconst(cnt * esz);
        return;
    }
    if (!strcmp(aux, "SHL") || !strcmp(aux, "SHR") ||
        !strcmp(aux, "ROL") || !strcmp(aux, "ROR")) {
        expectsym("(");
        expr();
        normbool();
        lk = rk;
        lv = rv;
        lr = rr;
        lt = rt;
        expectsym(",");
        expr();
        normbool();
        expectsym(")");
        shiftop(aux[1] == 'H' ? (aux[2] == 'L' ? 0 : 1)
                : (aux[2] == 'L' ? 2 : 3), lk, lv, lr, lt);
        return;
    }
    if (!strcmp(aux, "HIGH") || !strcmp(aux, "LOW") ||
        !strcmp(aux, "DOUBLE")) {
        expectsym("(");
        expr();
        expectsym(")");
        normbool();
        if (!strcmp(aux, "DOUBLE")) {
            rt = 'A';
            return;
        }
        if (rk == 'C') {
            if (!strcmp(aux, "HIGH")) rv = rv / 256;
            else rv = rv % 256;
            rt = 'B';
            return;
        }
        d = nreg();
        if (!strcmp(aux, "HIGH")) emit(O_SHRI, d, rr, 8);
        else ebini(K_AND, d, rr, 255);
        rr = d;
        rt = 'B';
        return;
    }
    if (!strcmp(aux, "CI")) {
        if (issym("(")) {
            nexttok();
            expectsym(")");
        }
        rr = nreg();
        emit(O_PCI, rr, 0, 0);
        rk = 'R';
        rt = 'B';
        rbool = 0;
        return;
    }
    if (!strcmp(aux, "INPUT")) {
        expectsym("(");
        expr();
        coerce('B');
        toreg();
        expectsym(")");
        d = nreg();
        emit(O_PINPUT, d, rr, 0);
        rr = d;
        rk = 'R';
        rt = 'B';
        rbool = 0;
        return;
    }
    if (!strcmp(aux, "UNSUPPORTED"))
        err3("'", lc(nm), "' is not supported by this compiler");
    err3("'", lc(nm), "' is a procedure and has no value");
}

static void primary(void)
{
    char k = curk(), t;
    const char *nm, *s;
    long x, b, o;
    int n;
    rbool = 0;
    if (k == 'N') {
        setconst(atol(cur()));
        nexttok();
        return;
    }
    if (k == 'S') {
        s = cur();
        n = utf8len(s);
        if (n == 1) setconst(utf8at(s, 0));
        else if (n == 2) setconst(utf8at(s, 0) * 256 + utf8at(s, 1));
        else error("a string of more than two characters can only "
                   "appear in a list or after a '.'");
        nexttok();
        return;
    }
    if (issym("(")) {
        nexttok();
        expr();
        expectsym(")");
        return;
    }
    if (issym(".")) {
        nexttok();
        addrof();
        return;
    }
    if (k == 'I') {
        nm = cur();
        x = lookup(nm);
        if (x == 0) err3("'", lc(nm), "' is not declared");
        if (sym[x].kind == 'V') {
            designator(0);
            b = drb;
            o = dro;
            t = dty;
            if (issym(":=")) {
                nexttok();
                expr();
                coerce(t);
                toreg();
                storeval(t, b, o);
                rt = t;
                return;
            }
            rr = rdloc(t, b, o);
            rk = 'R';
            rt = t;
            rbool = 0;
            return;
        }
        if (sym[x].kind == 'P') {
            nexttok();
            callproc(x);
            if (proc[sym[x].pid].res == 0)
                err3("procedure '", lc(nm), "' has no value");
            return;
        }
        if (sym[x].kind == 'B') {
            nexttok();
            builtinfn(sym[x].aux, nm);
            return;
        }
    }
    err3("expression expected, found '", cur(), "'");
}

/* ------------------------------------------------------ operators */
/* combine the left operand (arguments) with the current result,
   which is the right operand */
static void combine(int k, char lk, long lv, long lr, char lt, int lb)
{
    char rkk = rk, rtt = rt, restype;
    long rvv = rv, rrr = rr, d, a, b, v, m, lz;
    int rbb = rbool, isbit = k >= K_AND && k <= K_XOR;
    if (isbit && lb == 1 && rbb == 1) {
        d = nreg();
        ebin(k, d, lr, rrr);
        rk = 'R';
        rr = d;
        rt = 'B';
        rbool = 1;
        return;
    }
    if (lb == 1) {
        d = nreg();
        ebini(K_MUL, d, lr, 255);
        lr = d;
        lt = 'B';
    }
    if (rbb == 1) {
        d = nreg();
        ebini(K_MUL, d, rrr, 255);
        rrr = d;
        rtt = 'B';
    }
    rbool = 0;
    restype = (lt == 'A' || rtt == 'A') ? 'A' : 'B';
    if (lk == 'C' && rkk == 'C') {
        a = lv;
        b = rvv;
        switch (k) {
        case K_ADD: v = a + b; break;
        case K_SUB: v = a - b; break;
        case K_MUL:
            v = (long)((unsigned long)a * (unsigned long)b);
            break;
        case K_DIV:
            if (b == 0)
                error("division by zero in a constant expression");
            v = a / b;
            break;
        case K_MOD:
            if (b == 0)
                error("division by zero in a constant expression");
            v = a % b;
            break;
        case K_AND: case K_OR: case K_XOR:
            v = bitop(k, a, b);
            break;
        default:
            v = 0;
            if (k == K_EQ && a == b) v = 255;
            if (k == K_NE && a != b) v = 255;
            if (k == K_LT && a < b) v = 255;
            if (k == K_LE && a <= b) v = 255;
            if (k == K_GT && a > b) v = 255;
            if (k == K_GE && a >= b) v = 255;
            break;
        }
        setconst(mask(v, 'A'));
        return;
    }
    if ((k == K_DIV || k == K_MOD) && rkk == 'C' && rvv == 0)
        error("division by zero");
    d = nreg();
    if (rkk == 'C') ebini(k, d, lr, rvv);
    else if (lk == 'C') {
        if (k == K_ADD || k == K_MUL || k == K_AND || k == K_OR ||
            k == K_XOR || k == K_EQ || k == K_NE)
            ebini(k, d, rrr, lv);
        else {
            lz = loadc(lv);
            ebin(k, d, lz, rrr);
        }
    } else ebin(k, d, lr, rrr);
    rk = 'R';
    rr = d;
    if (k >= K_EQ) {
        rt = 'B';
        rbool = 1;
        return;
    }
    rt = restype;
    if (k == K_ADD || k == K_SUB || k == K_MUL) {
        m = restype == 'A' ? 65535L : 255;
        ebini(K_AND, d, d, m);
    }
}

static void unary(void)
{
    long d, m;
    if (issym("-")) {
        nexttok();
        primary();
        normbool();
        if (rk == 'C') {
            setconst(mask(0 - rv, 'A'));
            return;
        }
        d = nreg();
        ebini(K_MUL, d, rr, -1);
        m = rt == 'A' ? 65535L : 255;
        ebini(K_AND, d, d, m);
        rr = d;
        return;
    }
    if (issym("+")) {
        nexttok();
        primary();
        normbool();
        return;
    }
    primary();
}

#define SAVELEFT lk = rk; lv = rv; lr = rr; lt = rt; lb = rbool

static void mulexpr(void)
{
    int k;
    char lk, lt;
    long lv, lr;
    int lb;
    unary();
    for (;;) {
        if (issym("*")) k = K_MUL;
        else if (issym("/")) k = K_DIV;
        else if (isid("MOD")) k = K_MOD;
        else break;
        nexttok();
        SAVELEFT;
        unary();
        combine(k, lk, lv, lr, lt, lb);
    }
}

static void addexpr(void)
{
    int k;
    char lk, lt;
    long lv, lr;
    int lb;
    mulexpr();
    for (;;) {
        if (issym("+") || isid("PLUS")) k = K_ADD;
        else if (issym("-") || isid("MINUS")) k = K_SUB;
        else break;
        nexttok();
        SAVELEFT;
        mulexpr();
        combine(k, lk, lv, lr, lt, lb);
    }
}

static void relexpr(void)
{
    int k;
    char lk, lt;
    long lv, lr;
    int lb;
    addexpr();
    if (issym("=")) k = K_EQ;
    else if (issym("<>")) k = K_NE;
    else if (issym("<")) k = K_LT;
    else if (issym("<=")) k = K_LE;
    else if (issym(">")) k = K_GT;
    else if (issym(">=")) k = K_GE;
    else return;
    nexttok();
    SAVELEFT;
    addexpr();
    combine(k, lk, lv, lr, lt, lb);
}

static void notexpr(void)
{
    long d, m;
    if (!isid("NOT")) {
        relexpr();
        return;
    }
    nexttok();
    notexpr();
    m = rt == 'A' ? 65535L : 255;
    if (rk == 'C') {
        setconst(bitop(K_XOR, rv, m));
        return;
    }
    d = nreg();
    if (rbool == 1) ebini(K_XOR, d, rr, 1);
    else ebini(K_XOR, d, rr, m);
    rr = d;
}

static void andexpr(void)
{
    char lk, lt;
    long lv, lr;
    int lb;
    notexpr();
    while (isid("AND")) {
        nexttok();
        SAVELEFT;
        notexpr();
        combine(K_AND, lk, lv, lr, lt, lb);
    }
}

static void orexpr(void)
{
    int k;
    char lk, lt;
    long lv, lr;
    int lb;
    andexpr();
    for (;;) {
        if (isid("OR")) k = K_OR;
        else if (isid("XOR")) k = K_XOR;
        else break;
        nexttok();
        SAVELEFT;
        andexpr();
        combine(k, lk, lv, lr, lt, lb);
    }
}

void expr(void)
{
    orexpr();
}

long constexpr(void)
{
    expr();
    if (rk != 'C') error("constant expression expected");
    return rv;
}

/* a register that is zero when the condition is false */
long condreg(void)
{
    long d;
    expr();
    if (rk == 'C') return loadc(rv % 2);
    if (rbool == 1) return rr;
    d = nreg();
    ebini(K_AND, d, rr, 1);
    return d;
}
