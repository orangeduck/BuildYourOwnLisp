#include "../mpc/mpc.h"

void parse_flatmate(char* input) {

  /* Build a new parser 'Flatmate' */
  mpc_parser_t* Flatmate = mpc_or(4, 
    mpc_sym("Dan"),  mpc_sym("Chess"),
    mpc_sym("Adam"), mpc_sym("Lewis")
  );
  
  /* Attempt to parse into result 'r' */
  mpc_result_t r;
  if (mpc_parse("<stdin>", input, Flatmate, &r)) {
    /* On success report flatmate */
    printf("Got a flatmate %s!\n", (char*)r.output);
    free(r.output);
  } else {
    /* On Failure report nothing */
    printf("Not a flatmate!\n");
    mpc_err_delete(r.error);
  }
  
  mpc_delete(Flatmate);

}

int main(int argc, char** argv) {
  
  if (argc != 2) {
    puts("Usage: parser <flatmate>");
    return 0;
  }
  
  parse_flatmate(argv[1]);

  return 0;
}