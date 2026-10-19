/* decl.c -- declarations, procedures and the module */
#include "plm.h"

static char **pendn;      /* parameters whose declaration is to come */
static long *pendk;
static long npend = 0, pcap1 = 0, pcap2 = 0;

static char *lc(const char *s)
{
    return lowerc(s);
}

static void err3(const char *a, const char *b, const char *c)
{
    error(xcat(a, b, c));
}

static void notmember(const char *mnm, const char *nm)
{
    error(xcat(xcat("'", lc(mnm), "' is not a member of '"),
               lc(nm), "'"));
}

/* STRUCTURE ( member, ... ): returns the structure type number */
static long structtype(void)
{
    long id, off = 0, cnt = 0, n, sz, m;
    nexttok();
    expectsym("(");
    id = newstruct();
    stype[id].m0 = (int)nmem + 1;
    for (;;) {
        m = newmember();
        memb[m].name = getident();
        memb[m].cnt = -1;
        if (issym("(")) {
            nexttok();
            n = constexpr();
            if (n < 1) error("bad dimension");
            memb[m].cnt = n;
            expectsym(")");
        }
        if (isid("BYTE")) memb[m].type = 'B';
        else if (isid("ADDRESS")) memb[m].type = 'A';
        else error("BYTE or ADDRESS expected for a structure member");
        nexttok();
        memb[m].off = off;
        sz = elsize(memb[m].type, 0);
        if (memb[m].cnt >= 0) sz *= memb[m].cnt;
        off += sz;
        cnt++;
        if (!acceptsym(",")) break;
    }
    expectsym(")");
    stype[id].size = off;
    stype[id].nm = (int)cnt;
    return id;
}

static void bindparam(const char *name, long adr, char ty, long cnt,
                      long bptr)
{
    long j, q;
    for (j = 1; j <= npend; j++)
        if (!strcmp(pendn[j], name)) {
            if (cnt >= 0 || ty == 'S' || bptr >= 0)
                err3("parameter '", lc(name),
                     "' must be a BYTE or an ADDRESS");
            q = proc[curproc].p0 + pendk[j] - 1;
            par[q].adr = adr;
            par[q].type = ty;
            pendn[j] = "";
        }
}

static long basedptr(void)
{
    const char *pnm;
    char *mnm;
    long px, bptr, m;
    nexttok();
    pnm = getident();
    px = lookup(pnm);
    if (px == 0) err3("'", lc(pnm), "' is not declared");
    if (sym[px].kind != 'V') err3("'", lc(pnm), "' is not a variable");
    if (sym[px].bp >= 0)
        error("the pointer of a BASED variable cannot itself "
              "be BASED");
    bptr = sym[px].adr;
    if (sym[px].type == 'S') {
        expectsym(".");
        mnm = getident();
        m = findmember(sym[px].st, mnm);
        if (m == 0) notmember(mnm, pnm);
        if (memb[m].type != 'A')
            error("the pointer of a BASED variable must be an "
                  "ADDRESS");
        bptr += memb[m].off;
    } else if (sym[px].type != 'A')
        error("the pointer of a BASED variable must be an "
              "ADDRESS");
    return bptr;
}

static void declitem(void)
{
    char **names = NULL;
    long nn = 0, ncap = 0, dim = -1, stp = 0, bptr = -1, atad = -1;
    long nitems = 0, nbytes = 0, cnt, esz, total, adr, x, i;
    int hasinit = 0;
    char ty = 0, et;
    unsigned char *hx = NULL;
    if (issym("(")) {
        nexttok();
        for (;;) {
            nn++;
            names = (char **)xgrow(names, &ncap, nn + 1,
                                   sizeof(char *));
            names[nn] = getident();
            if (!acceptsym(",")) break;
        }
        expectsym(")");
    } else {
        nn = 1;
        names = (char **)xgrow(names, &ncap, 2, sizeof(char *));
        names[1] = getident();
    }
    if (issym("(")) {
        nexttok();
        if (issym("*")) {
            nexttok();
            dim = -2;
        } else {
            dim = constexpr();
            if (dim < 1) error("bad dimension");
        }
        expectsym(")");
    }
    for (;;) {
        if (isid("LITERALLY")) {
            if (nn != 1 || dim != -1)
                error("LITERALLY applies to a single name");
            nexttok();
            if (curk() != 'S')
                error("a string is expected after LITERALLY");
            defmacro(names[1], cur());
            nexttok();
            return;
        }
        if (isid("BYTE")) {
            ty = 'B';
            nexttok();
        } else if (isid("ADDRESS")) {
            ty = 'A';
            nexttok();
        } else if (isid("STRUCTURE")) {
            stp = structtype();
            ty = 'S';
        } else if (isid("LABEL")) {
            ty = 'L';
            nexttok();
        } else if (isid("BASED")) bptr = basedptr();
        else if (isid("AT")) {
            nexttok();
            expectsym("(");
            atad = constexpr();
            expectsym(")");
        } else if (isid("INITIAL") || isid("DATA")) {
            nexttok();
            et = ty;
            if (et == 0 || et == 'S') et = 'B';
            hx = constlist(et, &nbytes);
            nitems = clitems;
            hasinit = 1;
        } else if (isid("PUBLIC")) nexttok();
        else if (isid("EXTERNAL")) error("EXTERNAL is not supported");
        else break;
    }
    if (ty == 'L') return;
    if (ty == 0)
        error("a type (BYTE, ADDRESS or STRUCTURE) is expected");
    cnt = dim;
    if (dim == -2) {
        if (!hasinit) error("a dimension of * needs INITIAL or DATA");
        cnt = nitems;
    }
    if (hasinit && ty == 'S')
        error("INITIAL for structures is not supported");
    if (hasinit && bptr >= 0)
        error("a BASED variable cannot have an initial value");
    esz = elsize(ty, stp);
    total = esz;
    if (cnt >= 0) total = esz * cnt;
    if (atad >= 0 && nn > 1) error("AT applies to a single name");
    for (i = 1; i <= nn; i++) {
        if (bptr >= 0) adr = 0;
        else if (atad >= 0) adr = atad;
        else adr = alloc(total);
        x = addsym(names[i], 'V', ty, cnt, adr);
        sym[x].bp = bptr;
        sym[x].st = (int)stp;
        bindparam(names[i], adr, ty, cnt, bptr);
        if (hasinit) {
            if (cnt >= 0 && nitems > cnt)
                err3("too many initial values for '", lc(names[i]),
                     "'");
            if (cnt < 0 && nbytes > esz)
                err3("too many initial values for '", lc(names[i]),
                     "'");
            initbytes(adr, hx, nbytes);
        }
    }
}

void declstmt(void)
{
    nexttok();
    for (;;) {
        declitem();
        if (!acceptsym(",")) break;
    }
    expectsym(";");
}

/* ------------------------------------------------------ procedures */
void procdecl(void)
{
    char *nm;
    long p0, cnt = 0, pid, x, q, j, svnr, svbase, svmax, svnlb;
    long svlbbase, svline;
    char res = 0;
    int reent = 0;
    if (curproc > 0) error("procedures cannot be nested");
    nm = getident();
    expectsym(":");
    expectid("PROCEDURE");
    p0 = npar + 1;
    npend = 0;
    if (issym("(")) {
        nexttok();
        for (;;) {
            cnt++;
            q = newpar();
            par[q].name = getident();
            par[q].adr = 0;
            par[q].type = 'B';
            npend++;
            pendn = (char **)xgrow(pendn, &pcap1, npend + 1,
                                   sizeof(char *));
            pendk = (long *)xgrow(pendk, &pcap2, npend + 1,
                                  sizeof(long));
            pendn[npend] = par[q].name;
            pendk[npend] = cnt;
            if (!acceptsym(",")) break;
        }
        expectsym(")");
    }
    if (isid("BYTE")) {
        res = 'B';
        nexttok();
    } else if (isid("ADDRESS")) {
        res = 'A';
        nexttok();
    }
    for (;;) {
        if (isid("PUBLIC")) nexttok();
        else if (isid("REENTRANT")) {
            reent = 1;
            nexttok();
        } else if (isid("INTERRUPT")) {
            nexttok();
            (void)constexpr();
        } else if (isid("EXTERNAL"))
            error("EXTERNAL procedures are not supported");
        else break;
    }
    expectsym(";");
    pid = newproc();
    proc[pid].name = nm;
    proc[pid].res = res;
    proc[pid].np = (int)cnt;
    proc[pid].p0 = (int)p0;
    proc[pid].reent = reent;
    proc[pid].lo = memtop;
    proc[pid].hi = memtop;
    x = addsym(nm, 'P', res, -1, 0);
    sym[x].pid = (int)pid;
    svnr = nr;
    svbase = nrbase;
    svmax = maxr;
    svnlb = nlb;
    svlbbase = lbbase;
    svline = curline;
    cc = &proc[pid].code;
    nr = 1;
    nrbase = 1;
    maxr = 1;
    curproc = (int)pid;
    lbbase = nlb;
    pushscope();
    blockbody();
    popscope();
    expectid("END");
    if (curk() == 'I') nexttok();
    expectsym(";");
    for (j = 1; j <= npend; j++)
        if (pendn[j][0])
            errorat(xcat(xcat("parameter '", lc(pendn[j]),
                              "' of '"),
                         lc(nm), "' is not declared"), svline);
    npend = 0;
    checklabels();
    emit(O_RET, 0, 0, 0);
    proc[pid].nregs = maxr + 2;
    proc[pid].hi = memtop;
    savepatch(pid);
    finishcode(cc);
    cc = &maincode;
    nr = svnr;
    nrbase = svbase;
    maxr = svmax;
    curproc = 0;
    nlb = svnlb;
    lbbase = svlbbase;
}

/* ------------------------------------------------------ module */
void compilemodule(void)
{
    ti = 1;
    settle();
    (void)getident();
    expectsym(":");
    expectid("DO");
    expectsym(";");
    pushscope();
    blockbody();
    popscope();
    expectid("END");
    if (curk() == 'I' && !isid("EOF")) nexttok();
    expectsym(";");
    if (isid("EOF")) nexttok();
    if (curk() != 'E') error("text after the end of the module");
    checklabels();
    emit(O_PFLUSH, 0, 0, 0);
    emit(O_RET, 0, 0, 0);
    mmax = maxr;
    if (mmax < 5) mmax = 5;
    finishcode(&maincode);
}
