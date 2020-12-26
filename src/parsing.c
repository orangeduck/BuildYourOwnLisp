#include "pcq.h"

#ifdef _WIN32

static char buffer[2048];

char* readline(char* prompt) {
  fputs(prompt, stdout);
  fgets(buffer, 2048, stdin);
  char* cpy = malloc(strlen(buffer)+1);
  strcpy(cpy, buffer);
  cpy[strlen(cpy)-1] = '\0';
  return cpy;
}

void add_history(char* unused) {}

#else
#include <editline/readline.h>
#include <editline/history.h>
#endif

int main(int argc, char** argv) {
  
  /* Create Some Parsers */
  pcq_parser_t* Number   = pcq_new("number");
  pcq_parser_t* Operator = pcq_new("operator");
  pcq_parser_t* Expr     = pcq_new("expr");
  pcq_parser_t* Lispy    = pcq_new("lispy");
  
  /* Define them with the following Language */
  pcqa_lang(PCQA_LANG_DEFAULT,
    "                                                     \
      number   : /-?[0-9]+/ ;                             \
      operator : '+' | '-' | '*' | '/' ;                  \
      expr     : <number> | '(' <operator> <expr>+ ')' ;  \
      lispy    : /^/ <operator> <expr>+ /$/ ;             \
    ",
    Number, Operator, Expr, Lispy);
  
  puts("Lispy Version 0.0.0.0.2");
  puts("Press Ctrl+c to Exit\n");
  
  while (1) {
  
    char* input = readline("lispy> ");
    add_history(input);
    
    /* Attempt to parse the user input */
    pcq_result_t r;
    if (pcq_parse("<stdin>", input, Lispy, &r)) {
      /* On success print and delete the AST */
      pcq_ast_print(r.output);
      pcq_ast_delete(r.output);
    } else {
      /* Otherwise print and delete the Error */
      pcq_err_print(r.error);
      pcq_err_delete(r.error);
    }
    
    free(input);
  }
  
  /* Undefine and delete our parsers */
  pcq_cleanup(4, Number, Operator, Expr, Lispy);
  
  return 0;
}
