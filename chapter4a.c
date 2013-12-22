#include <stdio.h>

/* Declare a static buffer for user input of maximum size 2048 */
static char input[2048];

int main(int argc, char** argv) {

  /* Print Version and Exit Information */
  fputs("Lispy Version 0.0.0.0.1\n", stdout);
  fputs("Press Ctrl+c to Exit\n\n", stdout);

  /* In a never ending loop */
  while (1) {

    /* Output our prompt */
    fputs("lispy> ", stdout);

    /* Read a line of user input of maximum size 2047 */
    fgets(input, 2047, stdin);

    /* Echo input back to user */
    fprintf(stdout, "No you're a %s", input);
  }

  return 0;
}
