/* emit.c -- code buffers, labels, registers */
#include "plm.h"

Code maincode;            /* code of the main program */
Code *cc = &maincode;     /* code being generated */
long nr = 1;              /* last allocated register */
long nrbase = 1;          /* registers up to here are held by loops */
long maxr = 1;            /* highest register used in this procedure */
long mmax = 5;            /* highest register of the main program */

static long *labpc;       /* label number -> instruction index */
static long nlab = 0, labcap = 0;
static long lastlab = -1; /* index at which the last label was set */

long emit(int op, long a, long b, long c)
{
    Ins *p;
    cc->v = (Ins *)xgrow(cc->v, &cc->cap, cc->n + 1, sizeof(Ins));
    p = &cc->v[cc->n];
    p->op = op;
    p->a = a;
    p->b = b;
    p->c = c;
    return cc->n++;
}

long curn(void)
{
    return cc->n;
}

int newlab(void)
{
    nlab++;
    labpc = (long *)xgrow(labpc, &labcap, nlab + 1, sizeof(long));
    labpc[nlab] = -1;
    return (int)nlab;
}

void deflab(int lab)
{
    labpc[lab] = cc->n;
    lastlab = cc->n;
}

long nreg(void)
{
    nr++;
    if (nr > maxr) maxr = nr;
    return nr;
}

/* keep a register for the length of a loop or case */
long holdreg(void)
{
    nrbase++;
    nr = nrbase;
    if (nr > maxr) maxr = nr;
    return nrbase;
}

long loadc(long v)
{
    long d = nreg();
    emit(O_LOADI, d, v, 0);
    return d;
}

void ebin(int k, long d, long x, long y)
{
    emit(O_ADD + k, d, x, y);
}

void ebini(int k, long d, long x, long y)
{
    emit(O_ADDI + k, d, x, y);
}

/* BRF or BRT on a register; a comparison just before it is fused */
void branchop(int op, long reg, int lab)
{
    Ins *p;
    int k, imm;
    if (cc->n > 0 && lastlab != cc->n) {
        p = &cc->v[cc->n - 1];
        imm = p->op >= O_EQI && p->op <= O_GEI;
        if (p->a == reg && ((p->op >= O_EQ && p->op <= O_GE) || imm)) {
            k = imm ? p->op - O_EQI : p->op - O_EQ;
            if (op == O_BRF) k = k < 2 ? k ^ 1 : 7 - k;
            p->op = (imm ? O_BEQI : O_BEQ) + k;
            p->a = p->b;
            p->b = p->c;
            p->c = lab;
            return;
        }
    }
    emit(op, reg, lab, 0);
}

/* replace the label numbers by instruction indexes */
void finishcode(Code *c)
{
    long i, j;
    Ins *p;
    for (i = 0; i < c->n; i++) {
        p = &c->v[i];
        switch (p->op) {
        case O_BR:
            p->a = labpc[p->a];
            break;
        case O_BRF: case O_BRT:
            p->b = labpc[p->b];
            break;
        case O_BEQ: case O_BNE: case O_BLT:
        case O_BLE: case O_BGT: case O_BGE:
        case O_BEQI: case O_BNEI: case O_BLTI:
        case O_BLEI: case O_BGTI: case O_BGEI:
            p->c = labpc[p->c];
            break;
        case O_CASE:
            for (j = 1; j <= p->b; j++)
                c->v[i + j].a = labpc[c->v[i + j].a];
            break;
        default:
            break;
        }
    }
}
