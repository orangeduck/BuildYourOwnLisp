#include <stdio.h>
#include <stdlib.h>

#include <editline/readline.h>
#include <editline/history.h>

static char input[2048];

int main(int argc, char** argv) {
   
  /* Print Version and Exit Information */
  fputs("Lispy Version 0.0.0.0.1\n", stdout);
  fputs("Press Ctrl+c to Exit\n\n", stdout);
   
  /* In a never ending loop */
  while (1) {
    
    /* Output our prompt an get input */
    char* input = readline("lispy> ");
    
    /* Add input to history */
    add_history(input);
    
    /* Echo input back to user */    
    fprintf(stdout, "No you're a %s\n", input);

    /* Free retrived input */
    free(input);
    
  }
  
  return 0;
}
