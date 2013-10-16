#include <stdio.h>

#include "../mpc/mpc.h"

/* Use operator string to see which operation to perform */
long eval_op(long x, char* op, long y) {
  if (strcmp(op, "+") == 0) { return x + y; }
  if (strcmp(op, "-") == 0) { return x - y; }
  if (strcmp(op, "*") == 0) { return x * y; }
  if (strcmp(op, "/") == 0) { return x / y; }
  return 0;
}

long eval_expr(mpc_ast_t* t) {
  
  /* If no children we know it is a number. Otherwise it is a list */ 
  if (t->children_num == 0) { return atoi(t->contents); }
  
  /* The operator is always second element (open parenthesis is first) */
  char* op = t->children[1]->contents;
  
  /* We store first expression value in `x` */
  long x = eval_expr(t->children[2]);
  
  /* Iterate the remaining expressions using our operator */
  int i = 3;
  while (strcmp(t->children[i]->tag, "expr") == 0) {
    x = eval_op(x, op, eval_expr(t->children[i]));
    i++;
  }
  
  return x;  
}

long eval(mpc_ast_t* t) {

  /* Root's first child is always the main expression */
  return eval_expr(t->children[0]); 
  
}

static char input[2048];

int main(int argc, char** argv) {
  
  mpc_parser_t* Number = mpc_new("number");
  mpc_parser_t* Operator = mpc_new("operator");
  mpc_parser_t* Expr = mpc_new("expr");
  mpc_parser_t* Lispy = mpc_new("lispy");
  
  mpca_lang(
    "                                                            \
      number   : /-?[0-9]+/ ;                                    \
      operator : '+' | '-' | '*' | '/' ;                         \
      expr     : <number> | '(' <operator> <expr> <expr>+ ')' ;  \
      lispy    : <expr> /$/ ;                                    \
    ",
    Number, Operator, Expr, Lispy);
  
  fputs("Lispy Version 0.0.0.0.3\n", stdout);
  fputs("Press Ctrl+c to Exit\n\n", stdout);
  
  while (1) {
  
    fputs("lispy> ", stdout);    
    fgets(input, 2047, stdin);
    
    mpc_result_t r;
    if (mpc_parse("<stdin>", input, Lispy, &r)) {
      
      long result = eval(r.output);
      fprintf(stdout, "%li\n", result);
      mpc_ast_delete(r.output);
      
    } else {    
      mpc_err_print(r.error);
      mpc_err_delete(r.error);
    }
    
  }
  
  mpc_cleanup(4, Number, Operator, Expr, Lispy);
  
  return 0;
}