#include <stdio.h>
#include <stdarg.h>
#include <stdbool.h>
#include <errno.h>
#include "../mpc/mpc.h"

static int asprintf(char** sptr, char* fmt, ...) {
  va_list va;
  va_start(va, fmt);
  int wanted = vsnprintf(*sptr = NULL, 0, fmt, va);
  if((wanted > 0) && ((*sptr = malloc(1 + wanted)) != NULL)) {
    va_end(va);
    return vsprintf(*sptr, fmt, va);
  }
  
  va_end(va);
  return wanted;
} 

static mpc_parser_t* Number;
static mpc_parser_t* Symbol; 
static mpc_parser_t* Comment; 
static mpc_parser_t* String; 
static mpc_parser_t* Sexpr;
static mpc_parser_t* Qexpr;
static mpc_parser_t* Expr; 
static mpc_parser_t* Lispy;

enum { LVAL_ERR = 0, LVAL_NUM = 1, LVAL_SYM = 2, LVAL_STR = 3, LVAL_LAMBDA = 4, LVAL_SEXPR = 5, LVAL_QEXPR = 6 };

char* lval_type_names[] = {
  "error", "number", "symbol", "string", 
  "function", "expression", "list"
};

struct lval;
struct lenv;
typedef struct lval lval;
typedef struct lenv lenv;

typedef lval*(*lbuiltin)(lenv* e, lval* v);

struct lval {
  /* Type */
  int type;

  /* Basic */
  long num;
  char* err;
  char* sym;
  char* str;
  
  /* Expr */
  int count;
  lval** cell;
  
  /* Function */
  lbuiltin builtin;
  lenv* env;
  lval* formals;
  lval* body;
};

struct lenv {
  lenv* par;
  int count;
  char** syms;
  lval** vals;
};

char* strnew(char* x) {
  char* y = malloc(strlen(x) + 1);
  strcpy(y, x);
  return y;
}

char* strput(char* x, char* y) {
  x = realloc(x, strlen(y) + 1);
  strcpy(x, y);
  return x;
}

lval* lval_num(long x) {
  lval* v = malloc(sizeof(lval));
  v->type = LVAL_NUM;
  v->num = x;
  return v;
}

lval* lval_err(char* fmt, ...) {
  lval* v = malloc(sizeof(lval));
  v->type = LVAL_ERR;
  va_list va;
  va_start(va, fmt);
  v->err = malloc(512);
  vsnprintf(v->err, 511, fmt, va);
  v->err = realloc(v->err, strlen(v->err)+1);
  va_end(va);
  return v;
}

lval* lval_sexpr(void) {
  lval* v = malloc(sizeof(lval));
  v->type = LVAL_SEXPR;
  v->count = 0;
  v->cell = NULL;
  return v;
}

lval* lval_qexpr(void) {
  lval* v = malloc(sizeof(lval));
  v->type = LVAL_QEXPR;
  v->count = 0;
  v->cell = NULL;
  return v;
}

lval* lval_sym(char* s) {
  lval* v = malloc(sizeof(lval));
  v->type = LVAL_SYM;
  v->sym = strnew(s);
  return v;
}

lval* lval_str(char* s) {
  lval* v = malloc(sizeof(lval));
  v->type = LVAL_STR;
  v->str = strnew(s);
  return v;
}

lenv* lenv_new(lenv* par);
lenv* lenv_copy(lenv* e);
void lenv_del(lenv* e);

lval* lval_builtin(lbuiltin b) {
  lval* v = malloc(sizeof(lval));
  v->type = LVAL_LAMBDA;
  v->builtin = b;
  v->env = lenv_new(NULL);
  v->formals = lval_qexpr();
  v->body = lval_qexpr();
  return v;
}

lval* lval_lambda(lval* formals, lval* body) {
  lval* v = malloc(sizeof(lval));
  v->type = LVAL_LAMBDA;
  v->builtin = NULL;
  v->env = lenv_new(NULL);
  v->formals = formals;
  v->body = body;
  return v;  
}

lval* lval_copy(lval* v) {
  lval* x = malloc(sizeof(lval));
  x->type = v->type;
  switch (v->type) {
    case LVAL_ERR: x->err = strnew(v->err); break;
    case LVAL_NUM: x->num = v->num; break;
    case LVAL_SYM: x->sym = strnew(v->sym); break;
    case LVAL_STR: x->str = strnew(v->str); break;
    case LVAL_LAMBDA:
      x->builtin = v->builtin;
      x->env = lenv_copy(v->env);
      x->formals = lval_copy(v->formals);
      x->body = lval_copy(v->body);
    break;
    case LVAL_SEXPR:
    case LVAL_QEXPR:
      x->count = v->count;
      x->cell = malloc(sizeof(lval*) * v->count);
      for (int i = 0; i < v->count; i++) {
        x->cell[i] = lval_copy(v->cell[i]);
      }
    break;
  }
  return x;
}

void lval_del(lval* v) {

  switch (v->type) {
    case LVAL_ERR: free(v->err); break;
    case LVAL_NUM: break;
    case LVAL_SYM: free(v->sym); break;
    case LVAL_STR: free(v->str); break;
    case LVAL_LAMBDA:
      lenv_del(v->env);
      lval_del(v->formals);
      lval_del(v->body);
    break;
    case LVAL_QEXPR:
    case LVAL_SEXPR:
      for (int i = 0; i < v->count; i++) {
        lval_del(v->cell[i]);
      }
      free(v->cell);
    break;
  }
  
  free(v);
}

bool lval_eq(lval* x, lval* y) {
  if (x->type != y->type) { return false; }
  switch (x->type) {
    case LVAL_ERR: return (strcmp(x->err, y->err) == 0);
    case LVAL_NUM: return (x->num == y->num);
    case LVAL_SYM: return (strcmp(x->sym, y->sym) == 0);
    case LVAL_STR: return (strcmp(x->str, y->str) == 0);
    case LVAL_LAMBDA: if (x->builtin) { return x->builtin == y->builtin; } else { return lval_eq(x->formals, y->formals) && lval_eq(x->body, y->body); }
    case LVAL_QEXPR:
    case LVAL_SEXPR:
      if (x->count != y->count) { return false; }
      for (int i = 0; i < x->count; i++) {
        if (!lval_eq(x->cell[i], y->cell[i])) { return false; }
      }
      return true;
    break;
  }
  return false;
}

lval* lval_add(lval* v, lval* x) {
  v->count++;
  v->cell = realloc(v->cell, sizeof(lval*) * v->count);
  v->cell[v->count-1] = x;
  return v;
}

lval* lval_join(lval* x, lval* y) {
  for (int i = 0; i < y->count; i++) {
    x = lval_add(x, y->cell[i]);
  }
  free(y->cell);
  free(y);
  return x;
}

lval* lval_pop(lval* v, int i) {
  lval* x = v->cell[i];
  memmove(&v->cell[i], &v->cell[i+1], sizeof(lval*) * (v->count-i-1));
  v->count--;
  v->cell = realloc(v->cell, sizeof(lval*) * v->count);
  return x;
}

lval* lval_take(lval* v, int i) {
  lval* x = lval_pop(v, i);
  lval_del(v);
  return x;
}

lval* lval_qhead(lval* v) {
  while (v->count > 1) { lval_del(lval_pop(v, 1)); }
  return v;
}

lval* lval_qtail(lval* v) {
  lval_del(lval_pop(v, 0));
  return v;
}

char* lval_tostr(lenv* e, lval* v);
char* lenv_sym(lenv* e, lval* v);

char* lval_tostr_expr(lenv* e, lval* v, char open, char close) {
  char* s = malloc(1);
  s[0] = open;
  for (int i = 0; i < v->count; i++) {
    char* c = lval_tostr(e, v->cell[i]);
    s = realloc(s, strlen(s) + strlen(c) + 1);
    strcat(s, c);
    if (i != (v->count-1)) {
      s = realloc(s, strlen(s) + 2);
      strcat(s, " ");
    }
  }
  s = realloc(s, strlen(s) + 2);
  s[strlen(s)+1] = '\0';
  s[strlen(s)+0] = close;
  return s;
}

char* lval_tostr(lenv* e, lval* v) {
  char* s;
  char* escaped;
  switch (v->type) {
    case LVAL_NUM: asprintf(&s, "%li", v->num); return s;
    case LVAL_ERR: asprintf(&s, "Error: %s", v->err); return s;
    case LVAL_SYM: asprintf(&s, "%s", v->sym); return s;
    case LVAL_STR:
      escaped = mpcf_escape(strnew(v->str));
      asprintf(&s, "\"%s\"", escaped);
      free(escaped);
      return s;
    case LVAL_LAMBDA:
      if (v->builtin) {
        asprintf(&s, "(builtin '%s')", lenv_sym(e, v));
      } else {
        char* fmls = lval_tostr(e, v->formals);
        char* body = lval_tostr(e, v->body);
        asprintf(&s, "(\\ %s %s)", fmls, body);
        free(fmls); free(body);
      }
    return s;
    case LVAL_SEXPR: return lval_tostr_expr(e, v, '(', ')');
    case LVAL_QEXPR: return lval_tostr_expr(e, v, '{', '}');
    default: return NULL;
  }
}

void lval_print(lenv* e, lval* v) { char* s = lval_tostr(e, v); printf("%s", s); free(s); }
void lval_println(lenv* e, lval* v) { lval_print(e, v); putchar('\n'); }

/*
** Environment
*/

lenv* lenv_new(lenv* par) {
  lenv* e = malloc(sizeof(lenv));
  e->par = par;
  e->count = 0;
  e->syms = NULL;
  e->vals = NULL;
  return e;
}

void lenv_del(lenv* e) {
  for (int i = 0; i < e->count; i++) {
    free(e->syms[i]);
    lval_del(e->vals[i]);
  }
  free(e->syms);
  free(e->vals);
  free(e);
}

lenv* lenv_copy(lenv* e) {
  lenv* n = malloc(sizeof(lenv));
  n->par = e->par;
  n->count = e->count;
  n->syms = malloc(sizeof(char*) * n->count);
  n->vals = malloc(sizeof(lval*) * n->count);
  for (int i = 0; i < e->count; i++) {
    n->syms[i] = strnew(e->syms[i]);
    n->vals[i] = lval_copy(e->vals[i]);
  }
  return n;
}

lval* lenv_get(lenv* e, lval* k) {
  for (int i = 0; i < e->count; i++) {
    if (strcmp(e->syms[i], k->sym) == 0) { return lval_copy(e->vals[i]); }
  }
  if (e->par) {
    return lenv_get(e->par, k);
  } else {
    return lval_err("unbound symbol '%s'", k->sym);
  }
}

void lenv_put(lenv* e, lval* k, lval* v) {
    
  for (int i = 0; i < e->count; i++) {
    if (strcmp(e->syms[i], k->sym) == 0) {
      lval_del(e->vals[i]);
      e->vals[i] = lval_copy(v);
      e->syms[i] = strput(e->syms[i], k->sym);
      return;
    }
  }
  
  e->count++;
  e->vals = realloc(e->vals, sizeof(lval*) * e->count);
  e->syms = realloc(e->syms, sizeof(char*) * e->count);
  e->vals[e->count-1] = lval_copy(v);
  e->syms[e->count-1] = strnew(k->sym);
}

void lenv_def(lenv* e, lval* k, lval* v) {
  while (e->par) { e = e->par; }
  lenv_put(e, k, v);
}

char* lenv_sym(lenv* e, lval* x) {
  for (int i = 0; i < e->count; i++) {
    if (lval_eq(e->vals[i], x)) { return e->syms[i]; }
  }
  if (e->par) {
    return lenv_sym(e->par, x);
  } else {
    return NULL;
  }
}

/*
** Builtins
*/

#define LVAL_CHK_ARGNUM_GE(F, A, N) \
  if (A->count  < N) { \
    lval* err = lval_err("attempt to apply '%s' on too few of arguments (expected at least %i, got %i)", F, N, A->count); \
    lval_del(A); \
    return err; \
  }

#define LVAL_CHK_ARGNUM_EQ(F, A, N) \
  if (A->count != N) { \
    lval* err = lval_err("attempt to apply '%s' on the wrong number of arguments (expected %i, got %i)", F, N, A->count); \
    lval_del(A); \
    return err; \
  }
  
#define LVAL_CHK_TYPE(F, A, N, T) \
  if (A->cell[N]->type != T) { \
    char* str = lval_tostr(e, A->cell[N]); \
    lval* err = lval_err("attempt to apply '%s' on %s (expected %s, got %s)", F, str, lval_type_names[T], lval_type_names[A->cell[N]->type]); \
    free(str); \
    lval_del(A); \
    return err; \
  }
  
#define LVAL_CHK_TYPE_LOOP(F, X, A0, A1, T) \
  if (X->type != T) { \
    char* str = lval_tostr(e, X); \
    lval* err = lval_err("attempt to apply '%s' on %s (expected %s, got %s)", F, str, lval_type_names[T], lval_type_names[X->type]); \
    free(str); \
    lval_del(A0); lval_del(A1); \
    A0 = err; \
    break; \
  }

lval* builtin_eval(lenv* e, lval* a);
lval* lval_eval(lenv* e, lval* v);

lval* builtin_assign(lenv* e, lval* a, char* assign) {
  LVAL_CHK_ARGNUM_GE(assign, a, 2);
  LVAL_CHK_TYPE(assign, a, 0, LVAL_QEXPR);
  
  lval* formals = a->cell[0];
  
  for (int i = 0; i < formals->count; i++) {
    if (formals->cell[i]->type != LVAL_SYM) {
      char* str = lval_tostr(e, formals->cell[i]);
      lval* err = lval_err("attempt to define non-symbol %s (got %s)", str, lval_type_names[formals->cell[i]->type]);
      lval_del(a);
      free(str);
      return err;
    }
  }
  
  if (formals->count != a->count-1) {
    lval_del(a); return lval_err("attempt to define %i values to %i symbols", a->count-1, formals->count);
  }
  
  for (int i = 0; i < formals->count; i++) {
    if (strcmp(assign, "def") == 0) { lenv_def(e, formals->cell[i], a->cell[i+1]); }
    if (strcmp(assign, "=")   == 0) { lenv_put(e, formals->cell[i], a->cell[i+1]); }
  }
  
  lval_del(a);
  return lval_sexpr();
}

lval* builtin_def(lenv* e, lval* a) { return builtin_assign(e, a, "def"); }
lval* builtin_set(lenv* e, lval* a) { return builtin_assign(e, a, "="); }

lval* builtin_lambda(lenv* e, lval* a) {
  LVAL_CHK_ARGNUM_EQ("lambda", a, 2);
  LVAL_CHK_TYPE("lambda", a, 0, LVAL_QEXPR);
  LVAL_CHK_TYPE("lambda", a, 1, LVAL_QEXPR);
    
  for (int i = 0; i < a->cell[0]->count; i++) {
    if (a->cell[0]->cell[i]->type != LVAL_SYM) {
      char* str = lval_tostr(e, a->cell[0]->cell[i]);
      lval* err = lval_err("attempt to define non-symbol %s (got %s)", str, lval_type_names[a->cell[0]->cell[i]->type]);
      lval_del(a);
      free(str);
      return err;
    }
  }
  
  lval* formals = lval_pop(a, 0);
  lval* body = lval_pop(a, 0);
  lval_del(a);
  
  return lval_lambda(formals, body);
}

lval* builtin_head(lenv* e, lval* a) {
  LVAL_CHK_ARGNUM_EQ("head", a, 1);
  LVAL_CHK_TYPE("head", a, 0, LVAL_QEXPR);
  
  if (a->cell[0]->count == 0) {
    lval_del(a);
    return lval_err("attempt to apply 'head' on {}");
  } else {
    return lval_qhead(lval_take(a, 0));
  }
}

lval* builtin_tail(lenv* e, lval* a) {
  LVAL_CHK_ARGNUM_EQ("tail", a, 1);
  LVAL_CHK_TYPE("tail", a, 0, LVAL_QEXPR);
  
  if (a->cell[0]->count == 0) {
    lval_del(a);
    return lval_err("attempt to apply 'tail' on {}");
  } else {
    return lval_qtail(lval_take(a, 0));
  }
}

lval* builtin_eval(lenv* e, lval* a) {
  LVAL_CHK_ARGNUM_EQ("eval", a, 1);
  LVAL_CHK_TYPE("eval", a, 0, LVAL_QEXPR);
  
  lval* x = lval_take(a, 0);
  x->type = LVAL_SEXPR;
  return lval_eval(e, x);
}

lval* builtin_join(lenv* e, lval* a) {
  LVAL_CHK_TYPE("join", a, 0, LVAL_QEXPR);
  lval* x = lval_pop(a, 0);
  
  while (a->count) {
    lval* y = lval_pop(a, 0);
    LVAL_CHK_TYPE_LOOP("join", x, x, y, LVAL_QEXPR);
    LVAL_CHK_TYPE_LOOP("join", y, x, y, LVAL_QEXPR); 
    x = lval_join(x, y);
  }
  
  lval_del(a);
  return x;
}

lval* builtin_list(lenv* e, lval* a) {
  a->type = LVAL_QEXPR;
  return a;
}

lval* builtin_op(lenv* e, lval* a, char* op) {
  LVAL_CHK_TYPE(op, a, 0, LVAL_NUM);
  lval* x = lval_pop(a, 0);
  
  while (a->count) {
    lval* y = lval_pop(a, 0);
    LVAL_CHK_TYPE_LOOP(op, x, x, y, LVAL_NUM);
    LVAL_CHK_TYPE_LOOP(op, y, x, y, LVAL_NUM);
    
    if (strcmp(op, "+") == 0) { x->num += y->num; }
    if (strcmp(op, "-") == 0) { x->num -= y->num; }
    if (strcmp(op, "*") == 0) { x->num *= y->num; }
    if (strcmp(op, "/") == 0) {
      if (a->num == 0) { lval_del(x); lval_del(y); x = lval_err("division by zero"); break; }
      else { x->num /= y->num; }
    }
    
    lval_del(y);
  }
  
  lval_del(a);
  return x;
}

lval* builtin_add(lenv* e, lval* a) { return builtin_op(e, a, "+"); }
lval* builtin_sub(lenv* e, lval* a) { return builtin_op(e, a, "-"); }
lval* builtin_mul(lenv* e, lval* a) { return builtin_op(e, a, "*"); }
lval* builtin_div(lenv* e, lval* a) { return builtin_op(e, a, "/"); }

lval* builtin_cmp(lenv* e, lval* a, char* op) {
  LVAL_CHK_ARGNUM_EQ(op, a, 2);
  LVAL_CHK_TYPE(op, a, 0, LVAL_NUM);
  LVAL_CHK_TYPE(op, a, 1, LVAL_NUM);
  int r;
  if (strcmp(op, ">") == 0)  { r = (a->cell[0]->num >  a->cell[1]->num); }
  if (strcmp(op, "<") == 0)  { r = (a->cell[0]->num <  a->cell[1]->num); }
  if (strcmp(op, ">=") == 0) { r = (a->cell[0]->num >= a->cell[1]->num); }
  if (strcmp(op, "<=") == 0) { r = (a->cell[0]->num <= a->cell[1]->num); }
  lval_del(a);
  return lval_num(r);
}

lval* builtin_gt(lenv* e, lval* a) { return builtin_cmp(e, a, ">");  }
lval* builtin_lt(lenv* e, lval* a) { return builtin_cmp(e, a, "<");  }
lval* builtin_ge(lenv* e, lval* a) { return builtin_cmp(e, a, ">="); }
lval* builtin_le(lenv* e, lval* a) { return builtin_cmp(e, a, "<="); }

lval* builtin_if(lenv* e, lval* a) {
  LVAL_CHK_ARGNUM_EQ("if", a, 3);
  LVAL_CHK_TYPE("if", a, 0, LVAL_NUM);
  LVAL_CHK_TYPE("if", a, 1, LVAL_QEXPR);
  LVAL_CHK_TYPE("if", a, 2, LVAL_QEXPR);
  
  lval* x;
  a->cell[1]->type = LVAL_SEXPR;
  a->cell[2]->type = LVAL_SEXPR;
  if (a->cell[0]->num) { x = lval_eval(e, lval_pop(a, 1)); }
  else { x = lval_eval(e, lval_pop(a, 2)); }
  
  lval_del(a);
  return x;
}

lval* builtin_eq(lenv* e, lval* a) {
  LVAL_CHK_ARGNUM_EQ("==", a, 2);
  if (lval_eq(a->cell[0], a->cell[1])) { lval_del(a); return lval_num(1); } else { lval_del(a); return lval_num(0); }
}

lval* builtin_neq(lenv* e, lval* a) {
  LVAL_CHK_ARGNUM_EQ("!=", a, 2);
  if (lval_eq(a->cell[0], a->cell[1])) { lval_del(a); return lval_num(0); } else { lval_del(a); return lval_num(1); }
}

static bool running = true;

lval* builtin_exit(lenv* e, lval* a) {
  LVAL_CHK_ARGNUM_EQ("exit", a, 1);
  running = false;
  return lval_take(a, 0);
}

lval* lval_read(mpc_ast_t* t);

lval* builtin_load(lenv* e, lval* a) {
  LVAL_CHK_ARGNUM_EQ("load", a, 1);
  LVAL_CHK_TYPE("load", a, 0, LVAL_STR);
  
  mpc_result_t r;
  if (mpc_fparse_contents(a->cell[0]->str, Lispy, &r)) {
    
    lval* expr = lval_read(r.output);
    
    while (expr->count) {
      lval* x = lval_eval(e, lval_pop(expr, 0));
      if (x->type == LVAL_ERR) { lval_println(e, x); }
      lval_del(x);
    }
    
    lval_del(expr);
    
    mpc_ast_delete(r.output);
    
    lval_del(a);
    return lval_sexpr();
    
  } else {    
    mpc_err_print(r.error);
    mpc_err_delete(r.error);
    lval_del(a);
    return lval_err("Could not loading Library!");
  }
}

lval* builtin_print(lenv* e, lval* a) {
  for (int i = 0; i < a->count; i++) { lval_print(e, a->cell[i]); putchar(' '); }
  putchar('\n');
  lval_del(a);
  return lval_sexpr();
}

lval* builtin_str_len(lenv* e, lval* a) {
  LVAL_CHK_ARGNUM_EQ("str-len", a, 1);
  LVAL_CHK_TYPE("str-len", a, 0, LVAL_STR);
  lval* x = lval_num(strlen(a->cell[0]->str));
  lval_del(a);
  return x;
}

lval* builtin_str_join(lenv* e, lval* a) {
  LVAL_CHK_TYPE("str-join", a, 0, LVAL_STR);
  lval* x = lval_pop(a, 0);
  
  while (a->count) {
    lval* y = lval_pop(a, 0);
    LVAL_CHK_TYPE_LOOP("str-join", x, x, y, LVAL_STR);
    LVAL_CHK_TYPE_LOOP("str-join", y, x, y, LVAL_STR); 
    x->str = realloc(x->str, strlen(x->str) + strlen(y->str) + 1);
    strcat(x->str, y->str);
    lval_del(y);
  }
  
  lval_del(a);
  return x;
}

lval* builtin_str_head(lenv* e, lval* a) {
  LVAL_CHK_ARGNUM_EQ("str-head", a, 1);
  LVAL_CHK_TYPE("str-head", a, 0, LVAL_STR);
  
  if (strlen(a->cell[0]->str) == 0) {
    lval_del(a);
    return lval_err("attempt to apply 'str-head' \"\"");
  }
  
  char* y = strnew(a->cell[0]->str);
  y[1] = '\0';
  
  lval* x = lval_str(y);
  free(y);
  lval_del(a);
  return x;
}

lval* builtin_str_tail(lenv* e, lval* a) {
  LVAL_CHK_ARGNUM_EQ("str-tail", a, 1);
  LVAL_CHK_TYPE("str-tail", a, 0, LVAL_STR);
  
  if (strlen(a->cell[0]->str) == 0) {
    lval_del(a);
    return lval_err("attempt to apply 'str-head' on \"\"");
  }
  
  char* y = strnew(a->cell[0]->str);
  lval* x = lval_str(y+1);
  free(y);
  lval_del(a);
  return x;
}

void lenv_add_builtin(lenv* e, char* name, lbuiltin f) {
  lval* sym = lval_sym(name);
  lval* fun = lval_builtin(f);
  lenv_def(e, sym, fun);
  lval_del(sym); lval_del(fun);
}

void lenv_add_builtins(lenv* e) {
  /* Variable Functions */
  lenv_add_builtin(e, "\\",   builtin_lambda);
  lenv_add_builtin(e, "def",  builtin_def); lenv_add_builtin(e, "=",     builtin_set);
  
  /* List Functions */
  lenv_add_builtin(e, "list", builtin_list);
  lenv_add_builtin(e, "head", builtin_head); lenv_add_builtin(e, "tail",  builtin_tail);
  lenv_add_builtin(e, "eval", builtin_eval); lenv_add_builtin(e, "join",  builtin_join);
  
  /* Conditional Functions */
  lenv_add_builtin(e, "if",   builtin_if);
  lenv_add_builtin(e, "==",   builtin_eq); lenv_add_builtin(e, "!=",    builtin_neq);
  lenv_add_builtin(e, ">",    builtin_gt); lenv_add_builtin(e, "<",     builtin_lt);
  lenv_add_builtin(e, ">=",   builtin_ge); lenv_add_builtin(e, "<=",    builtin_le);
  
  /* Mathematical Functions */
  lenv_add_builtin(e, "+",    builtin_add); lenv_add_builtin(e, "-",     builtin_sub);
  lenv_add_builtin(e, "*",    builtin_mul); lenv_add_builtin(e, "/",     builtin_div);
  
  /* String Functions */
  lenv_add_builtin(e, "str-len", builtin_str_len);   lenv_add_builtin(e, "str-head", builtin_str_head);
  lenv_add_builtin(e, "str-join", builtin_str_join); lenv_add_builtin(e, "str-tail", builtin_str_tail);
  
  /* Misc Functions */
  lenv_add_builtin(e, "exit", builtin_exit);
  lenv_add_builtin(e, "load", builtin_load); lenv_add_builtin(e, "print", builtin_print);
}

lval* lval_apply(lenv* e, lval* f, lval* a) {
  
  if (f->builtin) { lval* x = f->builtin(e, a); lval_del(f); return x; }
  
  char* name = lenv_sym(e, f) ? lenv_sym(e, f) : "<unknown>";
  int total = f->formals->count;
  int given = a->count;
  
  while (a->count) {
    
    if (f->formals->count == 0) {
      lval_del(f); lval_del(a);
      return lval_err("attempt to apply function '%s' on too many arguments (expected %i or less, got %i)", name, total, given); 
    }
    
    lval* sym = lval_pop(f->formals, 0);
    
    if (strcmp(sym->sym, "&") == 0) {
      
      lval_del(sym);
      
      if (f->formals->count != 1) {
        lval_del(f); lval_del(a);
        return lval_err("function format invalid. Symbol '&' not followed by single symbol.");
      }
      
      sym = lval_pop(f->formals, 0);
      lenv_put(f->env, sym, builtin_list(e, a));
      lval_del(sym);
      break;
    }
    
    lval* val = lval_pop(a, 0);
    lenv_put(f->env, sym, val);
    lval_del(sym); lval_del(val);
  }
  
  lval_del(a);
  
  if (f->formals->count > 0 &&
    strcmp(f->formals->cell[0]->sym, "&") == 0) {
    
    if (f->formals->count != 2) {
      lval_del(f);
      return lval_err("function format invalid. Symbol '&' not followed by single symbol.");
    }
    
    lval_del(lval_pop(f->formals, 0));
    lval* sym = lval_pop(f->formals, 0);
    lval* val = lval_qexpr();
    lenv_put(f->env, sym, val);
    lval_del(sym); lval_del(val);
  }
  
  if (f->formals->count == 0) {
    f->env->par = e;
    lval* x = builtin_eval(f->env, lval_add(lval_sexpr(), lval_copy(f->body)));
    lval_del(f);
    return x;
  } else {
    return f;
  }
  
}

lval* lval_eval(lenv* e, lval* v) {

  if (v->type == LVAL_SYM) { return lenv_get(e, v); }  
  if (v->type == LVAL_SEXPR) {
  
    if (v->count == 0) { return v; }  
    if (v->count == 1) { return lval_eval(e, lval_take(v, 0)); }
    if (v->count >= 2) {
    
      for (int i = 0; i < v->count; i++) {
        v->cell[i] = lval_eval(e, v->cell[i]);
        if (v->cell[i]->type == LVAL_ERR) { return lval_take(v, i); }
      }
      
      if (v->cell[0]->type != LVAL_LAMBDA) {
        char* str = lval_tostr(e, v->cell[0]);
        lval* err = lval_err("attempt apply arguments to non-function %s (got %s)", str, lval_type_names[v->cell[0]->type]);
        free(str);
        lval_del(v);
        return err;
      } else {
        return lval_apply(e, lval_pop(v, 0), v);
      }
    
    }
  }
  
  return v;
}

lval* lval_read_num(mpc_ast_t* t) {
  long x = strtol(t->contents, NULL, 10);
  return errno != ERANGE ? lval_num(x) : lval_err("invalid number");
}

lval* lval_read(mpc_ast_t* t) {
    
  if (strstr(t->tag, "number")) { return lval_read_num(t); }
  if (strstr(t->tag, "symbol")) { return lval_sym(t->contents); }
  if (strstr(t->tag, "string")) {
    char* unescaped = mpcf_unescape(mpcf_strcrop(strnew(t->contents)));
    lval* x = lval_str(unescaped);
    free(unescaped);
    return x;
  }
  
  lval* x = NULL;
  if (strcmp(t->tag, ">") == 0) { x = lval_sexpr(); } 
  if (strstr(t->tag, "sexpr"))  { x = lval_sexpr(); }
  if (strstr(t->tag, "qexpr"))  { x = lval_qexpr(); }
  
  for (int i = 0; i < t->children_num; i++) {
    if (strcmp(t->children[i]->contents, "(") == 0) { continue; }
    if (strcmp(t->children[i]->contents, ")") == 0) { continue; }
    if (strcmp(t->children[i]->contents, "}") == 0) { continue; }
    if (strcmp(t->children[i]->contents, "{") == 0) { continue; }
    if (strcmp(t->children[i]->tag, "regex")  == 0) { continue; }
    if (strstr(t->children[i]->tag, "comment")) { continue; }
    x = lval_add(x, lval_read(t->children[i]));
  }
  
  return x;
}


static char input[2048];

static void main_prompt(lenv* e) {
  
  fputs("Lispy Version 0.0.0.0.4\n", stdout);
  fputs("Press Ctrl+c to Exit\n\n", stdout);
  
  running = true;
  while (running) {
  
    fputs("lispy> ", stdout);
    fgets(input, 2047, stdin);
    
    mpc_result_t r;
    if (mpc_parse("<stdin>", input, Lispy, &r)) {
      
      mpc_ast_print(r.output);
      lval* x = lval_eval(e, lval_read(r.output));
      lval_println(e, x);
      lval_del(x);
      
      mpc_ast_delete(r.output);
      
    } else {
      mpc_err_print(r.error);
      mpc_err_delete(r.error);
    }
    
  }
}

static void main_file(lenv* e, char* filename) {
  
  mpc_result_t r;
  if (mpc_fparse_contents(filename, Lispy, &r)) {
    
    lval* expr = lval_read(r.output);
    
    while (expr->count) {
      lval* x = lval_eval(e, lval_pop(expr, 0));
      if (x->type == LVAL_ERR) { lval_println(e, x); }
      lval_del(x);
    }
    
    lval_del(expr);
    
    mpc_ast_delete(r.output);
  } else {    
    mpc_err_print(r.error);
    mpc_err_delete(r.error);
  }
  
}

int main(int argc, char** argv) {
  
  Number  = mpc_new("number");
  Symbol  = mpc_new("symbol");
  Comment = mpc_new("comment");
  String  = mpc_new("string");
  Sexpr   = mpc_new("sexpr");
  Qexpr   = mpc_new("qexpr");
  Expr    = mpc_new("expr");
  Lispy   = mpc_new("lispy");
  
  mpca_lang(
    "                                                                             \
      number  : /-?[0-9]+/ ;                                                      \
      symbol  : /[a-zA-Z_0-9\\+\\-\\*\\/\\\\\\<\\>\\=\\!\\'\\.\\&\\$]+/ ;         \
      comment : /;[^\\n\\r]*/ ;                                                   \
      string  : /\\\"(\\.|[^\\\"])*\\\"/ ;                                        \
      sexpr   : '(' <expr>* ')' ;                                                 \
      qexpr   : '{' <expr>* '}' ;                                                 \
      expr    : <number> | <symbol> | <sexpr> | <qexpr> | <string> | <comment> ;  \
      lispy   : /^/ <expr>* /$/ ;                                                 \
    ",
    Number, Symbol, Comment, String, Sexpr, Qexpr, Expr, Lispy);
  
  lenv* e = lenv_new(NULL);
  lenv_add_builtins(e);
  
  if (argc == 1) { main_prompt(e); }
  if (argc == 2) { main_file(e, argv[1]); }
  
  lenv_del(e);
  
  mpc_cleanup(7, Number, Symbol, Comment, Sexpr, Qexpr, Expr, Lispy);
  
  return 0;
}
