/* stmt.c -- statements */
#include "plm.h"

int curproc = 0;          /* procedure being compiled, 0 = main */
long curline = 0;
static char **lbn;        /* label names */
static int *lbd;          /* 1 = defined */
static int *lbl;          /* label numbers */
long nlb = 0, lbbase = 0;
static long lcap = 0, lcap2 = 0, lcap3 = 0;

static char *lc(const char *s)
{
    return lowerc(s);
}

static void err3(const char *a, const char *b, const char *c)
{
    error(xcat(a, b, c));
}

static long findlabel(const char *nm)
{
    long i;
    for (i = lbbase + 1; i <= nlb; i++)
        if (!strcmp(lbn[i], nm)) return i;
    nlb++;
    lbn = (char **)xgrow(lbn, &lcap, nlb + 1, sizeof(char *));
    lbd = (int *)xgrow(lbd, &lcap2, nlb + 1, sizeof(int));
    lbl = (int *)xgrow(lbl, &lcap3, nlb + 1, sizeof(int));
    lbn[nlb] = xstrdup(nm);
    lbd[nlb] = 0;
    lbl[nlb] = newlab();
    return nlb;
}

void checklabels(void)
{
    long i;
    for (i = lbbase + 1; i <= nlb; i++)
        if (!lbd[i])
            err3("label '", lc(lbn[i]),
                 "' is used but not defined");
}

/* END, an optional label, and the semicolon */
void endblock(void)
{
    expectid("END");
    if (curk() == 'I') nexttok();
    expectsym(";");
}

/* statements and declarations up to the END of the block */
void blockbody(void)
{
    while (!isid("END")) {
        if (curk() == 'E')
            error("END expected before the end of the file");
        if (isid("DECLARE")) declstmt();
        else {
            if (curk() == 'I' && peekis(1, 'Y', ":") &&
                peekis(2, 'I', "PROCEDURE")) procdecl();
            else statement();
        }
    }
}

static void returnstmt(void)
{
    if (issym(";")) {
        if (curproc > 0) {
            if (proc[curproc].res != 0)
                error("a function must return a value");
            emit(O_RET, 0, 0, 0);
        } else {
            emit(O_PFLUSH, 0, 0, 0);
            emit(O_RET, 0, 0, 0);
        }
        return;
    }
    if (curproc == 0 || proc[curproc].res == 0)
        error("this procedure has no value to return");
    expr();
    coerce(proc[curproc].res);
    toreg();
    emit(O_RETR, rr, 0, 0);
}

static void assignment(void)
{
    const char *nm = cur();
    long x = lookup(nm), port, nt = 0, i;
    long *tbase = NULL, *toff = NULL;
    char *ttyp = NULL;
    long c1 = 0, c2 = 0, c3 = 0;
    if (x > 0 && sym[x].kind == 'B' && !strcmp(sym[x].aux, "OUTPUT")) {
        nexttok();
        expectsym("(");
        expr();
        coerce('B');
        toreg();
        expectsym(")");
        port = holdreg();
        emit(O_MOV, port, rr, 0);
        expectsym("=");
        expr();
        coerce('B');
        toreg();
        emit(O_POUTPUT, port, rr, 0);
        nrbase--;
        return;
    }
    for (;;) {
        designator(0);
        nt++;
        tbase = (long *)xgrow(tbase, &c1, nt + 1, sizeof(long));
        toff = (long *)xgrow(toff, &c2, nt + 1, sizeof(long));
        ttyp = (char *)xgrow(ttyp, &c3, nt + 1, 1);
        tbase[nt] = drb;
        toff[nt] = dro;
        ttyp[nt] = dty;
        if (!acceptsym(",")) break;
        if (curk() != 'I') error("variable expected after ','");
    }
    expectsym("=");
    expr();
    toreg();
    normbool();
    for (i = 1; i <= nt; i++) storeval(ttyp[i], tbase[i], toff[i]);
    free(tbase);
    free(toff);
    free(ttyp);
}

/* call a run time routine with na arguments of the given types */
static void rtproc(int op, int na, const char *types)
{
    long ar[4];
    int i;
    ar[1] = ar[2] = ar[3] = 0;
    if (na > 0) {
        expectsym("(");
        for (i = 1; i <= na; i++) {
            if (i > 1) expectsym(",");
            expr();
            coerce(types[i - 1]);
            toreg();
            ar[i] = rr;
        }
        expectsym(")");
    } else if (issym("(")) {
        nexttok();
        expectsym(")");
    }
    emit(op, ar[1], ar[2], ar[3]);
}

static void callstmt(void)
{
    const char *nm = cur(), *aux;
    long x = lookup(nm), v, wr2;
    int w;
    if (x == 0) err3("'", lc(nm), "' is not declared");
    nexttok();
    if (sym[x].kind == 'P') {
        callproc(x);
        return;
    }
    if (sym[x].kind != 'B') err3("'", lc(nm), "' is not a procedure");
    aux = sym[x].aux;
    if (!strcmp(aux, "CO")) rtproc(O_PCO, 1, "B");
    else if (!strcmp(aux, "PRINT")) rtproc(O_PPRINT, 1, "A");
    else if (!strcmp(aux, "PRNUM")) rtproc(O_PPRNUM, 1, "A");
    else if (!strcmp(aux, "CRLF")) rtproc(O_PCRLF, 0, "");
    else if (!strcmp(aux, "MOVE")) rtproc(O_PMOVE, 3, "AAA");
    else if (!strcmp(aux, "EXIT")) rtproc(O_PEXIT, 1, "B");
    else if (!strcmp(aux, "TIME")) {
        expectsym("(");
        expr();
        expectsym(")");
    } else if (!strcmp(aux, "PRHEX")) {
        wr2 = 0;
        expectsym("(");
        expr();
        w = 2;
        if (rt == 'A') w = 4;
        coerce('A');
        toreg();
        v = rr;
        if (acceptsym(",")) {
            expr();
            coerce('B');
            toreg();
            w = -1;
            wr2 = rr;
        }
        expectsym(")");
        if (w >= 0) wr2 = loadc(w);
        emit(O_PRHEX, v, wr2, 0);
    } else if (!strcmp(aux, "UNSUPPORTED"))
        err3("'", lc(nm), "' is not supported by this compiler");
    else err3("'", lc(nm), "' is a function; use it in an expression");
}

static void ifstmt(void)
{
    long r;
    int lelse, lend;
    nexttok();
    r = condreg();
    expectid("THEN");
    lelse = newlab();
    branchop(O_BRF, r, lelse);
    statement();
    if (isid("ELSE")) {
        nexttok();
        lend = newlab();
        emit(O_BR, lend, 0, 0);
        deflab(lelse);
        statement();
        deflab(lend);
    } else deflab(lelse);
}

static void casestmt(void)
{
    long svbase, h, n = 0, j, ccap = 0;
    int ldisp, lend;
    int *cl = NULL;
    nexttok();
    expr();
    toreg();
    normbool();
    svbase = nrbase;
    h = holdreg();
    emit(O_MOV, h, rr, 0);
    expectsym(";");
    ldisp = newlab();
    lend = newlab();
    emit(O_BR, ldisp, 0, 0);
    while (!isid("END")) {
        if (curk() == 'E')
            error("END expected before the end of the file");
        n++;
        cl = (int *)xgrow(cl, &ccap, n + 1, sizeof(int));
        cl[n] = newlab();
        deflab(cl[n]);
        statement();
        emit(O_BR, lend, 0, 0);
    }
    deflab(ldisp);
    nr = nrbase;
    emit(O_CASE, h, n, 0);
    for (j = 1; j <= n; j++) emit(O_NOP, cl[j], 0, 0);
    deflab(lend);
    endblock();
    nrbase = svbase;
    nr = nrbase;
    free(cl);
}

/* DO var = start TO limit [BY step]; ... END; */
static void iterdo(void)
{
    const char *nm = cur();
    long x = lookup(nm), svbase, hl, hs = 0, stepc = 1, v, t, n2, t2;
    long b2, o2;
    int lbody, lend;
    if (x == 0) err3("'", lc(nm), "' is not declared");
    if (sym[x].kind != 'V' || sym[x].cnt >= 0 || sym[x].type == 'S')
        err3("'", lc(nm), "' cannot be the index of a DO loop");
    nexttok();
    expectsym("=");
    expr();
    toreg();
    symdesig(x);
    storeval(sym[x].type, drb, dro);
    expectid("TO");
    svbase = nrbase;
    hl = holdreg();
    expr();
    toreg();
    normbool();
    emit(O_MOV, hl, rr, 0);
    if (isid("BY")) {
        nexttok();
        expr();
        if (rk == 'C') stepc = rv;
        else {
            toreg();
            normbool();
            stepc = -1;
            hs = holdreg();
            emit(O_MOV, hs, rr, 0);
        }
    }
    expectsym(";");
    lbody = newlab();
    lend = newlab();
    nr = nrbase;
    symdesig(x);
    v = rdloc(sym[x].type, drb, dro);
    t = nreg();
    ebin(K_LE, t, v, hl);
    branchop(O_BRF, t, lend);
    deflab(lbody);
    blockbody();
    expectid("END");
    if (curk() == 'I') nexttok();
    nr = nrbase;
    symdesig(x);
    v = rdloc(sym[x].type, drb, dro);
    n2 = nreg();
    if (stepc >= 0) ebini(K_ADD, n2, v, stepc);
    else ebin(K_ADD, n2, v, hs);
    rk = 'R';
    rr = n2;
    rt = 'A';
    rbool = 0;
    b2 = drb;
    o2 = dro;
    storeval(sym[x].type, b2, o2);
    t2 = nreg();
    ebin(K_LE, t2, n2, hl);
    branchop(O_BRT, t2, lbody);
    deflab(lend);
    expectsym(";");
    nrbase = svbase;
    nr = nrbase;
}

/* DO; ... END; */
static void doblock(void)
{
    pushscope();
    blockbody();
    popscope();
    endblock();
}

static void dostmt(void)
{
    long r;
    int ltop, lend;
    nexttok();
    if (issym(";")) {
        nexttok();
        doblock();
        return;
    }
    if (isid("WHILE")) {
        nexttok();
        ltop = newlab();
        lend = newlab();
        deflab(ltop);
        r = condreg();
        expectsym(";");
        branchop(O_BRF, r, lend);
        blockbody();
        endblock();
        emit(O_BR, ltop, 0, 0);
        deflab(lend);
        return;
    }
    if (isid("CASE")) {
        casestmt();
        return;
    }
    if (curk() == 'I') {
        iterdo();
        return;
    }
    error("';', WHILE, CASE or a variable expected after DO");
}

void statement(void)
{
    const char *nm;
    long k;
    nr = nrbase;
    curline = tok[ti].ln;
    if (curk() == 'I' && peekis(1, 'Y', ":")) {
        nm = cur();
        nexttok();
        nexttok();
        k = findlabel(nm);
        if (lbd[k] == 1)
            err3("label '", lc(nm), "' is defined twice");
        lbd[k] = 1;
        deflab(lbl[k]);
        if (isid("END")) return;
        statement();
        return;
    }
    if (issym(";")) {
        nexttok();
        return;
    }
    if (isid("IF")) {
        ifstmt();
        return;
    }
    if (isid("DO")) {
        dostmt();
        return;
    }
    if (isid("CALL")) {
        nexttok();
        callstmt();
        expectsym(";");
        return;
    }
    if (isid("RETURN")) {
        nexttok();
        returnstmt();
        expectsym(";");
        return;
    }
    if (isid("GOTO") || isid("GO")) {
        if (isid("GO")) {
            nexttok();
            expectid("TO");
        } else nexttok();
        nm = getident();
        k = findlabel(nm);
        emit(O_BR, lbl[k], 0, 0);
        expectsym(";");
        return;
    }
    if (isid("HALT")) {
        nexttok();
        emit(O_PHALT, 0, 0, 0);
        expectsym(";");
        return;
    }
    if (isid("ENABLE") || isid("DISABLE")) {
        nexttok();
        expectsym(";");
        return;
    }
    if (curk() == 'I') {
        assignment();
        expectsym(";");
        return;
    }
    err3("statement expected, found '", cur(), "'");
}
