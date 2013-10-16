#include <stdio.h>

#include "../mpc/mpc.h"

int main(int argc, char** argv) {

  mpc_parser_t* Flatmate = mpc_or(4, 
    mpc_string("Dan"),
    mpc_string("Chess"),
    mpc_string("Adam"),
    mpc_string("Lewis")
  );

  mpc_parser_t* Greet = mpc_also(
    mpc_string("Hello "), Flatmate, 
    free, mpcf_strfold); 

  mpc_parser_t* Greetings = mpc_many(Greet, mpcf_strfold);


  mpc_delete(Greetings);
  
  return 0;
  
}

