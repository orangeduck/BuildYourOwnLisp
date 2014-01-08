#include "mpc.h"

int main(int argc, char** argv) {

  mpc_parser_t* Flatmate = mpc_or(4, 
    mpc_sym("Dan"),  mpc_sym("Chess"),
    mpc_sym("Adam"), mpc_sym("Lewis")
  );

  mpc_parser_t* Greet = mpc_and(2, mpcf_strfold,
    mpc_sym("Hello"),
    Flatmate, 
    free); 

  mpc_parser_t* Greetings = mpc_many(mpcf_strfold, Greet);

  /* Do some parsing... */
  
  mpc_delete(Greetings);
  
  return 0;
  
}

