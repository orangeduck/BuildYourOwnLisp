#include <stdio.h>

#include "../mpc/mpc.h"

static char input[2048];

int main(int argc, char** argv) {
  
  /* Create Some Parsers */
  mpc_parser_t* Number = mpc_new("number");
  mpc_parser_t* Operator = mpc_new("operator");
  mpc_parser_t* Expr = mpc_new("expr");
  mpc_parser_t* Lispy = mpc_new("lispy");
  
  /* Define them with the following Language */
  mpca_lang(
    "                                                     \
      number   : /-?[0-9]+/ ;                             \
      operator : '+' | '-' | '*' | '/' ;                  \
      expr     : <number> | '(' <operator> <expr>+ ')' ;  \
      lispy    : <expr> /$/ ;                             \
    ",
    Number, Operator, Expr, Lispy);
  
  fputs("Lispy Version 0.0.0.0.2\n", stdout);
  fputs("Press Ctrl+c to Exit\n\n", stdout);
  
  while (1) {
  
    fputs("lispy> ", stdout);    
    fgets(input, 2047, stdin);
    
    /* Attempt to Parse the Input */
    mpc_result_t r;
    if (mpc_parse("<stdin>", input, Lispy, &r)) {
    
      /* On Success Print the AST */
      mpc_ast_print(r.output);
      mpc_ast_delete(r.output);
      puts("\n");
      
    } else {
    
      /* Otherwise Print the Error */
      mpc_err_print(r.error);
      mpc_err_delete(r.error);
      
    }
    
  }
  
  /* Undefine and Delete our Parsers */
  mpc_cleanup(4, Number, Operator, Expr, Lispy);
  
  return 0;
}