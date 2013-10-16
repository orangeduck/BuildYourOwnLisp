#include "../mpc/mpc.h"

int main(int argc, char** argv) {
  
  /* Requires some input on the command line */
  if (argc != 2) {
    puts("Usage: parser <flatmate>");
    return 0;
  }
  
  /* Build a new parser 'Flatmate' */
  mpc_parser_t* Flatmate = mpc_or(4, 
    mpc_string("Dan"),
    mpc_string("Chess"),
    mpc_string("Adam"),
    mpc_string("Lewis")
  );
  
  /* Parse the result into result 'r' */
  mpc_result_t r;
  if (mpc_parse("<stdin>", argv[1], Flatmate, &r)) {
    /* On success report flatmate */
    printf("Got a flatmate %s!\n", (char*)r.output);
    free(r.output);
  } else {
    /* On Failure report nothing */
    printf("Not a flatmate!\n");
    mpc_err_delete(r.error);
  }
  
  mpc_delete(Flatmate);

  return 0;
}