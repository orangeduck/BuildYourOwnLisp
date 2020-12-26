/*
** pcq - Micro Parser Combinator library for C
**
** https://github.com/orangeduck/pcq
**
** Daniel Holden - contact@daniel-holden.com
** Licensed under BSD3
*/

#ifndef pcq_h
#define pcq_h

#ifdef __cplusplus
extern "C" {
#endif

#include <stdlib.h>
#include <stdio.h>
#include <stdarg.h>
#include <string.h>
#include <math.h>
#include <errno.h>
#include <ctype.h>

/*
** State Type
*/

typedef struct {
  long pos;
  long row;
  long col;
  int term;
} pcq_state_t;

/*
** Error Type
*/

typedef struct {
  pcq_state_t state;
  int expected_num;
  char *filename;
  char *failure;
  char **expected;
  char received;
} pcq_err_t;

void pcq_err_delete(pcq_err_t *e);
char *pcq_err_string(pcq_err_t *e);
void pcq_err_print(pcq_err_t *e);
void pcq_err_print_to(pcq_err_t *e, FILE *f);

/*
** Parsing
*/

typedef void pcq_val_t;

typedef union {
  pcq_err_t *error;
  pcq_val_t *output;
} pcq_result_t;

struct pcq_parser_t;
typedef struct pcq_parser_t pcq_parser_t;

int pcq_parse(const char *filename, const char *string, pcq_parser_t *p, pcq_result_t *r);
int pcq_nparse(const char *filename, const char *string, size_t length, pcq_parser_t *p, pcq_result_t *r);
int pcq_parse_file(const char *filename, FILE *file, pcq_parser_t *p, pcq_result_t *r);
int pcq_parse_pipe(const char *filename, FILE *pipe, pcq_parser_t *p, pcq_result_t *r);
int pcq_parse_contents(const char *filename, pcq_parser_t *p, pcq_result_t *r);

/*
** Function Types
*/

typedef void(*pcq_dtor_t)(pcq_val_t*);
typedef pcq_val_t*(*pcq_ctor_t)(void);

typedef pcq_val_t*(*pcq_apply_t)(pcq_val_t*);
typedef pcq_val_t*(*pcq_apply_to_t)(pcq_val_t*,void*);
typedef pcq_val_t*(*pcq_fold_t)(int,pcq_val_t**);

typedef int(*pcq_check_t)(pcq_val_t**);
typedef int(*pcq_check_with_t)(pcq_val_t**,void*);

/*
** Building a Parser
*/

pcq_parser_t *pcq_new(const char *name);
pcq_parser_t *pcq_copy(pcq_parser_t *a);
pcq_parser_t *pcq_define(pcq_parser_t *p, pcq_parser_t *a);
pcq_parser_t *pcq_undefine(pcq_parser_t *p);

void pcq_delete(pcq_parser_t *p);
void pcq_cleanup(int n, ...);

/*
** Basic Parsers
*/

pcq_parser_t *pcq_any(void);
pcq_parser_t *pcq_char(char c);
pcq_parser_t *pcq_range(char s, char e);
pcq_parser_t *pcq_oneof(const char *s);
pcq_parser_t *pcq_noneof(const char *s);
pcq_parser_t *pcq_satisfy(int(*f)(char));
pcq_parser_t *pcq_string(const char *s);

/*
** Other Parsers
*/

pcq_parser_t *pcq_pass(void);
pcq_parser_t *pcq_fail(const char *m);
pcq_parser_t *pcq_failf(const char *fmt, ...);
pcq_parser_t *pcq_lift(pcq_ctor_t f);
pcq_parser_t *pcq_lift_val(pcq_val_t *x);
pcq_parser_t *pcq_anchor(int(*f)(char,char));
pcq_parser_t *pcq_state(void);

/*
** Combinator Parsers
*/

pcq_parser_t *pcq_expect(pcq_parser_t *a, const char *e);
pcq_parser_t *pcq_expectf(pcq_parser_t *a, const char *fmt, ...);
pcq_parser_t *pcq_apply(pcq_parser_t *a, pcq_apply_t f);
pcq_parser_t *pcq_apply_to(pcq_parser_t *a, pcq_apply_to_t f, void *x);
pcq_parser_t *pcq_check(pcq_parser_t *a, pcq_dtor_t da, pcq_check_t f, const char *e);
pcq_parser_t *pcq_check_with(pcq_parser_t *a, pcq_dtor_t da, pcq_check_with_t f, void *x, const char *e);
pcq_parser_t *pcq_checkf(pcq_parser_t *a, pcq_dtor_t da, pcq_check_t f, const char *fmt, ...);
pcq_parser_t *pcq_check_withf(pcq_parser_t *a, pcq_dtor_t da, pcq_check_with_t f, void *x, const char *fmt, ...);

pcq_parser_t *pcq_not(pcq_parser_t *a, pcq_dtor_t da);
pcq_parser_t *pcq_not_lift(pcq_parser_t *a, pcq_dtor_t da, pcq_ctor_t lf);
pcq_parser_t *pcq_maybe(pcq_parser_t *a);
pcq_parser_t *pcq_maybe_lift(pcq_parser_t *a, pcq_ctor_t lf);

pcq_parser_t *pcq_many(pcq_fold_t f, pcq_parser_t *a);
pcq_parser_t *pcq_many1(pcq_fold_t f, pcq_parser_t *a);
pcq_parser_t *pcq_count(int n, pcq_fold_t f, pcq_parser_t *a, pcq_dtor_t da);

pcq_parser_t *pcq_or(int n, ...);
pcq_parser_t *pcq_and(int n, pcq_fold_t f, ...);

pcq_parser_t *pcq_predictive(pcq_parser_t *a);

/*
** Common Parsers
*/

pcq_parser_t *pcq_eoi(void);
pcq_parser_t *pcq_soi(void);

pcq_parser_t *pcq_boundary(void);
pcq_parser_t *pcq_boundary_newline(void);

pcq_parser_t *pcq_whitespace(void);
pcq_parser_t *pcq_whitespaces(void);
pcq_parser_t *pcq_blank(void);

pcq_parser_t *pcq_newline(void);
pcq_parser_t *pcq_tab(void);
pcq_parser_t *pcq_escape(void);

pcq_parser_t *pcq_digit(void);
pcq_parser_t *pcq_hexdigit(void);
pcq_parser_t *pcq_octdigit(void);
pcq_parser_t *pcq_digits(void);
pcq_parser_t *pcq_hexdigits(void);
pcq_parser_t *pcq_octdigits(void);

pcq_parser_t *pcq_lower(void);
pcq_parser_t *pcq_upper(void);
pcq_parser_t *pcq_alpha(void);
pcq_parser_t *pcq_underscore(void);
pcq_parser_t *pcq_alphanum(void);

pcq_parser_t *pcq_int(void);
pcq_parser_t *pcq_hex(void);
pcq_parser_t *pcq_oct(void);
pcq_parser_t *pcq_number(void);

pcq_parser_t *pcq_real(void);
pcq_parser_t *pcq_float(void);

pcq_parser_t *pcq_char_lit(void);
pcq_parser_t *pcq_string_lit(void);
pcq_parser_t *pcq_regex_lit(void);

pcq_parser_t *pcq_ident(void);

/*
** Useful Parsers
*/

pcq_parser_t *pcq_startwith(pcq_parser_t *a);
pcq_parser_t *pcq_endwith(pcq_parser_t *a, pcq_dtor_t da);
pcq_parser_t *pcq_whole(pcq_parser_t *a, pcq_dtor_t da);

pcq_parser_t *pcq_stripl(pcq_parser_t *a);
pcq_parser_t *pcq_stripr(pcq_parser_t *a);
pcq_parser_t *pcq_strip(pcq_parser_t *a);
pcq_parser_t *pcq_tok(pcq_parser_t *a);
pcq_parser_t *pcq_sym(const char *s);
pcq_parser_t *pcq_total(pcq_parser_t *a, pcq_dtor_t da);

pcq_parser_t *pcq_between(pcq_parser_t *a, pcq_dtor_t ad, const char *o, const char *c);
pcq_parser_t *pcq_parens(pcq_parser_t *a, pcq_dtor_t ad);
pcq_parser_t *pcq_braces(pcq_parser_t *a, pcq_dtor_t ad);
pcq_parser_t *pcq_brackets(pcq_parser_t *a, pcq_dtor_t ad);
pcq_parser_t *pcq_squares(pcq_parser_t *a, pcq_dtor_t ad);

pcq_parser_t *pcq_tok_between(pcq_parser_t *a, pcq_dtor_t ad, const char *o, const char *c);
pcq_parser_t *pcq_tok_parens(pcq_parser_t *a, pcq_dtor_t ad);
pcq_parser_t *pcq_tok_braces(pcq_parser_t *a, pcq_dtor_t ad);
pcq_parser_t *pcq_tok_brackets(pcq_parser_t *a, pcq_dtor_t ad);
pcq_parser_t *pcq_tok_squares(pcq_parser_t *a, pcq_dtor_t ad);

/*
** Common Function Parameters
*/

void pcqf_dtor_null(pcq_val_t *x);

pcq_val_t *pcqf_ctor_null(void);
pcq_val_t *pcqf_ctor_str(void);

pcq_val_t *pcqf_free(pcq_val_t *x);
pcq_val_t *pcqf_int(pcq_val_t *x);
pcq_val_t *pcqf_hex(pcq_val_t *x);
pcq_val_t *pcqf_oct(pcq_val_t *x);
pcq_val_t *pcqf_float(pcq_val_t *x);
pcq_val_t *pcqf_strtriml(pcq_val_t *x);
pcq_val_t *pcqf_strtrimr(pcq_val_t *x);
pcq_val_t *pcqf_strtrim(pcq_val_t *x);

pcq_val_t *pcqf_escape(pcq_val_t *x);
pcq_val_t *pcqf_escape_regex(pcq_val_t *x);
pcq_val_t *pcqf_escape_string_raw(pcq_val_t *x);
pcq_val_t *pcqf_escape_char_raw(pcq_val_t *x);

pcq_val_t *pcqf_unescape(pcq_val_t *x);
pcq_val_t *pcqf_unescape_regex(pcq_val_t *x);
pcq_val_t *pcqf_unescape_string_raw(pcq_val_t *x);
pcq_val_t *pcqf_unescape_char_raw(pcq_val_t *x);

pcq_val_t *pcqf_null(int n, pcq_val_t** xs);
pcq_val_t *pcqf_fst(int n, pcq_val_t** xs);
pcq_val_t *pcqf_snd(int n, pcq_val_t** xs);
pcq_val_t *pcqf_trd(int n, pcq_val_t** xs);

pcq_val_t *pcqf_fst_free(int n, pcq_val_t** xs);
pcq_val_t *pcqf_snd_free(int n, pcq_val_t** xs);
pcq_val_t *pcqf_trd_free(int n, pcq_val_t** xs);
pcq_val_t *pcqf_all_free(int n, pcq_val_t** xs);

pcq_val_t *pcqf_freefold(int n, pcq_val_t** xs);
pcq_val_t *pcqf_strfold(int n, pcq_val_t** xs);
pcq_val_t *pcqf_maths(int n, pcq_val_t** xs);

/*
** Regular Expression Parsers
*/

enum {
  PCQ_RE_DEFAULT   = 0,
  PCQ_RE_M         = 1,
  PCQ_RE_S         = 2,
  PCQ_RE_MULTILINE = 1,
  PCQ_RE_DOTALL    = 2
};

pcq_parser_t *pcq_re(const char *re);
pcq_parser_t *pcq_re_mode(const char *re, int mode);

/*
** AST
*/

typedef struct pcq_ast_t {
  char *tag;
  char *contents;
  pcq_state_t state;
  int children_num;
  struct pcq_ast_t** children;
} pcq_ast_t;

pcq_ast_t *pcq_ast_new(const char *tag, const char *contents);
pcq_ast_t *pcq_ast_build(int n, const char *tag, ...);
pcq_ast_t *pcq_ast_add_root(pcq_ast_t *a);
pcq_ast_t *pcq_ast_add_child(pcq_ast_t *r, pcq_ast_t *a);
pcq_ast_t *pcq_ast_add_tag(pcq_ast_t *a, const char *t);
pcq_ast_t *pcq_ast_add_root_tag(pcq_ast_t *a, const char *t);
pcq_ast_t *pcq_ast_tag(pcq_ast_t *a, const char *t);
pcq_ast_t *pcq_ast_state(pcq_ast_t *a, pcq_state_t s);

void pcq_ast_delete(pcq_ast_t *a);
void pcq_ast_print(pcq_ast_t *a);
void pcq_ast_print_to(pcq_ast_t *a, FILE *fp);

int pcq_ast_get_index(pcq_ast_t *ast, const char *tag);
int pcq_ast_get_index_lb(pcq_ast_t *ast, const char *tag, int lb);
pcq_ast_t *pcq_ast_get_child(pcq_ast_t *ast, const char *tag);
pcq_ast_t *pcq_ast_get_child_lb(pcq_ast_t *ast, const char *tag, int lb);

typedef enum {
  pcq_ast_trav_order_pre,
  pcq_ast_trav_order_post
} pcq_ast_trav_order_t;

typedef struct pcq_ast_trav_t {
  pcq_ast_t             *curr_node;
  struct pcq_ast_trav_t *parent;
  int                    curr_child;
  pcq_ast_trav_order_t   order;
} pcq_ast_trav_t;

pcq_ast_trav_t *pcq_ast_traverse_start(pcq_ast_t *ast,
                                       pcq_ast_trav_order_t order);

pcq_ast_t *pcq_ast_traverse_next(pcq_ast_trav_t **trav);

void pcq_ast_traverse_free(pcq_ast_trav_t **trav);

/*
** Warning: This function currently doesn't test for equality of the `state` member!
*/
int pcq_ast_eq(pcq_ast_t *a, pcq_ast_t *b);

pcq_val_t *pcqf_fold_ast(int n, pcq_val_t **as);
pcq_val_t *pcqf_str_ast(pcq_val_t *c);
pcq_val_t *pcqf_state_ast(int n, pcq_val_t **xs);

pcq_parser_t *pcqa_tag(pcq_parser_t *a, const char *t);
pcq_parser_t *pcqa_add_tag(pcq_parser_t *a, const char *t);
pcq_parser_t *pcqa_root(pcq_parser_t *a);
pcq_parser_t *pcqa_state(pcq_parser_t *a);
pcq_parser_t *pcqa_total(pcq_parser_t *a);

pcq_parser_t *pcqa_not(pcq_parser_t *a);
pcq_parser_t *pcqa_maybe(pcq_parser_t *a);

pcq_parser_t *pcqa_many(pcq_parser_t *a);
pcq_parser_t *pcqa_many1(pcq_parser_t *a);
pcq_parser_t *pcqa_count(int n, pcq_parser_t *a);

pcq_parser_t *pcqa_or(int n, ...);
pcq_parser_t *pcqa_and(int n, ...);

enum {
  PCQA_LANG_DEFAULT              = 0,
  PCQA_LANG_PREDICTIVE           = 1,
  PCQA_LANG_WHITESPACE_SENSITIVE = 2
};

pcq_parser_t *pcqa_grammar(int flags, const char *grammar, ...);

pcq_err_t *pcqa_lang(int flags, const char *language, ...);
pcq_err_t *pcqa_lang_file(int flags, FILE *f, ...);
pcq_err_t *pcqa_lang_pipe(int flags, FILE *f, ...);
pcq_err_t *pcqa_lang_contents(int flags, const char *filename, ...);

/*
** Misc
*/


void pcq_print(pcq_parser_t *p);
void pcq_optimise(pcq_parser_t *p);
void pcq_stats(pcq_parser_t *p);

int pcq_test_pass(pcq_parser_t *p, const char *s, const void *d,
  int(*tester)(const void*, const void*),
  pcq_dtor_t destructor,
  void(*printer)(const void*));

int pcq_test_fail(pcq_parser_t *p, const char *s, const void *d,
  int(*tester)(const void*, const void*),
  pcq_dtor_t destructor,
  void(*printer)(const void*));

#ifdef __cplusplus
}
#endif

#endif
