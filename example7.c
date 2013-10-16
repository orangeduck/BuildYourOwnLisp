#include <stdio.h>
#include <stdarg.h>
#include <errno.h>
#include "../mpc/mpc.h"

enum { LVAL_ERR, LVAL_NUM, LVAL_SYM, LVAL_SEXPR, LVAL_QEXPR };

typedef struct lval {
  int type;

  long num;
  char* err;
  char* sym;
  
  int count;
  struct lval** cell;
  
} lval;

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
  v->sym = malloc(strlen(s) + 1);
  strcpy(v->sym, s);
  return v;
}

void lval_del(lval* v) {

  switch (v->type) {
    case LVAL_ERR: free(v->err); break;
    case LVAL_NUM: break;
    case LVAL_SYM: free(v->sym); break;
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

lval* lval_app(lval* v, lval* x) {
  v->count++;
  v->cell = realloc(v->cell, sizeof(lval*) * v->count);
  v->cell[v->count-1] = x;
  return v;
}

lval* lval_cons(lval* x, lval* v) {
  v->count++;
  v->cell = realloc(v->cell, sizeof(lval*) * v->count);
  memmove(&v->cell[1], &v->cell[0], sizeof(lval*) * (v->count-1));
  v->cell[0] = x;
  return v;
}

lval* lval_join(lval* x, lval* y) {
  for (int i = 0; i < y->count; i++) {
    x = lval_app(x, y->cell[i]);
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

lval* lval_take_tail(lval* v) {
  lval_del(lval_pop(v, 0));
  return v;
}

typedef struct {
  int count;
  char** syms;
  lval** vals;
} lenv;

lenv* lenv_new(void) {
  lenv* e = malloc(sizeof(lenv));
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

lval* lval_copy(lval* v) {
  lval* x = malloc(sizeof(lval));
  x->type = v->type;
  switch (v->type) {
    case LVAL_ERR: x->err = malloc(strlen(v->err) + 1); strcpy(x->err, v->err); break;
    case LVAL_NUM: x->num = v->num; break;
    case LVAL_SYM: x->sym = malloc(strlen(v->sym) + 1); strcpy(x->sym, v->sym); break;
    case LVAL_SEXPR:
    case LVAL_QEXPR:
      x->count = v->count;
      x->cell = malloc(sizeof(lval*) * x->count);
      for (int i = 0; i < x->count; i++) {
        x->cell[i] = lval_copy(v->cell[i]);
      }
    break;
  }
  return x;
}

lval* lenv_get(lenv* e, lval* k) {
  for (int i = 0; i < e->count; i++) {
    if (strcmp(e->syms[i], k->sym) == 0) { return lval_copy(e->vals[i]); }
  }
  return lval_err("unbound symbol '%s'", k->sym);
}

void lenv_put(lenv* e, lval* k, lval* v) {
  for (int i = 0; i < e->count; i++) {
    if (strcmp(e->syms[i], k->sym) == 0) {
      lval_del(e->vals[i]);
      e->vals[i] = lval_copy(v);
      e->syms[i] = realloc(e->syms[i], strlen(k->sym)+1);
      strcpy(e->syms[i], k->sym);
      return;
    }
  }
  
  e->count++;
  e->vals = realloc(e->vals, sizeof(lval*) * e->count);
  e->syms = realloc(e->syms, sizeof(char*) * e->count);
  e->vals[e->count-1] = lval_copy(v);
  e->syms[e->count-1] = malloc(strlen(k->sym)+1);
  strcpy(e->syms[e->count-1], k->sym);
}

void lenv_rem(lenv* e, lval* k) {
  for (int i = 0; i < e->count; i++) {
    if (strcmp(e->syms[i], k->sym) == 0) {
      lval_del(e->vals[i]);
      free(e->syms[i]);
      memmove(&e->vals[i], &e->vals[i+1], sizeof(lval*) * (e->count-i-1));
      memmove(&e->syms[i], &e->syms[i+1], sizeof(char*) * (e->count-i-1));
      e->count--;
      e->vals = realloc(e->vals, sizeof(lval*) * e->count);
      e->syms = realloc(e->syms, sizeof(char*) * e->count);
      return;
    }
  }
}

void lval_print(lval* v);

void lval_print_expr(lval* v, char open, char close) {
  putchar(open);
  for (int i = 0; i < v->count; i++) {
    lval_print(v->cell[i]);    
    if (i != (v->count-1)) {
      putchar(' ');
    }
  }
  putchar(close);
}

void lval_print(lval* v) {
  switch (v->type) {
    case LVAL_NUM:   printf("%li", v->num); break;
    case LVAL_ERR:   printf("Error: %s", v->err); break;
    case LVAL_SYM:   printf("%s", v->sym); break;
    case LVAL_SEXPR: lval_print_expr(v, '(', ')'); break;
    case LVAL_QEXPR: lval_print_expr(v, '{', '}'); break;
  }
}

void lval_println(lval* v) { lval_print(v); putchar('\n'); }

#define LVAL_CHK_ARGN_GE(F, V, N) if (V->count  < (N+1)) { lval_del(V); return lval_err("attempt to apply '%s' on an incorrect number of arguments (expected at least %i, got %i)", F, N, (V->count-1)); }
#define LVAL_CHK_ARGN_EQ(F, V, N) if (V->count != (N+1)) { lval_del(V); return lval_err("attempt to apply '%s' on an incorrect number of arguments (expected %i, got %i)", F, N, (V->count-1)); }
#define LVAL_CHK_TYPE(F, V, N, T) if (V->cell[N+1]->type != T) { lval_del(V); return lval_err("attempt to apply '%s' on invalid argument", F); }
#define LVAL_CHK_TYPE_LOOP(F, X, V0, V1, T) if (X->type != T) { lval_del(V0); lval_del(V1); V0 = lval_err("attempt to apply '%s' on invalid argument", F); break; }

lval* lval_eval(lenv* e, lval* v);

lval* lval_builtin(lenv* e, lval* v) {
  
  if (strcmp(v->cell[0]->sym, "def") == 0) {
    LVAL_CHK_ARGN_GE("def", v, 2);
    LVAL_CHK_TYPE("def", v, 0, LVAL_QEXPR);
    lval* lvals = v->cell[1];
    for (int i = 0; i < lvals->count; i++) { if (lvals->cell[i]->type != LVAL_SYM) { lval_del(v); return lval_err("attempt to define non symbolic argument"); } }
    if (lvals->count != v->count-2) { lval_del(v); return lval_err("attempt to define %i values to %i symbols", v->count-2, lvals->count); }
    for (int i = 0; i < lvals->count; i++) {
      lenv_put(e, lvals->cell[i], v->cell[i+2]);
    }
    lval_del(v);
    return lval_sexpr();
  }
  
  if (strcmp(v->cell[0]->sym, "head") == 0) {
    LVAL_CHK_ARGN_EQ("head", v, 1);
    LVAL_CHK_TYPE("head", v, 0, LVAL_QEXPR);
    if (v->cell[1]->count == 0) {
      lval_del(v);
      return lval_err("attempt to apply 'head' on {}");
    } else {
      return lval_eval(e, lval_take(lval_take(v, 1), 0));
    }
  }
  
  else if (strcmp(v->cell[0]->sym, "tail") == 0) {
    LVAL_CHK_ARGN_EQ("tail", v, 1);
    LVAL_CHK_TYPE("tail", v, 0, LVAL_QEXPR);
    if (v->cell[1]->count == 0) {
      lval_del(v);
      return lval_err("attempt to apply 'tail' on {}");
    } else {
      return lval_eval(e, lval_take_tail(lval_take(v, 1)));
    }
  }
  
  else if (strcmp(v->cell[0]->sym, "eval") == 0) {
    LVAL_CHK_ARGN_EQ("eval", v, 1);
    LVAL_CHK_TYPE("eval", v, 0, LVAL_QEXPR);
    lval* x = lval_take(v, 1);
    x->type = LVAL_SEXPR;
    return lval_eval(e, x);
  }
  
  else if (strcmp(v->cell[0]->sym, "cons") == 0) {
    LVAL_CHK_ARGN_EQ("cons", v, 2);
    LVAL_CHK_TYPE("cons", v, 1, LVAL_QEXPR);
    lval* x = lval_pop(v, 1);
    lval* y = lval_pop(v, 1);
    lval_del(v);
    return lval_cons(x, y);
  }
  
  else if (strcmp(v->cell[0]->sym, "app") == 0) {
    LVAL_CHK_ARGN_EQ("app", v, 2);
    LVAL_CHK_TYPE("app", v, 0, LVAL_QEXPR);
    lval* x = lval_pop(v, 1);
    lval* y = lval_pop(v, 1);
    lval_del(v);
    return lval_app(x, y);
  }
  
  else if (strcmp(v->cell[0]->sym, "join") == 0) {
    
    lval* x = lval_pop(v, 1);
    
    while (v->count > 1) {
      lval* y = lval_pop(v, 1);
      LVAL_CHK_TYPE_LOOP("join", x, x, y, LVAL_QEXPR);
      LVAL_CHK_TYPE_LOOP("join", y, x, y, LVAL_QEXPR); 
      x = lval_join(x, y);
    }
    
    lval_del(v);
    return x;
  }
  
  else if (strstr("+-*/", v->cell[0]->sym)) {
  
    LVAL_CHK_TYPE(v->cell[0]->sym, v, 1, LVAL_NUM);
    lval* x = lval_eval(e, lval_pop(v, 1));
    
    while (v->count > 1) {
      lval* y = lval_eval(e, lval_pop(v, 1));
      
      LVAL_CHK_TYPE_LOOP(v->cell[0]->sym, x, x, y, LVAL_NUM);
      LVAL_CHK_TYPE_LOOP(v->cell[0]->sym, y, x, y, LVAL_NUM);
      
      if (strcmp(v->cell[0]->sym, "+") == 0) { x->num += y->num; }
      if (strcmp(v->cell[0]->sym, "-") == 0) { x->num -= y->num; }
      if (strcmp(v->cell[0]->sym, "*") == 0) { x->num *= y->num; }
      if (strcmp(v->cell[0]->sym, "/") == 0) {
        if (v->num == 0) { lval_del(x); lval_del(y); x = lval_err("division by zero"); break; }
        else { x->num /= y->num; }
      }
      
      lval_del(y);
    }
    
    lval_del(v);
    return x;
  }
  
  return NULL;
}

lval* lval_eval_sexpr(lenv* e, lval* v) {
  
  for (int i = 1; i < v->count; i++) {
    v->cell[i] = lval_eval(e, v->cell[i]);
  }
  
  if (v->count == 0) { return v; }  
  if (v->count == 1) { return lval_eval(e, lval_take(v, 0)); }

  for (int i = 0; i < v->count; i++) {
    if (v->cell[i]->type == LVAL_ERR) { return lval_take(v, i); }
  }
  
  /* Ensure Identifier Start */
  if (v->cell[0]->type != LVAL_SYM) { lval_del(v); return lval_err("first element is not a function"); }
  if (strstr("+-*/ app cons eval head tail join def", v->cell[0]->sym)) { return lval_builtin(e, v); }
  
  lval_del(v);
  return lval_err("Weird unbound symbol '%s'", v->cell[0]->sym);
}

lval* lval_eval(lenv* e, lval* v) {
  if (v->type == LVAL_SYM) { return lenv_get(e, v); }
  if (v->type == LVAL_SEXPR) { return lval_eval_sexpr(e, v); }
  return v;
}

lval* lval_read_num(mpc_ast_t* t) {
  long x = strtol(t->contents, NULL, 10);
  return errno != ERANGE ? lval_num(x) : lval_err("invalid number");
}

lval* lval_read(mpc_ast_t* t) {
  
  if (strcmp(t->tag, "root") == 0) { return lval_read(t->children[0]); }
  
  if (t->children_num == 0) {
    if (strchr("0123456789", t->contents[0])) {
      return lval_read_num(t);
    } else {
      return lval_sym(t->contents);
    }
  }
  
  lval* x;
  if (strcmp(t->tag, "sexpr") == 0) { x = lval_sexpr(); }
  if (strcmp(t->tag, "qexpr") == 0) { x = lval_qexpr(); }
  
  int i = 1;
  while (strcmp(t->children[i]->tag, "char") != 0) {
    x = lval_app(x, lval_read(t->children[i]));
    i++;
  }
  
  return x;
}


static char input[2048];

int main(int argc, char** argv) {
  
  mpc_parser_t* Number = mpc_new("number");
  mpc_parser_t* Symbol = mpc_new("symbol");
  mpc_parser_t* Sexpr = mpc_new("sexpr");
  mpc_parser_t* Qexpr = mpc_new("qexpr");
  mpc_parser_t* Expr = mpc_new("expr");
  mpc_parser_t* Lispy = mpc_new("lispy");
  
  mpca_lang(
    "                                                     \
      number : /-?[0-9]+/ ;                               \
      symbol : /[a-zA-Z_\\+\\-\\*\\/\\\\]+/ ;             \
      sexpr  : '(' <expr>* ')' ;                          \
      qexpr  : '{' <expr>* '}' ;                          \
      expr   : <number> | <symbol> | <sexpr> | <qexpr> ;  \
      lispy  : <expr> /$/ ;                               \
    ",
    Number, Symbol, Sexpr, Qexpr, Expr, Lispy);
  
  fputs("Lispy Version 0.0.0.0.4\n", stdout);
  fputs("Press Ctrl+c to Exit\n\n", stdout);
  
  lenv* e = lenv_new();

  while (1) {
  
    fputs("lispy> ", stdout);
    fgets(input, 2047, stdin);
    
    mpc_result_t r;
    if (mpc_parse("<stdin>", input, Lispy, &r)) {
      
      lval* x = lval_eval(e, lval_read(r.output));
      lval_println(x);
      lval_del(x);
      
      mpc_ast_delete(r.output);
      
    } else {    
      mpc_err_print(r.error);
      mpc_err_delete(r.error);
    }
    
  }
  
  lenv_del(e);
  
  mpc_cleanup(6, Number, Symbol, Sexpr, Qexpr, Expr, Lispy);
  
  return 0;
}
