#include <stdio.h>
#include <stdarg.h>
#include <stdbool.h>
#include <errno.h>
#include "../mpc/mpc.h"

static mpc_parser_t* Number;
static mpc_parser_t* Symbol; 
static mpc_parser_t* Comment; 
static mpc_parser_t* Sexpr;
static mpc_parser_t* Qexpr;
static mpc_parser_t* Expr; 
static mpc_parser_t* Lispy;

enum { LVAL_ERR, LVAL_NUM, LVAL_SYM, LVAL_LAMBDA, LVAL_SEXPR, LVAL_QEXPR };

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

void lval_print(lenv* e, lval* v);
char* lenv_sym(lenv* e, lval* v);

void lval_print_expr(lenv* e, lval* v, char open, char close) {
  putchar(open);
  for (int i = 0; i < v->count; i++) {
    lval_print(e, v->cell[i]);    
    if (i != (v->count-1)) {
      putchar(' ');
    }
  }
  putchar(close);
}

void lval_print(lenv* e, lval* v) {
  switch (v->type) {
    case LVAL_NUM:   printf("%li", v->num); break;
    case LVAL_ERR:   printf("Error: %s", v->err); break;
    case LVAL_SYM:   printf("%s", v->sym); break;
    case LVAL_LAMBDA:
      if (v->builtin) {
        printf("(builtin '%s')", lenv_sym(e, v));
      } else {
        printf("(\\ "); lval_print(e, v->formals); putchar(' '); lval_print(e, v->body); putchar(')');
      }
    break;
    case LVAL_SEXPR: lval_print_expr(e, v, '(', ')'); break;
    case LVAL_QEXPR: lval_print_expr(e, v, '{', '}'); break;
  }
}

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

#define LVAL_CHK_ARGNUM_GE(F, A, N) if (A->count  < N) { lval* err = lval_err("attempt to apply '%s' on too few of arguments (expected at least %i, got %i)", F, N, A->count); lval_del(A); return err; }
#define LVAL_CHK_ARGNUM_EQ(F, A, N) if (A->count != N) { lval* err = lval_err("attempt to apply '%s' on the wrong number of arguments (expected %i, got %i)", F, N, A->count); lval_del(A); return err; }
#define LVAL_CHK_TYPE(F, A, N, T) if (A->cell[N]->type != T) { lval_del(A); return lval_err("attempt to apply '%s' on invalid argument", F); }

lval* builtin_eval(lenv* e, lval* a);
lval* lval_eval(lenv* e, lval* v);

lval* builtin_assign(lenv* e, lval* a, char* assign) {
  LVAL_CHK_ARGNUM_GE(assign, a, 2);
  LVAL_CHK_TYPE(assign, a, 0, LVAL_QEXPR);
  
  lval* formals = a->cell[0];
  
  for (int i = 0; i < formals->count; i++) {
    if (formals->cell[i]->type != LVAL_SYM) { lval_del(a); return lval_err("attempt to define non symbolic argument"); }
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
    if (a->cell[0]->cell[i]->type != LVAL_SYM) { lval_del(a); return lval_err("attempt to define non symbolic argument"); }
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

#define LVAL_CHK_TYPE_LOOP(F, X, A0, A1, T) if (X->type != T) { lval_del(A0); lval_del(A1); A0 = lval_err("attempt to apply '%s' on invalid argument", F); break; }

lval* builtin_join(lenv* e, lval* a) {
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
  lval* x = lval_eval(e, lval_pop(a, 0));
  
  while (a->count) {
    lval* y = lval_eval(e, lval_pop(a, 0));
    
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
  LVAL_CHK_TYPE("load", a, 0, LVAL_QEXPR);
  if (a->cell[0]->count != 1) { lval_del(a); return lval_err("Invalid Library!"); }
  if (a->cell[0]->cell[0]->type != LVAL_SYM) { lval_del(a); return lval_err("Invalid Library!"); }
  
  mpc_result_t r;
  if (mpc_fparse_contents(a->cell[0]->cell[0]->sym, Lispy, &r)) {
    
    lval* x = lval_eval(e, lval_read(r.output));
    lval_del(x);
    
    mpc_ast_delete(r.output);
    
    lval_del(a);
    lval* res = lval_qexpr();
    lval_add(res, lval_sym("ok"));
    return res;
    
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

void lenv_add_builtin(lenv* e, char* name, lbuiltin f) {
  lval* sym = lval_sym(name);
  lval* fun = lval_builtin(f);
  lenv_def(e, sym, fun);
  lval_del(sym); lval_del(fun);
}

void lenv_add_builtins(lenv* e) {
  lenv_add_builtin(e, "def",  builtin_def);    lenv_add_builtin(e, "=",     builtin_set);
  lenv_add_builtin(e, "\\",   builtin_lambda);
  
  lenv_add_builtin(e, "head", builtin_head);   lenv_add_builtin(e, "tail",  builtin_tail);
  lenv_add_builtin(e, "eval", builtin_eval);   lenv_add_builtin(e, "join",  builtin_join);
  lenv_add_builtin(e, "list", builtin_list);
  
  lenv_add_builtin(e, "if",   builtin_if);
  lenv_add_builtin(e, "==",   builtin_eq);     lenv_add_builtin(e, "!=",    builtin_neq);
  lenv_add_builtin(e, ">",    builtin_gt);     lenv_add_builtin(e, "<",     builtin_lt);
  lenv_add_builtin(e, ">=",   builtin_ge);     lenv_add_builtin(e, "<=",    builtin_le);
  
  lenv_add_builtin(e, "+",    builtin_add);    lenv_add_builtin(e, "-",     builtin_sub);
  lenv_add_builtin(e, "*",    builtin_mul);    lenv_add_builtin(e, "/",     builtin_div);
  
  lenv_add_builtin(e, "load", builtin_load);   lenv_add_builtin(e, "print", builtin_print);
  lenv_add_builtin(e, "exit",  builtin_exit);
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
        return lval_del(v), lval_err("attempt apply arguments to non-function");
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
  
  lval* x = NULL;
  if (strcmp(t->tag, ">") == 0) { x = lval_sexpr(); } 
  if (strstr(t->tag, "sexpr"))  { x = lval_sexpr(); }
  if (strstr(t->tag, "qexpr"))  { x = lval_qexpr(); }
  
  for (int i = 0; i < t->children_num; i++) {
    if (strcmp(t->children[i]->contents, "(") == 0) { continue; }
    if (strcmp(t->children[i]->contents, ")") == 0) { continue; }
    if (strcmp(t->children[i]->contents, "}") == 0) { continue; }
    if (strcmp(t->children[i]->contents, "{") == 0) { continue; }
    if (strcmp(t->children[i]->contents,  "") == 0) { continue; }
    if (strstr(t->children[i]->tag, "comment")) { continue; }
    x = lval_add(x, lval_read(t->children[i]));;
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
    lval* x = lval_eval(e, lval_read(r.output));
    if (x->type == LVAL_ERR) { lval_println(e, x); }
    lval_del(x);
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
  Sexpr   = mpc_new("sexpr");
  Qexpr   = mpc_new("qexpr");
  Expr    = mpc_new("expr");
  Lispy   = mpc_new("lispy");
  
  mpca_lang(
    "                                                                 \
      number  : /-?[0-9]+/ ;                                          \
      symbol  : /[a-zA-Z_\\+\\-\\*\\/\\\\\\<\\>\\=\\!\\'\\.\\&]+/ ;   \
      comment : /;[^\\n\\r]*/ ;                                       \
      sexpr   : '(' <expr>* ')' ;                                     \
      qexpr   : '{' <expr>* '}' ;                                     \
      expr    : <number> | <symbol> | <sexpr> | <qexpr> | <comment> ; \
      lispy   : /^/ <expr>* /$/ ;                                     \
    ",
    Number, Symbol, Comment, Sexpr, Qexpr, Expr, Lispy);
  
  lenv* e = lenv_new(NULL);
  lenv_add_builtins(e);
  
  if (argc == 1) { main_prompt(e); }
  if (argc == 2) { main_file(e, argv[1]); }
  
  lenv_del(e);
  
  mpc_cleanup(7, Number, Symbol, Comment, Sexpr, Qexpr, Expr, Lispy);
  
  return 0;
}
