#include "mpc.h"

int main(int argc, char** argv) {

  mpc_parser_t* Flatmate  = mpc_new("flatmate");
  mpc_parser_t* Greet     = mpc_new("greet");
  mpc_parser_t* Greetings = mpc_new("greetings");

  mpca_lang(
    "                                                            \
      flatmate  : \"Chess\" | \"Dan\" | \"Adam\" | \"Lewis\";    \
      greet     : \"Hello\" <flatmate>;                          \
      greetings : <greet>*;                                      \
    ",
    Flatmate, Greet, Greetings);

  /* Do some parsing... */

  mpc_cleanup(3, Flatmate, Greet, Greetings);
  
  return 0;
  
}