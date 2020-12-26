#include "pcq.h"

int main(int argc, char** argv) {

  pcq_parser_t* Adjective = pcq_new("adjective");
  pcq_parser_t* Noun      = pcq_new("noun");
  pcq_parser_t* Phrase    = pcq_new("phrase");
  pcq_parser_t* Doge      = pcq_new("doge");

  pcqa_lang(PCQA_LANG_DEFAULT,
    "                                           \
      adjective : \"wow\" | \"many\"            \
                |  \"so\" | \"such\";           \
      noun      : \"lisp\" | \"language\"       \
                | \"book\" | \"build\" | \"c\"; \
      phrase    : <adjective> <noun>;           \
      doge      : <phrase>*;                    \
    ",
    Adjective, Noun, Phrase, Doge);

  /* Do some parsing here... */

  pcq_cleanup(4, Adjective, Noun, Phrase, Doge);
  
  return 0;
  
}
