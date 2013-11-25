#include <stdio.h>
#include <stdlib.h>

/* If we are compiling on Windows include these functions */
#ifdef _WIN32

static char input[2048];

char* readline(char* prompt) {
  
  fputs("lispy> ", stdout);
  fgets(input, 2047, stdin);
    
  char* cpy = malloc(strlen(input)+1);
  strcpy(cpy, input);
  input[strlen(input)] = '\0';
  
  return input;
}

void add_history(char* unused) {}

/* Otherwise include the editline headers */
#else

#include <editline/readline.h>
#include <editline/history.h>

#endif

int main(int argc, char** argv) {
   
  fputs("Lispy Version 0.0.0.0.1\n", stdout);
  fputs("Press Ctrl+c to Exit\n\n", stdout);
   
  while (1) {
    
    /* In either case readline will be defined */
    char* input = readline("lispy> ");
    add_history(input);

    fprintf(stdout, "No you're a %s\n", input);
    free(input);
    
  }
  
  return 0;
}
