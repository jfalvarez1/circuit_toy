/**
 * Circuit Playground - sketch interpreter
 *
 * Arduino-shaped code compiled to bytecode and run against the simulation clock. See sketch.h
 * for what this is and is not.
 *
 * Bytecode rather than a tree walk, for one reason: delay(). A sketch that blinks spends all of
 * its life paused in the middle of loop(), and it has to come back to exactly where it was
 * three thousand solver steps later. With a program counter that is a stored integer; with a
 * tree walker it is a continuation, and every statement form has to be written twice.
 */

#include "sketch.h"

#include <ctype.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define SK_MAX_CODE     4096
#define SK_MAX_GLOBALS  64
#define SK_MAX_LOCALS   256
#define SK_MAX_FUNCS    32
#define SK_MAX_PARAMS   8
#define SK_MAX_STACK    128
#define SK_MAX_FRAMES   32
#define SK_MAX_STRINGS  32
#define SK_NAME         24
#define SK_MAX_TOKENS   4096
/* Instructions per call to sketch_advance. A sketch is meant to do a little work and then wait,
   so this is generous - but a `while (1) {}` with no delay in it must not lock the program up,
   and on real hardware that hangs too. Running out of budget just means the rest happens on the
   next solver step. */
#define SK_BUDGET       20000

/* ---------------------------------------------------------------- lexer */

typedef enum { TK_EOF, TK_NUM, TK_ID, TK_STR, TK_PUNCT } TokKind;

typedef struct {
    TokKind kind;
    double num;
    char text[SK_NAME];
    int line;
} Token;

typedef enum {
    OP_CONST, OP_LOADG, OP_STOREG, OP_LOADL, OP_STOREL,
    OP_ADD, OP_SUB, OP_MUL, OP_DIV, OP_MOD, OP_NEG, OP_NOT,
    OP_EQ, OP_NE, OP_LT, OP_LE, OP_GT, OP_GE,
    OP_JMP, OP_JZ, OP_JZ_KEEP, OP_JNZ_KEEP,
    OP_POP, OP_DUP,
    OP_CALL, OP_RET, OP_BUILTIN, OP_PRINTS, OP_HALT
} Op;

typedef struct { unsigned char op; int a; double d; } Instr;

typedef enum {
    B_PINMODE, B_DIGITALWRITE, B_DIGITALREAD, B_ANALOGWRITE, B_ANALOGREAD,
    B_DELAY, B_DELAYUS, B_MILLIS, B_MICROS,
    B_PRINT, B_PRINTLN, B_NOP,
    B_MAP, B_CONSTRAIN, B_MIN, B_MAX, B_ABS, B_SQRT, B_POW,
    B_COUNT
} Builtin;

static const struct { const char *name; int argc; } sk_builtins[B_COUNT] = {
    [B_PINMODE]      = { "pinMode", 2 },
    [B_DIGITALWRITE] = { "digitalWrite", 2 },
    [B_DIGITALREAD]  = { "digitalRead", 1 },
    [B_ANALOGWRITE]  = { "analogWrite", 2 },
    [B_ANALOGREAD]   = { "analogRead", 1 },
    [B_DELAY]        = { "delay", 1 },
    [B_DELAYUS]      = { "delayMicroseconds", 1 },
    [B_MILLIS]       = { "millis", 0 },
    [B_MICROS]       = { "micros", 0 },
    [B_PRINT]        = { "print", 1 },
    [B_PRINTLN]      = { "println", 1 },
    [B_NOP]          = { "begin", -1 },
    [B_MAP]          = { "map", 5 },
    [B_CONSTRAIN]    = { "constrain", 3 },
    [B_MIN]          = { "min", 2 },
    [B_MAX]          = { "max", 2 },
    [B_ABS]          = { "abs", 1 },
    [B_SQRT]         = { "sqrt", 1 },
    [B_POW]          = { "pow", 2 },
};

typedef struct {
    char name[SK_NAME];
    int entry;                       /* first instruction */
    int nparams;
    int nlocals;
    bool defined;
} SkFunc;

typedef struct { int ret_pc, base, func; } Frame;

typedef enum { SK_INIT, SK_SETUP, SK_LOOP, SK_DONE, SK_FAULT } SkState;

struct Sketch {
    Instr code[SK_MAX_CODE];
    int ncode;

    char gname[SK_MAX_GLOBALS][SK_NAME];
    double globals[SK_MAX_GLOBALS];
    int nglobals;

    SkFunc funcs[SK_MAX_FUNCS];
    int nfuncs;
    int fn_setup, fn_loop;
    /* Where the global initialisers live. They are compiled in a pass of their own, after every
       function body, so that they are one contiguous run the driver can enter - in the source
       they are interleaved with the functions, and starting at instruction zero would drop the
       program into whichever function happened to be written first. */
    int init_entry;

    char strings[SK_MAX_STRINGS][48];
    int nstrings;

    /* run state */
    double stack[SK_MAX_STACK];
    int sp;
    double locals[SK_MAX_LOCALS];
    Frame frames[SK_MAX_FRAMES];
    int fp;
    int pc;
    SkState state;

    double t;                        /* simulated seconds, from the caller */
    double wake;                     /* not before this */
    bool waiting;
    long loops;
    double shortest_delay;           /* the finest the sketch has asked for; 0 = none yet */

    int pin_mode[SKETCH_MAX_PINS];
    double pin_duty[SKETCH_MAX_PINS];
    bool pin_pwm[SKETCH_MAX_PINS];
    double pin_volts[SKETCH_MAX_PINS];
    double vcc;

    char last_print[96];
    char err[128];
    bool faulted;
};

/* ---------------------------------------------------------------- compiler state */

typedef struct {
    Token *tok;
    int ntok;
    int p;
    Sketch *s;
    char *err;
    size_t err_size;
    bool failed;

    /* the function being compiled */
    char lname[SK_MAX_PARAMS + 32][SK_NAME];
    int nlocals;
    int cur_func;

    int break_patch[16], nbreak;
    int cont_patch[16], ncont;
    int loop_depth;
    /* Set while a pass is walking code it does not want to keep - the function pass crossing a
       global declaration, which it parses only to give the variable its slot. */
    bool emit_off;
} Comp;

static void sk_fail(Comp *c, int line, const char *msg) {
    if (c->failed) return;
    c->failed = true;
    if (c->err && c->err_size) snprintf(c->err, c->err_size, "line %d: %s", line, msg);
}

static int sk_lex(const char *src, Token *out, int max, char *err, size_t err_size) {
    int n = 0, line = 1;
    const char *p = src;
    while (*p) {
        if (*p == '\n') { line++; p++; continue; }
        if (isspace((unsigned char)*p)) { p++; continue; }
        if (p[0] == '/' && p[1] == '/') { while (*p && *p != '\n') p++; continue; }
        if (p[0] == '/' && p[1] == '*') {
            p += 2;
            while (*p && !(p[0] == '*' && p[1] == '/')) { if (*p == '\n') line++; p++; }
            if (*p) p += 2;
            continue;
        }
        /* A preprocessor line. #include and #define are the two that show up, and neither
           changes what the pins do; skipping the line is more use than refusing the sketch. */
        if (*p == '#') { while (*p && *p != '\n') p++; continue; }
        if (n >= max) { snprintf(err, err_size, "line %d: sketch is too long", line); return -1; }
        Token *t = &out[n];
        t->line = line;
        t->text[0] = 0;
        if (isdigit((unsigned char)*p) || (*p == '.' && isdigit((unsigned char)p[1]))) {
            char *end = NULL;
            /* 0x.. as well as decimal: a pin mask written in hex is common enough. */
            t->num = (p[0] == '0' && (p[1] == 'x' || p[1] == 'X'))
                     ? (double)strtol(p, &end, 16) : strtod(p, &end);
            p = end;
            /* an integer suffix says nothing this interpreter needs */
            while (*p == 'U' || *p == 'u' || *p == 'L' || *p == 'l' || *p == 'F' || *p == 'f') p++;
            t->kind = TK_NUM;
            n++;
            continue;
        }
        if (isalpha((unsigned char)*p) || *p == '_') {
            int k = 0;
            while (isalnum((unsigned char)*p) || *p == '_') {
                if (k < SK_NAME - 1) t->text[k++] = *p;
                p++;
            }
            t->text[k] = 0;
            t->kind = TK_ID;
            n++;
            continue;
        }
        if (*p == '"') {
            p++;
            int k = 0;
            while (*p && *p != '"') {
                if (*p == '\\' && p[1]) { p++; }
                if (k < SK_NAME - 1) t->text[k++] = *p;
                p++;
            }
            if (*p == '"') p++;
            t->text[k] = 0;
            t->kind = TK_STR;
            n++;
            continue;
        }
        /* two-character operators first, or == lexes as = = */
        static const char *two[] = { "==", "!=", "<=", ">=", "&&", "||", "++", "--",
                                     "+=", "-=", "*=", "/=", NULL };
        int matched = 0;
        for (int i = 0; two[i]; i++) {
            if (p[0] == two[i][0] && p[1] == two[i][1]) {
                t->text[0] = p[0]; t->text[1] = p[1]; t->text[2] = 0;
                p += 2; matched = 1; break;
            }
        }
        if (!matched) { t->text[0] = *p++; t->text[1] = 0; }
        t->kind = TK_PUNCT;
        n++;
    }
    if (n < max) { out[n].kind = TK_EOF; out[n].line = line; out[n].text[0] = 0; }
    return n;
}

/* ---------------------------------------------------------------- token helpers */

static Token *cur(Comp *c) { return &c->tok[c->p]; }

static bool is_punct(Comp *c, const char *s) {
    return cur(c)->kind == TK_PUNCT && strcmp(cur(c)->text, s) == 0;
}
static bool is_id(Comp *c, const char *s) {
    return cur(c)->kind == TK_ID && strcmp(cur(c)->text, s) == 0;
}
static bool accept_punct(Comp *c, const char *s) {
    if (is_punct(c, s)) { c->p++; return true; }
    return false;
}
static bool accept_id(Comp *c, const char *s) {
    if (is_id(c, s)) { c->p++; return true; }
    return false;
}
static void expect_punct(Comp *c, const char *s) {
    if (!accept_punct(c, s)) {
        char m[64];
        snprintf(m, sizeof m, "expected '%s'", s);
        sk_fail(c, cur(c)->line, m);
        /* do not spin on the offending token */
        if (cur(c)->kind != TK_EOF) c->p++;
    }
}

static bool is_type_word(const char *w) {
    static const char *types[] = { "int", "long", "short", "float", "double", "bool", "boolean",
                                   "byte", "char", "unsigned", "signed", "void", "const",
                                   "static", "uint8_t", "uint16_t", "uint32_t", "int8_t",
                                   "int16_t", "int32_t", "size_t", NULL };
    for (int i = 0; types[i]; i++) if (strcmp(w, types[i]) == 0) return true;
    return false;
}

/* ---------------------------------------------------------------- emit */

static int emit(Comp *c, Op op, int a, double d) {
    if (c->emit_off) return -1;
    if (c->s->ncode >= SK_MAX_CODE) {
        sk_fail(c, cur(c)->line, "sketch is too long to compile");
        return c->s->ncode - 1;
    }
    Instr *i = &c->s->code[c->s->ncode];
    i->op = (unsigned char)op; i->a = a; i->d = d;
    return c->s->ncode++;
}
static void patch(Comp *c, int at, int target) {
    if (at >= 0 && at < c->s->ncode) c->s->code[at].a = target;
}
static int here(Comp *c) { return c->s->ncode; }

static int global_slot(Comp *c, const char *name, bool create) {
    Sketch *s = c->s;
    for (int i = 0; i < s->nglobals; i++) if (strcmp(s->gname[i], name) == 0) return i;
    if (!create) return -1;
    if (s->nglobals >= SK_MAX_GLOBALS) { sk_fail(c, cur(c)->line, "too many variables"); return 0; }
    snprintf(s->gname[s->nglobals], SK_NAME, "%s", name);
    s->globals[s->nglobals] = 0;
    return s->nglobals++;
}

static int local_slot(Comp *c, const char *name, bool create) {
    for (int i = 0; i < c->nlocals; i++) if (strcmp(c->lname[i], name) == 0) return i;
    if (!create) return -1;
    if (c->nlocals >= (int)(sizeof c->lname / sizeof c->lname[0])) {
        sk_fail(c, cur(c)->line, "too many local variables in one function");
        return 0;
    }
    snprintf(c->lname[c->nlocals], SK_NAME, "%s", name);
    return c->nlocals++;
}

static int func_index(Comp *c, const char *name) {
    for (int i = 0; i < c->s->nfuncs; i++) if (strcmp(c->s->funcs[i].name, name) == 0) return i;
    return -1;
}

/* ---------------------------------------------------------------- named constants */

static bool named_constant(const char *w, double *out) {
    if (!strcmp(w, "HIGH") || !strcmp(w, "true"))        { *out = 1; return true; }
    if (!strcmp(w, "LOW") || !strcmp(w, "false"))        { *out = 0; return true; }
    if (!strcmp(w, "INPUT"))                             { *out = SKETCH_PIN_INPUT; return true; }
    if (!strcmp(w, "OUTPUT"))                            { *out = SKETCH_PIN_OUTPUT; return true; }
    if (!strcmp(w, "INPUT_PULLUP"))                      { *out = SKETCH_PIN_INPUT_PULLUP; return true; }
    if (!strcmp(w, "LED_BUILTIN"))                       { *out = 13; return true; }
    if (w[0] == 'A' && isdigit((unsigned char)w[1]) && w[2] == 0) {
        *out = SKETCH_A0 + (w[1] - '0');
        return true;
    }
    return false;
}

/* ---------------------------------------------------------------- expressions */

static void expr(Comp *c);

static void emit_load(Comp *c, const char *name, int line) {
    int l = local_slot(c, name, false);
    if (l >= 0) { emit(c, OP_LOADL, l, 0); return; }
    int g = global_slot(c, name, false);
    if (g >= 0) { emit(c, OP_LOADG, g, 0); return; }
    double k;
    if (named_constant(name, &k)) { emit(c, OP_CONST, 0, k); return; }
    char m[64];
    snprintf(m, sizeof m, "'%s' is not defined", name);
    sk_fail(c, line, m);
    emit(c, OP_CONST, 0, 0);
}

static void emit_store(Comp *c, const char *name, int line) {
    int l = local_slot(c, name, false);
    if (l >= 0) { emit(c, OP_STOREL, l, 0); return; }
    int g = global_slot(c, name, false);
    if (g >= 0) { emit(c, OP_STOREG, g, 0); return; }
    char m[64];
    snprintf(m, sizeof m, "'%s' is not defined", name);
    sk_fail(c, line, m);
    emit(c, OP_POP, 0, 0);
}

static int builtin_by_name(const char *name) {
    for (int i = 0; i < B_COUNT; i++)
        if (sk_builtins[i].name && strcmp(sk_builtins[i].name, name) == 0) return i;
    return -1;
}

/* A call whose arguments are already known to be an argument list. Emits them left to right,
   which is the order the VM pops them back off in. */
static int call_args(Comp *c) {
    int n = 0;
    expect_punct(c, "(");
    if (!is_punct(c, ")")) {
        do {
            /* Serial.print("text") is the one place a string can appear, and it is handled by
               the caller; anything else here is a number. */
            expr(c);
            n++;
        } while (accept_punct(c, ","));
    }
    expect_punct(c, ")");
    return n;
}

static void primary(Comp *c) {
    Token *t = cur(c);
    if (t->kind == TK_NUM) { c->p++; emit(c, OP_CONST, 0, t->num); return; }
    if (accept_punct(c, "(")) { expr(c); expect_punct(c, ")"); return; }
    if (accept_punct(c, "-")) { primary(c); emit(c, OP_NEG, 0, 0); return; }
    if (accept_punct(c, "+")) { primary(c); return; }
    if (accept_punct(c, "!")) { primary(c); emit(c, OP_NOT, 0, 0); return; }
    if (is_punct(c, "++") || is_punct(c, "--")) {
        bool inc = is_punct(c, "++");
        c->p++;
        Token *v = cur(c);
        if (v->kind != TK_ID) { sk_fail(c, v->line, "expected a variable after ++"); return; }
        c->p++;
        emit_load(c, v->text, v->line);
        emit(c, OP_CONST, 0, 1);
        emit(c, inc ? OP_ADD : OP_SUB, 0, 0);
        emit(c, OP_DUP, 0, 0);
        emit_store(c, v->text, v->line);
        return;
    }
    if (t->kind == TK_ID) {
        char name[SK_NAME];
        snprintf(name, sizeof name, "%s", t->text);
        int line = t->line;
        c->p++;

        /* Serial.print / Serial.println / Serial.begin. Serial is not an object here; it is a
           name followed by a dot, and the method after it is all that is looked at. */
        if (strcmp(name, "Serial") == 0 && is_punct(c, ".")) {
            c->p++;
            Token *m = cur(c);
            if (m->kind != TK_ID) { sk_fail(c, m->line, "expected a Serial method"); return; }
            char method[SK_NAME];
            snprintf(method, sizeof method, "%s", m->text);
            c->p++;
            if (strcmp(method, "begin") == 0 || strcmp(method, "flush") == 0 ||
                strcmp(method, "end") == 0 || strcmp(method, "available") == 0) {
                (void)call_args(c);
                emit(c, OP_CONST, 0, 0);
                return;
            }
            bool ln = (strcmp(method, "println") == 0);
            if (!ln && strcmp(method, "print") != 0) {
                char msg[64];
                snprintf(msg, sizeof msg, "Serial.%s is not supported", method);
                sk_fail(c, line, msg);
                return;
            }
            expect_punct(c, "(");
            if (cur(c)->kind == TK_STR) {
                Sketch *s = c->s;
                int idx = 0;
                if (s->nstrings < SK_MAX_STRINGS) {
                    idx = s->nstrings;
                    snprintf(s->strings[idx], sizeof s->strings[0], "%s", cur(c)->text);
                    s->nstrings++;
                }
                c->p++;
                emit(c, OP_PRINTS, idx, 0);
            } else if (is_punct(c, ")")) {
                emit(c, OP_CONST, 0, 0);
                emit(c, OP_BUILTIN, ln ? B_PRINTLN : B_PRINT, 0);
            } else {
                expr(c);
                emit(c, OP_BUILTIN, ln ? B_PRINTLN : B_PRINT, 0);
            }
            expect_punct(c, ")");
            return;
        }

        if (is_punct(c, "(")) {
            int b = builtin_by_name(name);
            if (b >= 0 && b != B_NOP) {
                int n = call_args(c);
                if (sk_builtins[b].argc >= 0 && n != sk_builtins[b].argc) {
                    char msg[80];
                    snprintf(msg, sizeof msg, "%s takes %d argument%s, not %d",
                             name, sk_builtins[b].argc, sk_builtins[b].argc == 1 ? "" : "s", n);
                    sk_fail(c, line, msg);
                }
                emit(c, OP_BUILTIN, b, 0);
                return;
            }
            int f = func_index(c, name);
            if (f < 0) {
                char msg[64];
                snprintf(msg, sizeof msg, "'%s' is not defined", name);
                sk_fail(c, line, msg);
                (void)call_args(c);
                emit(c, OP_CONST, 0, 0);
                return;
            }
            int n = call_args(c);
            if (n != c->s->funcs[f].nparams) {
                char msg[80];
                snprintf(msg, sizeof msg, "%s takes %d argument%s, not %d", name,
                         c->s->funcs[f].nparams, c->s->funcs[f].nparams == 1 ? "" : "s", n);
                sk_fail(c, line, msg);
            }
            emit(c, OP_CALL, f, 0);
            return;
        }

        /* postfix ++/--: the value before the change is what the expression is worth */
        if (is_punct(c, "++") || is_punct(c, "--")) {
            bool inc = is_punct(c, "++");
            c->p++;
            emit_load(c, name, line);
            emit(c, OP_DUP, 0, 0);
            emit(c, OP_CONST, 0, 1);
            emit(c, inc ? OP_ADD : OP_SUB, 0, 0);
            emit_store(c, name, line);
            return;
        }

        emit_load(c, name, line);
        return;
    }
    if (t->kind == TK_STR) {
        sk_fail(c, t->line, "text is only understood inside Serial.print");
        c->p++;
        emit(c, OP_CONST, 0, 0);
        return;
    }
    {
        char m[64];
        snprintf(m, sizeof m, "unexpected '%s'", t->kind == TK_EOF ? "end of sketch" : t->text);
        sk_fail(c, t->line, m);
        if (t->kind != TK_EOF) c->p++;
        emit(c, OP_CONST, 0, 0);
    }
}

static void mul_expr(Comp *c) {
    primary(c);
    for (;;) {
        if (accept_punct(c, "*"))      { primary(c); emit(c, OP_MUL, 0, 0); }
        else if (accept_punct(c, "/")) { primary(c); emit(c, OP_DIV, 0, 0); }
        else if (accept_punct(c, "%")) { primary(c); emit(c, OP_MOD, 0, 0); }
        else break;
    }
}
static void add_expr(Comp *c) {
    mul_expr(c);
    for (;;) {
        if (accept_punct(c, "+"))      { mul_expr(c); emit(c, OP_ADD, 0, 0); }
        else if (accept_punct(c, "-")) { mul_expr(c); emit(c, OP_SUB, 0, 0); }
        else break;
    }
}
static void rel_expr(Comp *c) {
    add_expr(c);
    for (;;) {
        if (accept_punct(c, "<"))       { add_expr(c); emit(c, OP_LT, 0, 0); }
        else if (accept_punct(c, "<=")) { add_expr(c); emit(c, OP_LE, 0, 0); }
        else if (accept_punct(c, ">"))  { add_expr(c); emit(c, OP_GT, 0, 0); }
        else if (accept_punct(c, ">=")) { add_expr(c); emit(c, OP_GE, 0, 0); }
        else break;
    }
}
static void eq_expr(Comp *c) {
    rel_expr(c);
    for (;;) {
        if (accept_punct(c, "=="))      { rel_expr(c); emit(c, OP_EQ, 0, 0); }
        else if (accept_punct(c, "!=")) { rel_expr(c); emit(c, OP_NE, 0, 0); }
        else break;
    }
}
/* && and || short-circuit, because `if (i < n && a[i])` is the shape people write and the
   right-hand side of one of these can call digitalRead. */
static void and_expr(Comp *c) {
    eq_expr(c);
    while (accept_punct(c, "&&")) {
        int j = emit(c, OP_JZ_KEEP, 0, 0);
        emit(c, OP_POP, 0, 0);
        eq_expr(c);
        patch(c, j, here(c));
    }
}
static void or_expr(Comp *c) {
    and_expr(c);
    while (accept_punct(c, "||")) {
        int j = emit(c, OP_JNZ_KEEP, 0, 0);
        emit(c, OP_POP, 0, 0);
        and_expr(c);
        patch(c, j, here(c));
    }
}

static void expr(Comp *c) {
    /* assignment, right associative: look ahead for `name =` before committing to a value */
    if (cur(c)->kind == TK_ID && c->p + 1 < c->ntok) {
        Token *nx = &c->tok[c->p + 1];
        if (nx->kind == TK_PUNCT) {
            const char *o = nx->text;
            bool plain = (strcmp(o, "=") == 0);
            bool compound = (!strcmp(o, "+=") || !strcmp(o, "-=") ||
                             !strcmp(o, "*=") || !strcmp(o, "/="));
            if (plain || compound) {
                char name[SK_NAME];
                snprintf(name, sizeof name, "%s", cur(c)->text);
                int line = cur(c)->line;
                c->p += 2;
                if (compound) emit_load(c, name, line);
                expr(c);
                if (compound) {
                    emit(c, o[0] == '+' ? OP_ADD : o[0] == '-' ? OP_SUB :
                            o[0] == '*' ? OP_MUL : OP_DIV, 0, 0);
                }
                emit(c, OP_DUP, 0, 0);          /* an assignment is worth what it stored */
                emit_store(c, name, line);
                return;
            }
        }
    }
    or_expr(c);
}

/* ---------------------------------------------------------------- statements */

static void statement(Comp *c);

static void declaration(Comp *c, bool global) {
    /* the type words have already been consumed by the caller */
    do {
        Token *nt = cur(c);
        if (nt->kind != TK_ID) { sk_fail(c, nt->line, "expected a variable name"); return; }
        char name[SK_NAME];
        snprintf(name, sizeof name, "%s", nt->text);
        int line = nt->line;
        c->p++;
        /* arrays are not supported, and saying so beats a confusing error further down */
        if (is_punct(c, "[")) {
            sk_fail(c, line, "arrays are not supported");
            return;
        }
        int slot = global ? global_slot(c, name, true) : local_slot(c, name, true);
        if (accept_punct(c, "=")) {
            expr(c);
            emit(c, global ? OP_STOREG : OP_STOREL, slot, 0);
        } else {
            emit(c, OP_CONST, 0, 0);
            emit(c, global ? OP_STOREG : OP_STOREL, slot, 0);
        }
    } while (accept_punct(c, ","));
    expect_punct(c, ";");
}

static bool starts_declaration(Comp *c) {
    return cur(c)->kind == TK_ID && is_type_word(cur(c)->text);
}

static void skip_type_words(Comp *c) {
    while (starts_declaration(c)) c->p++;
}

static void block(Comp *c) {
    expect_punct(c, "{");
    while (!is_punct(c, "}") && cur(c)->kind != TK_EOF && !c->failed) statement(c);
    expect_punct(c, "}");
}

static void statement(Comp *c) {
    if (c->failed) return;
    if (is_punct(c, "{")) { block(c); return; }
    if (accept_punct(c, ";")) return;

    if (is_id(c, "if")) {
        c->p++;
        expect_punct(c, "(");
        expr(c);
        expect_punct(c, ")");
        int jz = emit(c, OP_JZ, 0, 0);
        statement(c);
        if (accept_id(c, "else")) {
            int jmp = emit(c, OP_JMP, 0, 0);
            patch(c, jz, here(c));
            statement(c);
            patch(c, jmp, here(c));
        } else {
            patch(c, jz, here(c));
        }
        return;
    }
    if (is_id(c, "while")) {
        c->p++;
        int top = here(c);
        expect_punct(c, "(");
        expr(c);
        expect_punct(c, ")");
        int jz = emit(c, OP_JZ, 0, 0);
        int sb = c->nbreak, sc = c->ncont;
        c->loop_depth++;
        statement(c);
        c->loop_depth--;
        for (int i = sc; i < c->ncont; i++) patch(c, c->cont_patch[i], top);
        c->ncont = sc;
        emit(c, OP_JMP, top, 0);
        patch(c, jz, here(c));
        for (int i = sb; i < c->nbreak; i++) patch(c, c->break_patch[i], here(c));
        c->nbreak = sb;
        return;
    }
    if (is_id(c, "for")) {
        c->p++;
        expect_punct(c, "(");
        /* init */
        if (!is_punct(c, ";")) {
            if (starts_declaration(c)) { skip_type_words(c); declaration(c, false); }
            else { expr(c); emit(c, OP_POP, 0, 0); expect_punct(c, ";"); }
        } else c->p++;
        /* condition */
        int top = here(c);
        int jz = -1;
        if (!is_punct(c, ";")) { expr(c); jz = emit(c, OP_JZ, 0, 0); }
        expect_punct(c, ";");
        /* The post expression is compiled here but has to RUN after the body, so it is jumped
           over on the way in and jumped back to at the end. Nothing is moved or re-parsed. */
        int jbody = emit(c, OP_JMP, 0, 0);
        int post = here(c);
        if (!is_punct(c, ")")) { expr(c); emit(c, OP_POP, 0, 0); }
        emit(c, OP_JMP, top, 0);
        expect_punct(c, ")");
        patch(c, jbody, here(c));
        int sb = c->nbreak, sc = c->ncont;
        c->loop_depth++;
        statement(c);
        c->loop_depth--;
        for (int i = sc; i < c->ncont; i++) patch(c, c->cont_patch[i], post);
        c->ncont = sc;
        emit(c, OP_JMP, post, 0);
        if (jz >= 0) patch(c, jz, here(c));
        for (int i = sb; i < c->nbreak; i++) patch(c, c->break_patch[i], here(c));
        c->nbreak = sb;
        return;
    }
    if (is_id(c, "do")) {
        c->p++;
        int top = here(c);
        int sb = c->nbreak, sc = c->ncont;
        c->loop_depth++;
        statement(c);
        c->loop_depth--;
        int cont_here = here(c);
        for (int i = sc; i < c->ncont; i++) patch(c, c->cont_patch[i], cont_here);
        c->ncont = sc;
        if (!accept_id(c, "while")) sk_fail(c, cur(c)->line, "expected 'while' after 'do'");
        expect_punct(c, "(");
        expr(c);
        expect_punct(c, ")");
        expect_punct(c, ";");
        emit(c, OP_NOT, 0, 0);
        emit(c, OP_JZ, top, 0);
        for (int i = sb; i < c->nbreak; i++) patch(c, c->break_patch[i], here(c));
        c->nbreak = sb;
        return;
    }
    if (is_id(c, "break")) {
        int line = cur(c)->line;
        c->p++; expect_punct(c, ";");
        if (c->loop_depth <= 0) { sk_fail(c, line, "'break' outside a loop"); return; }
        if (c->nbreak < 16) c->break_patch[c->nbreak++] = emit(c, OP_JMP, 0, 0);
        return;
    }
    if (is_id(c, "continue")) {
        int line = cur(c)->line;
        c->p++; expect_punct(c, ";");
        if (c->loop_depth <= 0) { sk_fail(c, line, "'continue' outside a loop"); return; }
        if (c->ncont < 16) c->cont_patch[c->ncont++] = emit(c, OP_JMP, 0, 0);
        return;
    }
    if (is_id(c, "return")) {
        c->p++;
        if (!is_punct(c, ";")) expr(c);
        else emit(c, OP_CONST, 0, 0);
        expect_punct(c, ";");
        emit(c, OP_RET, 0, 0);
        return;
    }
    if (starts_declaration(c)) {
        skip_type_words(c);
        declaration(c, false);
        return;
    }
    expr(c);
    emit(c, OP_POP, 0, 0);
    expect_punct(c, ";");
}

/* ---------------------------------------------------------------- top level */

/* Find every function before compiling any of them, so a sketch can call something defined
   further down - which is how most sketches are written. */
static void scan_functions(Comp *c) {
    int save = c->p;
    while (c->tok[c->p].kind != TK_EOF) {
        if (c->tok[c->p].kind == TK_ID && is_type_word(c->tok[c->p].text)) {
            int q = c->p;
            while (c->tok[q].kind == TK_ID && is_type_word(c->tok[q].text)) q++;
            if (c->tok[q].kind == TK_ID && c->tok[q + 1].kind == TK_PUNCT &&
                strcmp(c->tok[q + 1].text, "(") == 0) {
                /* a definition has a body; a prototype ends in a semicolon */
                int r = q + 2, depth = 1, nparams = 0, seen_name = 0;
                while (c->tok[r].kind != TK_EOF && depth > 0) {
                    if (c->tok[r].kind == TK_PUNCT) {
                        if (!strcmp(c->tok[r].text, "(")) depth++;
                        else if (!strcmp(c->tok[r].text, ")")) depth--;
                        else if (!strcmp(c->tok[r].text, ",") && depth == 1) { nparams++; seen_name = 0; }
                    } else if (depth == 1 && c->tok[r].kind == TK_ID && !seen_name &&
                               !is_type_word(c->tok[r].text)) {
                        seen_name = 1;
                    }
                    r++;
                }
                if (seen_name) nparams++;
                if (c->tok[r].kind == TK_PUNCT && !strcmp(c->tok[r].text, "{")) {
                    if (func_index(c, c->tok[q].text) < 0) {
                        if (c->s->nfuncs >= SK_MAX_FUNCS) {
                            sk_fail(c, c->tok[q].line, "too many functions");
                            c->p = save;
                            return;
                        }
                        SkFunc *f = &c->s->funcs[c->s->nfuncs++];
                        snprintf(f->name, SK_NAME, "%s", c->tok[q].text);
                        f->nparams = nparams;
                        f->entry = -1;
                        f->nlocals = 0;
                        f->defined = false;
                    }
                }
            }
        }
        c->p++;
    }
    c->p = save;
}

static void function_body(Comp *c, int fidx) {
    SkFunc *f = &c->s->funcs[fidx];
    c->nlocals = 0;
    c->cur_func = fidx;
    c->nbreak = c->ncont = c->loop_depth = 0;

    /* parameters become the first locals, in order */
    expect_punct(c, "(");
    if (!is_punct(c, ")")) {
        do {
            skip_type_words(c);
            if (cur(c)->kind == TK_ID) { local_slot(c, cur(c)->text, true); c->p++; }
            else if (!is_punct(c, ")")) { sk_fail(c, cur(c)->line, "expected a parameter name"); break; }
        } while (accept_punct(c, ","));
    }
    expect_punct(c, ")");

    f->entry = here(c);
    f->defined = true;
    block(c);
    /* falling off the end returns zero, the same as a function with no return in it */
    emit(c, OP_CONST, 0, 0);
    emit(c, OP_RET, 0, 0);
    f->nlocals = c->nlocals;
}

/* Consume a balanced run of brackets starting at the opening one. */
static void skip_balanced(Comp *c, const char *open, const char *close) {
    int depth = 0;
    while (c->tok[c->p].kind != TK_EOF) {
        if (c->tok[c->p].kind == TK_PUNCT) {
            if (!strcmp(c->tok[c->p].text, open)) depth++;
            else if (!strcmp(c->tok[c->p].text, close)) {
                depth--;
                if (depth == 0) { c->p++; return; }
            }
        }
        c->p++;
    }
}

/* One walk over the top level. compile_funcs true emits the function bodies and gives every
   global its slot without emitting anything for it; false does the reverse, so the initialisers
   come out as one contiguous run. Both walks see the source in the same order. */
static void top_level_pass(Comp *c, bool compile_funcs) {
    c->p = 0;
    while (c->tok[c->p].kind != TK_EOF && !c->failed) {
        if (accept_punct(c, ";")) continue;
        if (!starts_declaration(c)) {
            char m[64];
            snprintf(m, sizeof m, "unexpected '%s' outside a function", c->tok[c->p].text);
            sk_fail(c, c->tok[c->p].line, m);
            return;
        }
        int q = c->p;
        while (c->tok[q].kind == TK_ID && is_type_word(c->tok[q].text)) q++;
        bool is_func = (c->tok[q].kind == TK_ID && c->tok[q + 1].kind == TK_PUNCT &&
                        strcmp(c->tok[q + 1].text, "(") == 0);
        if (is_func) {
            char fname[SK_NAME];
            snprintf(fname, sizeof fname, "%s", c->tok[q].text);
            int line = c->tok[q].line;
            c->p = q + 1;                          /* at the '(' */
            int save = c->p;
            skip_balanced(c, "(", ")");
            bool prototype = is_punct(c, ";");
            c->p = save;
            if (prototype) {
                skip_balanced(c, "(", ")");
                accept_punct(c, ";");
                continue;
            }
            if (compile_funcs) {
                int fidx = func_index(c, fname);
                if (fidx < 0) { sk_fail(c, line, "internal: function was not registered"); return; }
                if (c->s->funcs[fidx].defined) {
                    char m[64];
                    snprintf(m, sizeof m, "'%s' is defined twice", fname);
                    sk_fail(c, line, m);
                    return;
                }
                function_body(c, fidx);
            } else {
                skip_balanced(c, "(", ")");
                skip_balanced(c, "{", "}");
            }
        } else {
            skip_type_words(c);
            c->nlocals = 0;
            c->emit_off = compile_funcs;
            declaration(c, true);
            c->emit_off = false;
        }
    }
}

Sketch *sketch_compile(const char *src, char *err, size_t err_size) {
    if (err && err_size) err[0] = 0;
    if (!src) { if (err) snprintf(err, err_size, "no sketch"); return NULL; }
    if (strlen(src) >= SKETCH_MAX_SRC) {
        if (err) snprintf(err, err_size, "sketch is larger than %d characters", SKETCH_MAX_SRC);
        return NULL;
    }

    Token *toks = (Token *)calloc(SK_MAX_TOKENS, sizeof(Token));
    Sketch *s = (Sketch *)calloc(1, sizeof(Sketch));
    if (!toks || !s) { free(toks); free(s); if (err) snprintf(err, err_size, "out of memory"); return NULL; }

    int n = sk_lex(src, toks, SK_MAX_TOKENS - 2, err, err_size);
    if (n < 0) { free(toks); free(s); return NULL; }

    Comp c;
    memset(&c, 0, sizeof c);
    c.tok = toks; c.ntok = n; c.p = 0; c.s = s; c.err = err; c.err_size = err_size;
    s->fn_setup = s->fn_loop = -1;
    s->vcc = 5.0;

    /* Global initialisers run before setup(), so they are compiled into a preamble the driver
       enters first. Slot 0 of the code is that preamble. */
    int preamble = here(&c);
    scan_functions(&c);

    top_level_pass(&c, true);           /* function bodies; globals get their slots */
    if (!c.failed) {
        s->init_entry = here(&c);
        top_level_pass(&c, false);      /* and now the global initialisers, in source order */
    }

    if (!c.failed) {
        emit(&c, OP_HALT, 0, 0);
        s->fn_setup = func_index(&c, "setup");
        s->fn_loop = func_index(&c, "loop");
        if (s->fn_loop < 0 && s->fn_setup < 0)
            sk_fail(&c, 1, "a sketch needs a loop() or a setup()");
        for (int i = 0; i < s->nfuncs; i++) {
            if (!s->funcs[i].defined) {
                char m[80];
                snprintf(m, sizeof m, "'%s' is declared but never defined", s->funcs[i].name);
                sk_fail(&c, 1, m);
                break;
            }
        }
    }
    (void)preamble;

    free(toks);
    if (c.failed) { free(s); return NULL; }
    sketch_reset(s);
    return s;
}

void sketch_free(Sketch *s) { free(s); }

/* ---------------------------------------------------------------- run */

void sketch_reset(Sketch *s) {
    if (!s) return;
    for (int i = 0; i < SK_MAX_GLOBALS; i++) s->globals[i] = 0;
    s->sp = 0; s->fp = 0; s->pc = 0;
    s->state = SK_INIT;
    s->t = 0; s->wake = 0; s->waiting = false; s->loops = 0;
    s->faulted = false; s->err[0] = 0; s->last_print[0] = 0; s->shortest_delay = 0;
    for (int i = 0; i < SKETCH_MAX_PINS; i++) {
        s->pin_mode[i] = SKETCH_PIN_INPUT;
        s->pin_duty[i] = 0;
        s->pin_pwm[i] = false;
        s->pin_volts[i] = 0;
    }
}

void sketch_set_pin_voltage(Sketch *s, int pin, double v) {
    if (s && pin >= 0 && pin < SKETCH_MAX_PINS) s->pin_volts[pin] = v;
}
void sketch_set_vcc(Sketch *s, double vcc) { if (s && vcc > 0) s->vcc = vcc; }

bool sketch_pin_is_output(const Sketch *s, int pin) {
    return s && pin >= 0 && pin < SKETCH_MAX_PINS && s->pin_mode[pin] == SKETCH_PIN_OUTPUT;
}
double sketch_pin_duty(const Sketch *s, int pin) {
    return (s && pin >= 0 && pin < SKETCH_MAX_PINS) ? s->pin_duty[pin] : 0.0;
}
SketchPinMode sketch_pin_mode(const Sketch *s, int pin) {
    return (s && pin >= 0 && pin < SKETCH_MAX_PINS) ? (SketchPinMode)s->pin_mode[pin]
                                                    : SKETCH_PIN_INPUT;
}
bool sketch_pin_is_pwm(const Sketch *s, int pin) {
    return s && pin >= 0 && pin < SKETCH_MAX_PINS && s->pin_pwm[pin];
}
const char *sketch_last_print(const Sketch *s) {
    return (s && s->last_print[0]) ? s->last_print : NULL;
}
const char *sketch_error(const Sketch *s) { return (s && s->faulted) ? s->err : NULL; }
double sketch_millis(const Sketch *s) { return s ? s->t * 1000.0 : 0.0; }
double sketch_shortest_delay(const Sketch *s) { return s ? s->shortest_delay : 0.0; }
long sketch_loop_count(const Sketch *s) { return s ? s->loops : 0; }
bool sketch_is_waiting(const Sketch *s) { return s && s->waiting; }

static void sk_fault(Sketch *s, const char *msg) {
    if (s->faulted) return;
    s->faulted = true;
    s->state = SK_FAULT;
    snprintf(s->err, sizeof s->err, "%s", msg);
}

static void push(Sketch *s, double v) {
    if (s->sp >= SK_MAX_STACK) { sk_fault(s, "expression nested too deeply"); return; }
    s->stack[s->sp++] = v;
}
static double pop(Sketch *s) {
    if (s->sp <= 0) { sk_fault(s, "internal: stack underflow"); return 0; }
    return s->stack[--s->sp];
}

/* Enter a function with its arguments already on the stack. */
static void enter(Sketch *s, int fidx, int ret_pc) {
    SkFunc *f = &s->funcs[fidx];
    if (s->fp >= SK_MAX_FRAMES) { sk_fault(s, "functions nested too deeply (is it recursive?)"); return; }
    int base = 0;
    for (int i = 0; i < s->fp; i++) base += s->funcs[s->frames[i].func].nlocals;
    if (base + f->nlocals > SK_MAX_LOCALS) { sk_fault(s, "too many local variables at once"); return; }
    for (int i = f->nparams - 1; i >= 0; i--) s->locals[base + i] = pop(s);
    for (int i = f->nparams; i < f->nlocals; i++) s->locals[base + i] = 0;
    s->frames[s->fp].ret_pc = ret_pc;
    s->frames[s->fp].base = base;
    s->frames[s->fp].func = fidx;
    s->fp++;
    s->pc = f->entry;
}

static int pin_of(Sketch *s, double v) {
    int p = (int)llround(v);
    if (p < 0 || p >= SKETCH_MAX_PINS) {
        char m[64];
        snprintf(m, sizeof m, "pin %d does not exist", p);
        sk_fault(s, m);
        return -1;
    }
    return p;
}

static void do_builtin(Sketch *s, int b) {
    switch (b) {
        case B_PINMODE: {
            double mode = pop(s);
            int p = pin_of(s, pop(s));
            if (p >= 0) {
                int m = (int)mode;
                s->pin_mode[p] = (m == SKETCH_PIN_OUTPUT) ? SKETCH_PIN_OUTPUT
                               : (m == SKETCH_PIN_INPUT_PULLUP) ? SKETCH_PIN_INPUT_PULLUP
                               : SKETCH_PIN_INPUT;
                if (s->pin_mode[p] != SKETCH_PIN_OUTPUT) { s->pin_duty[p] = 0; s->pin_pwm[p] = false; }
            }
            push(s, 0);
            break;
        }
        case B_DIGITALWRITE: {
            double val = pop(s);
            int p = pin_of(s, pop(s));
            if (p >= 0) {
                s->pin_duty[p] = (val != 0) ? 1.0 : 0.0;
                s->pin_pwm[p] = false;
                /* Writing HIGH to a pin still in INPUT mode switches the pull-up on, on real
                   hardware. Here it would silently drive the node hard, which is worse than
                   doing nothing, so it only sets the pull-up. */
                if (s->pin_mode[p] == SKETCH_PIN_INPUT && val != 0)
                    s->pin_mode[p] = SKETCH_PIN_INPUT_PULLUP;
            }
            push(s, 0);
            break;
        }
        case B_DIGITALREAD: {
            int p = pin_of(s, pop(s));
            /* The threshold a 5 V part uses. Below 0.3*Vcc is low, above 0.6*Vcc is high, and
               in between it reads as whatever is nearer - a real input has hysteresis and a
               forbidden band, and a simulation that pretends otherwise is more misleading than
               one that picks a side. */
            push(s, (p >= 0 && s->pin_volts[p] > 0.5 * s->vcc) ? 1.0 : 0.0);
            break;
        }
        case B_ANALOGWRITE: {
            double val = pop(s);
            int p = pin_of(s, pop(s));
            if (p >= 0) {
                double duty = val / 255.0;
                if (duty < 0) duty = 0;
                if (duty > 1) duty = 1;
                s->pin_duty[p] = duty;
                s->pin_mode[p] = SKETCH_PIN_OUTPUT;
                /* 0 and 255 are steady levels on real hardware, not one-sided pulses */
                s->pin_pwm[p] = (duty > 0.0 && duty < 1.0);
            }
            push(s, 0);
            break;
        }
        case B_ANALOGREAD: {
            int p = pin_of(s, pop(s));
            double v = (p >= 0) ? s->pin_volts[p] : 0;
            double counts = (v / s->vcc) * 1023.0;
            if (counts < 0) counts = 0;
            if (counts > 1023) counts = 1023;
            push(s, floor(counts + 0.5));
            break;
        }
        case B_DELAY: {
            double ms = pop(s);
            if (ms > 0) {
                s->wake = s->t + ms / 1000.0; s->waiting = true;
                double d = ms / 1000.0;
                if (s->shortest_delay <= 0 || d < s->shortest_delay) s->shortest_delay = d;
            }
            push(s, 0);
            break;
        }
        case B_DELAYUS: {
            double us = pop(s);
            if (us > 0) {
                s->wake = s->t + us / 1e6; s->waiting = true;
                double d = us / 1e6;
                if (s->shortest_delay <= 0 || d < s->shortest_delay) s->shortest_delay = d;
            }
            push(s, 0);
            break;
        }
        case B_MILLIS:  push(s, floor(s->t * 1000.0)); break;
        case B_MICROS:  push(s, floor(s->t * 1e6)); break;
        case B_PRINT:
        case B_PRINTLN: {
            double v = pop(s);
            if (v == floor(v) && fabs(v) < 1e15) snprintf(s->last_print, sizeof s->last_print, "%lld", (long long)v);
            else snprintf(s->last_print, sizeof s->last_print, "%g", v);
            push(s, 0);
            break;
        }
        case B_MAP: {
            double oh = pop(s), ol = pop(s), ih = pop(s), il = pop(s), x = pop(s);
            push(s, (ih == il) ? ol : (x - il) * (oh - ol) / (ih - il) + ol);
            break;
        }
        case B_CONSTRAIN: {
            double hi = pop(s), lo = pop(s), x = pop(s);
            push(s, x < lo ? lo : (x > hi ? hi : x));
            break;
        }
        case B_MIN: { double b2 = pop(s), a2 = pop(s); push(s, a2 < b2 ? a2 : b2); break; }
        case B_MAX: { double b2 = pop(s), a2 = pop(s); push(s, a2 > b2 ? a2 : b2); break; }
        case B_ABS: push(s, fabs(pop(s))); break;
        case B_SQRT: { double x = pop(s); push(s, x > 0 ? sqrt(x) : 0); break; }
        case B_POW:  { double e = pop(s), b2 = pop(s); push(s, pow(b2, e)); break; }
        default: push(s, 0); break;
    }
}

void sketch_advance(Sketch *s, double t) {
    if (!s || s->state == SK_FAULT) return;
    s->t = t;

    if (s->state == SK_INIT) {
        /* the preamble - global initialisers - then setup(), then loop() forever */
        s->pc = s->init_entry;
        s->sp = 0; s->fp = 0;
        s->state = SK_SETUP;
    }
    if (s->waiting) {
        if (s->t < s->wake) return;
        s->waiting = false;
    }

    int budget = SK_BUDGET;
    while (budget-- > 0 && !s->faulted) {
        if (s->pc < 0 || s->pc >= s->ncode) { sk_fault(s, "internal: ran off the end of the code"); return; }
        Instr *i = &s->code[s->pc++];
        switch ((Op)i->op) {
            case OP_CONST:  push(s, i->d); break;
            case OP_LOADG:  push(s, s->globals[i->a]); break;
            case OP_STOREG: s->globals[i->a] = pop(s); break;
            case OP_LOADL: {
                int base = s->fp > 0 ? s->frames[s->fp - 1].base : 0;
                push(s, s->locals[base + i->a]);
                break;
            }
            case OP_STOREL: {
                int base = s->fp > 0 ? s->frames[s->fp - 1].base : 0;
                s->locals[base + i->a] = pop(s);
                break;
            }
            case OP_ADD: { double b = pop(s), a = pop(s); push(s, a + b); break; }
            case OP_SUB: { double b = pop(s), a = pop(s); push(s, a - b); break; }
            case OP_MUL: { double b = pop(s), a = pop(s); push(s, a * b); break; }
            case OP_DIV: {
                double b = pop(s), a = pop(s);
                if (b == 0) { sk_fault(s, "divide by zero"); return; }
                push(s, a / b);
                break;
            }
            case OP_MOD: {
                double b = pop(s), a = pop(s);
                if ((long long)b == 0) { sk_fault(s, "modulo by zero"); return; }
                push(s, (double)((long long)a % (long long)b));
                break;
            }
            case OP_NEG: push(s, -pop(s)); break;
            case OP_NOT: push(s, pop(s) == 0 ? 1 : 0); break;
            case OP_EQ: { double b = pop(s), a = pop(s); push(s, a == b); break; }
            case OP_NE: { double b = pop(s), a = pop(s); push(s, a != b); break; }
            case OP_LT: { double b = pop(s), a = pop(s); push(s, a <  b); break; }
            case OP_LE: { double b = pop(s), a = pop(s); push(s, a <= b); break; }
            case OP_GT: { double b = pop(s), a = pop(s); push(s, a >  b); break; }
            case OP_GE: { double b = pop(s), a = pop(s); push(s, a >= b); break; }
            case OP_JMP: s->pc = i->a; break;
            case OP_JZ:  if (pop(s) == 0) s->pc = i->a; break;
            case OP_JZ_KEEP:  if (s->sp > 0 && s->stack[s->sp - 1] == 0) s->pc = i->a; break;
            case OP_JNZ_KEEP: if (s->sp > 0 && s->stack[s->sp - 1] != 0) s->pc = i->a; break;
            case OP_POP: pop(s); break;
            case OP_DUP: { double v = s->sp > 0 ? s->stack[s->sp - 1] : 0; push(s, v); break; }
            case OP_CALL: enter(s, i->a, s->pc); break;
            case OP_RET: {
                double rv = pop(s);
                if (s->fp <= 0) { sk_fault(s, "internal: return with no frame"); return; }
                s->fp--;
                int ret_pc = s->frames[s->fp].ret_pc;
                if (ret_pc < 0) {
                    /* the driver's own frame came back: move to the next phase */
                    if (s->state == SK_SETUP) {
                        if (s->fn_loop < 0) { s->state = SK_DONE; return; }
                        s->state = SK_LOOP;
                        s->sp = 0;
                        enter(s, s->fn_loop, -1);
                    } else {
                        s->loops++;
                        s->sp = 0;
                        enter(s, s->fn_loop, -1);
                        /* One pass of loop() per call. Without this a sketch whose loop has no
                           delay in it runs its whole budget every solver step and time never
                           moves for it, which is the wrong end of the trade: the circuit is
                           what the user is watching. */
                        return;
                    }
                } else {
                    s->pc = ret_pc;
                    push(s, rv);
                }
                break;
            }
            case OP_BUILTIN:
                do_builtin(s, i->a);
                if (s->waiting) return;
                break;
            case OP_PRINTS:
                snprintf(s->last_print, sizeof s->last_print, "%s",
                         (i->a >= 0 && i->a < s->nstrings) ? s->strings[i->a] : "");
                push(s, 0);
                break;
            case OP_HALT:
                /* end of the preamble: global initialisers are done, so setup() runs */
                if (s->state == SK_SETUP) {
                    s->sp = 0; s->fp = 0;
                    if (s->fn_setup >= 0) { enter(s, s->fn_setup, -1); break; }
                    if (s->fn_loop >= 0) { s->state = SK_LOOP; enter(s, s->fn_loop, -1); break; }
                }
                s->state = SK_DONE;
                return;
            default:
                sk_fault(s, "internal: bad instruction");
                return;
        }
    }
}
