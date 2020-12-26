#include "pcq.h"

int main(int argc, char** argv) {

  /* Build a parser 'Adjective' to recognize descriptions */
  pcq_parser_t* Adjective = pcq_or(4, 
    pcq_sym("wow"), pcq_sym("many"),
    pcq_sym("so"),  pcq_sym("such")
  );

  /* Build a parser 'Noun' to recognize things */
  pcq_parser_t* Noun = pcq_or(5,
    pcq_sym("lisp"), pcq_sym("language"),
    pcq_sym("book"), pcq_sym("build"), 
    pcq_sym("c")
  );
  
  pcq_parser_t* Phrase = pcq_and(2, pcqf_strfold, 
    Adjective, Noun, free);
  
  pcq_parser_t* Doge = pcq_many(pcqf_strfold, Phrase);

  /* Do some parsing here... */
  
  pcq_delete(Doge);
  
  return 0;
  
}

