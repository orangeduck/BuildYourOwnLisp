#include <stdio.h>
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

lval* lval_err(char* m) {
  lval* v = malloc(sizeof(lval));
  v->type = LVAL_ERR;
  v->err = malloc(strlen(m) + 1);
  strcpy(v->err, m);
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

lval* lval_cons(lval* x, lval* v) {
  v->count++;
  v->cell = realloc(v->cell, sizeof(lval*) * v->count);
  v->cell[v->count-1] = x;
  return v;
}

lval* lval_join(lval* x, lval* y) {
  for (int i = 0; i < y->count; i++) {
    x = lval_cons(y->cell[i], x);
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

void lval_print(lval* v);
lval* lval_eval(lval* v);

void lval_expr_print(lval* v, char open, char close) {
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
    case LVAL_SEXPR: lval_expr_print(v, '(', ')'); break;
    case LVAL_QEXPR: lval_expr_print(v, '{', '}'); break;
  }
}

void lval_println(lval* v) { lval_print(v); putchar('\n'); }

#define LVAL_CHK_ARGN(V, N) if (V->count != (N+1)) { lval_del(V); return lval_err("Function passed too many arguments!"); }
#define LVAL_CHK_TYPE(V, N, T) if (V->cell[N+1]->type != T) { lval_del(V); return lval_err("Function passed incorrect types!"); }

lval* lval_builtin_fun(lval* v) {
  
  if (strcmp(v->cell[0]->sym, "head") == 0) {
    LVAL_CHK_ARGN(v, 1);
    LVAL_CHK_TYPE(v, 0, LVAL_QEXPR);
    if (v->cell[1]->count == 0) {
      lval_del(v);
      return lval_err("Taking head of empty cell!");
    } else {
      return lval_eval(lval_take(lval_take(v, 1), 0));
    }
  }
  
  else if (strcmp(v->cell[0]->sym, "tail") == 0) {
    LVAL_CHK_ARGN(v, 1);
    LVAL_CHK_TYPE(v, 0, LVAL_QEXPR);
    if (v->cell[1]->count == 0) {
      lval_del(v);
      return lval_err("Taking tail of empty cell!");
    } else {
      return lval_eval(lval_take_tail(lval_take(v, 1)));
    }
  }
  
  else if (strcmp(v->cell[0]->sym, "eval") == 0) {
    LVAL_CHK_ARGN(v, 1);
    LVAL_CHK_TYPE(v, 0, LVAL_QEXPR);
    lval* x = lval_take(v, 1);
    x->type = LVAL_SEXPR;
    return lval_eval(x);
  }
  
  else if (strcmp(v->cell[0]->sym, "join") == 0) {
    
    lval* x = lval_pop(v, 1);
    
    while (v->count > 1) {
      lval* y = lval_pop(v, 1);
      
      /* Type Check */
      if (x->type != LVAL_QEXPR) { lval_del(x); lval_del(y); x = lval_err("Cannot Join non cell"); break; } 
      if (y->type != LVAL_QEXPR) { lval_del(x); lval_del(y); x = lval_err("Cannot Join non cell"); break; } 
      
      x = lval_join(x, y);
    }
    
    lval_del(v);
    return x;
  }
  
  return NULL;
}

lval* lval_builtin_op(lval* v) {
  
  lval* x = lval_eval(lval_pop(v, 1));
  
  while (v->count > 1) {
    lval* y = lval_eval(lval_pop(v, 1));
    
    /* Type Check */
    if (x->type != LVAL_NUM) { lval_del(x); lval_del(y); x = lval_err("Cannot operator on non number!"); break; }
    if (y->type != LVAL_NUM) { lval_del(x); lval_del(y); x = lval_err("Cannot operator on non number!"); break; }
    
    if (strcmp(v->cell[0]->sym, "+") == 0) { x->num += y->num; }
    if (strcmp(v->cell[0]->sym, "-") == 0) { x->num -= y->num; }
    if (strcmp(v->cell[0]->sym, "*") == 0) { x->num *= y->num; }
    if (strcmp(v->cell[0]->sym, "/") == 0) {
      if (v->num == 0) { lval_del(x); lval_del(y); x = lval_err("Division By Zero!"); break; }
      else { x->num /= y->num; }
    }
    
    lval_del(y);
  }
  
  lval_del(v);
  return x;
  
}

lval* lval_eval_sexpr(lval* v) {
  
  /* Empty Expression */
  if (v->count == 0) { return v; }
  
  /* Single Expression */
  if (v->count == 1) { return lval_eval(lval_take(v, 0)); }
  
  /* Error Checking */
  for (int i = 0; i < v->count; i++) {
    if (v->cell[i]->type == LVAL_ERR) { return lval_take(v, i); }
  }
  
  /* Evaluate Children */
  for (int i = 0; i < v->count; i++) {
    v->cell[i] = lval_eval(v->cell[i]);
  }
  
  /* Ensure Identifier Start */
  if (v->cell[0]->type != LVAL_SYM) { lval_del(v); return lval_err("S-expression Does not start with identifier!"); }
  if (strstr("+-*/", v->cell[0]->sym)) { return lval_builtin_op(v); }
  if (strstr("eval head tail join", v->cell[0]->sym)) { return lval_builtin_fun(v); }
  
  lval_del(v);
  return lval_err("Unknown Function!");
}

lval* lval_eval(lval* v) {
  return v->type == LVAL_SEXPR ? lval_eval_sexpr(v) : v;
}

lval* lval_read_num(mpc_ast_t* t) {
  long x = strtol(t->contents, NULL, 10);
  return errno != ERANGE ? lval_num(x) : lval_err("Invalid Number!");
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
    x = lval_cons(lval_read(t->children[i]), x);
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
    "                                                                              \
      number : /-?[0-9]+/ ;                                                        \
      symbol : \"head\" | \"tail\" | \"eval\" | \"join\" | '+' | '-' | '*' | '/' ; \
      sexpr  : '(' <expr>* ')' ;                                                   \
      qexpr  : '{' <expr>* '}' ;                                                   \
      expr   : <number> | <symbol> | <sexpr> | <qexpr> ;                            \
      lispy  : <expr> /$/ ;                                                        \
    ",
    Number, Symbol, Sexpr, Qexpr, Expr, Lispy);
  
  fputs("Lispy Version 0.0.0.0.4\n", stdout);
  fputs("Press Ctrl+c to Exit\n\n", stdout);
  
  while (1) {
  
    fputs("lispy> ", stdout);
    fgets(input, 2047, stdin);
    
    mpc_result_t r;
    if (mpc_parse("<stdin>", input, Lispy, &r)) {
      

      lval* x = lval_eval(lval_read(r.output));
      lval_println(x);
      lval_del(x);
      
      mpc_ast_delete(r.output);
      
    } else {    
      mpc_err_print(r.error);
      mpc_err_delete(r.error);
    }
    
  }
  
  mpc_cleanup(6, Number, Symbol, Sexpr, Qexpr, Expr, Lispy);
  
  return 0;
}
