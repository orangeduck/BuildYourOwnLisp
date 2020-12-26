#include "pcq.h"

/*
** State Type
*/

static pcq_state_t pcq_state_invalid(void) {
  pcq_state_t s;
  s.pos = -1;
  s.row = -1;
  s.col = -1;
  s.term = 0;
  return s;
}

static pcq_state_t pcq_state_new(void) {
  pcq_state_t s;
  s.pos = 0;
  s.row = 0;
  s.col = 0;
  s.term = 0;
  return s;
}

/*
** Input Type
*/

/*
** In pcq the input type has three modes of
** operation: String, File and Pipe.
**
** String is easy. The whole contents are
** loaded into a buffer and scanned through.
** The cursor can jump around at will making
** backtracking easy.
**
** The second is a File which is also somewhat
** easy. The contents are never loaded into
** memory but backtracking can still be achieved
** by seeking in the file at different positions.
**
** The final mode is Pipe. This is the difficult
** one. As we assume pipes cannot be seeked - and
** only support a single character lookahead at
** any point, when the input is marked for a
** potential backtracking we start buffering any
** input.
**
** This means that if we are requested to seek
** back we can simply start reading from the
** buffer instead of the input.
**
** Of course using `pcq_predictive` will disable
** backtracking and make LL(1) grammars easy
** to parse for all input methods.
**
*/

enum {
  PCQ_INPUT_STRING = 0,
  PCQ_INPUT_FILE   = 1,
  PCQ_INPUT_PIPE   = 2
};

enum {
  PCQ_INPUT_MARKS_MIN = 32
};

enum {
  PCQ_INPUT_MEM_NUM = 512
};

typedef struct {
  char mem[64];
} pcq_mem_t;

typedef struct {

  int type;
  char *filename;
  pcq_state_t state;

  char *string;
  char *buffer;
  FILE *file;

  int suppress;
  int backtrack;
  int marks_slots;
  int marks_num;
  pcq_state_t *marks;

  char *lasts;
  char last;

  size_t mem_index;
  char mem_full[PCQ_INPUT_MEM_NUM];
  pcq_mem_t mem[PCQ_INPUT_MEM_NUM];

} pcq_input_t;

static pcq_input_t *pcq_input_new_string(const char *filename, const char *string) {

  pcq_input_t *i = malloc(sizeof(pcq_input_t));

  i->filename = malloc(strlen(filename) + 1);
  strcpy(i->filename, filename);
  i->type = PCQ_INPUT_STRING;

  i->state = pcq_state_new();

  i->string = malloc(strlen(string) + 1);
  strcpy(i->string, string);
  i->buffer = NULL;
  i->file = NULL;

  i->suppress = 0;
  i->backtrack = 1;
  i->marks_num = 0;
  i->marks_slots = PCQ_INPUT_MARKS_MIN;
  i->marks = malloc(sizeof(pcq_state_t) * i->marks_slots);
  i->lasts = malloc(sizeof(char) * i->marks_slots);
  i->last = '\0';

  i->mem_index = 0;
  memset(i->mem_full, 0, sizeof(char) * PCQ_INPUT_MEM_NUM);

  return i;
}

static pcq_input_t *pcq_input_new_nstring(const char *filename, const char *string, size_t length) {

  pcq_input_t *i = malloc(sizeof(pcq_input_t));

  i->filename = malloc(strlen(filename) + 1);
  strcpy(i->filename, filename);
  i->type = PCQ_INPUT_STRING;

  i->state = pcq_state_new();

  i->string = malloc(length + 1);
  strncpy(i->string, string, length);
  i->string[length] = '\0';
  i->buffer = NULL;
  i->file = NULL;

  i->suppress = 0;
  i->backtrack = 1;
  i->marks_num = 0;
  i->marks_slots = PCQ_INPUT_MARKS_MIN;
  i->marks = malloc(sizeof(pcq_state_t) * i->marks_slots);
  i->lasts = malloc(sizeof(char) * i->marks_slots);
  i->last = '\0';

  i->mem_index = 0;
  memset(i->mem_full, 0, sizeof(char) * PCQ_INPUT_MEM_NUM);

  return i;

}

static pcq_input_t *pcq_input_new_pipe(const char *filename, FILE *pipe) {

  pcq_input_t *i = malloc(sizeof(pcq_input_t));

  i->filename = malloc(strlen(filename) + 1);
  strcpy(i->filename, filename);

  i->type = PCQ_INPUT_PIPE;
  i->state = pcq_state_new();

  i->string = NULL;
  i->buffer = NULL;
  i->file = pipe;

  i->suppress = 0;
  i->backtrack = 1;
  i->marks_num = 0;
  i->marks_slots = PCQ_INPUT_MARKS_MIN;
  i->marks = malloc(sizeof(pcq_state_t) * i->marks_slots);
  i->lasts = malloc(sizeof(char) * i->marks_slots);
  i->last = '\0';

  i->mem_index = 0;
  memset(i->mem_full, 0, sizeof(char) * PCQ_INPUT_MEM_NUM);

  return i;

}

static pcq_input_t *pcq_input_new_file(const char *filename, FILE *file) {

  pcq_input_t *i = malloc(sizeof(pcq_input_t));

  i->filename = malloc(strlen(filename) + 1);
  strcpy(i->filename, filename);
  i->type = PCQ_INPUT_FILE;
  i->state = pcq_state_new();

  i->string = NULL;
  i->buffer = NULL;
  i->file = file;

  i->suppress = 0;
  i->backtrack = 1;
  i->marks_num = 0;
  i->marks_slots = PCQ_INPUT_MARKS_MIN;
  i->marks = malloc(sizeof(pcq_state_t) * i->marks_slots);
  i->lasts = malloc(sizeof(char) * i->marks_slots);
  i->last = '\0';

  i->mem_index = 0;
  memset(i->mem_full, 0, sizeof(char) * PCQ_INPUT_MEM_NUM);

  return i;
}

static void pcq_input_delete(pcq_input_t *i) {

  free(i->filename);

  if (i->type == PCQ_INPUT_STRING) { free(i->string); }
  if (i->type == PCQ_INPUT_PIPE) { free(i->buffer); }

  free(i->marks);
  free(i->lasts);
  free(i);
}

static int pcq_mem_ptr(pcq_input_t *i, void *p) {
  return
    (char*)p >= (char*)(i->mem) &&
    (char*)p <  (char*)(i->mem) + (PCQ_INPUT_MEM_NUM * sizeof(pcq_mem_t));
}

static void *pcq_malloc(pcq_input_t *i, size_t n) {
  size_t j;
  char *p;

  if (n > sizeof(pcq_mem_t)) { return malloc(n); }

  j = i->mem_index;
  do {
    if (!i->mem_full[i->mem_index]) {
      p = (void*)(i->mem + i->mem_index);
      i->mem_full[i->mem_index] = 1;
      i->mem_index = (i->mem_index+1) % PCQ_INPUT_MEM_NUM;
      return p;
    }
    i->mem_index = (i->mem_index+1) % PCQ_INPUT_MEM_NUM;
  } while (j != i->mem_index);

  return malloc(n);
}

static void *pcq_calloc(pcq_input_t *i, size_t n, size_t m) {
  char *x = pcq_malloc(i, n * m);
  memset(x, 0, n * m);
  return x;
}

static void pcq_free(pcq_input_t *i, void *p) {
  size_t j;
  if (!pcq_mem_ptr(i, p)) { free(p); return; }
  j = ((size_t)(((char*)p) - ((char*)i->mem))) / sizeof(pcq_mem_t);
  i->mem_full[j] = 0;
}

static void *pcq_realloc(pcq_input_t *i, void *p, size_t n) {

  char *q = NULL;

  if (!pcq_mem_ptr(i, p)) { return realloc(p, n); }

  if (n > sizeof(pcq_mem_t)) {
    q = malloc(n);
    memcpy(q, p, sizeof(pcq_mem_t));
    pcq_free(i, p);
    return q;
  }

  return p;
}

static void *pcq_export(pcq_input_t *i, void *p) {
  char *q = NULL;
  if (!pcq_mem_ptr(i, p)) { return p; }
  q = malloc(sizeof(pcq_mem_t));
  memcpy(q, p, sizeof(pcq_mem_t));
  pcq_free(i, p);
  return q;
}

static void pcq_input_backtrack_disable(pcq_input_t *i) { i->backtrack--; }
static void pcq_input_backtrack_enable(pcq_input_t *i) { i->backtrack++; }

static void pcq_input_suppress_disable(pcq_input_t *i) { i->suppress--; }
static void pcq_input_suppress_enable(pcq_input_t *i) { i->suppress++; }

static void pcq_input_mark(pcq_input_t *i) {

  if (i->backtrack < 1) { return; }

  i->marks_num++;

  if (i->marks_num > i->marks_slots) {
    i->marks_slots = i->marks_num + i->marks_num / 2;
    i->marks = realloc(i->marks, sizeof(pcq_state_t) * i->marks_slots);
    i->lasts = realloc(i->lasts, sizeof(char) * i->marks_slots);
  }

  i->marks[i->marks_num-1] = i->state;
  i->lasts[i->marks_num-1] = i->last;

  if (i->type == PCQ_INPUT_PIPE && i->marks_num == 1) {
    i->buffer = calloc(1, 1);
  }

}

static void pcq_input_unmark(pcq_input_t *i) {
  int j;

  if (i->backtrack < 1) { return; }

  i->marks_num--;

  if (i->marks_slots > i->marks_num + i->marks_num / 2
  &&  i->marks_slots > PCQ_INPUT_MARKS_MIN) {
    i->marks_slots =
      i->marks_num > PCQ_INPUT_MARKS_MIN ?
      i->marks_num : PCQ_INPUT_MARKS_MIN;
    i->marks = realloc(i->marks, sizeof(pcq_state_t) * i->marks_slots);
    i->lasts = realloc(i->lasts, sizeof(char) * i->marks_slots);
  }

  if (i->type == PCQ_INPUT_PIPE && i->marks_num == 0) {
    for (j = strlen(i->buffer) - 1; j >= 0; j--)
      ungetc(i->buffer[j], i->file);

    free(i->buffer);
    i->buffer = NULL;
  }

}

static void pcq_input_rewind(pcq_input_t *i) {

  if (i->backtrack < 1) { return; }

  i->state = i->marks[i->marks_num-1];
  i->last  = i->lasts[i->marks_num-1];

  if (i->type == PCQ_INPUT_FILE) {
    fseek(i->file, i->state.pos, SEEK_SET);
  }

  pcq_input_unmark(i);
}

static int pcq_input_buffer_in_range(pcq_input_t *i) {
  return i->state.pos < (long)(strlen(i->buffer) + i->marks[0].pos);
}

static char pcq_input_buffer_get(pcq_input_t *i) {
  return i->buffer[i->state.pos - i->marks[0].pos];
}

static char pcq_input_getc(pcq_input_t *i) {

  char c = '\0';

  switch (i->type) {

    case PCQ_INPUT_STRING: return i->string[i->state.pos];
    case PCQ_INPUT_FILE: c = fgetc(i->file); return c;
    case PCQ_INPUT_PIPE:

      if (!i->buffer) { c = getc(i->file); return c; }

      if (i->buffer && pcq_input_buffer_in_range(i)) {
        c = pcq_input_buffer_get(i);
        return c;
      } else {
        c = getc(i->file);
        return c;
      }

    default: return c;
  }
}

static char pcq_input_peekc(pcq_input_t *i) {

  char c = '\0';

  switch (i->type) {
    case PCQ_INPUT_STRING: return i->string[i->state.pos];
    case PCQ_INPUT_FILE:

      c = fgetc(i->file);
      if (feof(i->file)) { return '\0'; }

      fseek(i->file, -1, SEEK_CUR);
      return c;

    case PCQ_INPUT_PIPE:

      if (!i->buffer) {
        c = getc(i->file);
        if (feof(i->file)) { return '\0'; }
        ungetc(c, i->file);
        return c;
      }

      if (i->buffer && pcq_input_buffer_in_range(i)) {
        return pcq_input_buffer_get(i);
      } else {
        c = getc(i->file);
        if (feof(i->file)) { return '\0'; }
        ungetc(c, i->file);
        return c;
      }

    default: return c;
  }

}

static int pcq_input_terminated(pcq_input_t *i) {
  return pcq_input_peekc(i) == '\0';
}

static int pcq_input_failure(pcq_input_t *i, char c) {

  switch (i->type) {
    case PCQ_INPUT_STRING: { break; }
    case PCQ_INPUT_FILE: fseek(i->file, -1, SEEK_CUR); { break; }
    case PCQ_INPUT_PIPE: {

      if (!i->buffer) { ungetc(c, i->file); break; }

      if (i->buffer && pcq_input_buffer_in_range(i)) {
        break;
      } else {
        ungetc(c, i->file);
      }
    }
    default: { break; }
  }
  return 0;
}

static int pcq_input_success(pcq_input_t *i, char c, char **o) {

  if (i->type == PCQ_INPUT_PIPE
  &&  i->buffer && !pcq_input_buffer_in_range(i)) {
    i->buffer = realloc(i->buffer, strlen(i->buffer) + 2);
    i->buffer[strlen(i->buffer) + 1] = '\0';
    i->buffer[strlen(i->buffer) + 0] = c;
  }

  i->last = c;
  i->state.pos++;
  i->state.col++;

  if (c == '\n') {
    i->state.col = 0;
    i->state.row++;
  }

  if (o) {
    (*o) = pcq_malloc(i, 2);
    (*o)[0] = c;
    (*o)[1] = '\0';
  }

  return 1;
}

static int pcq_input_any(pcq_input_t *i, char **o) {
  char x;
  if (pcq_input_terminated(i)) { return 0; }
  x = pcq_input_getc(i);
  return pcq_input_success(i, x, o);
}

static int pcq_input_char(pcq_input_t *i, char c, char **o) {
  char x;
  if (pcq_input_terminated(i)) { return 0; }
  x = pcq_input_getc(i);
  return x == c ? pcq_input_success(i, x, o) : pcq_input_failure(i, x);
}

static int pcq_input_range(pcq_input_t *i, char c, char d, char **o) {
  char x;
  if (pcq_input_terminated(i)) { return 0; }
  x = pcq_input_getc(i);
  return x >= c && x <= d ? pcq_input_success(i, x, o) : pcq_input_failure(i, x);
}

static int pcq_input_oneof(pcq_input_t *i, const char *c, char **o) {
  char x;
  if (pcq_input_terminated(i)) { return 0; }
  x = pcq_input_getc(i);
  return strchr(c, x) != 0 ? pcq_input_success(i, x, o) : pcq_input_failure(i, x);
}

static int pcq_input_noneof(pcq_input_t *i, const char *c, char **o) {
  char x;
  if (pcq_input_terminated(i)) { return 0; }
  x = pcq_input_getc(i);
  return strchr(c, x) == 0 ? pcq_input_success(i, x, o) : pcq_input_failure(i, x);
}

static int pcq_input_satisfy(pcq_input_t *i, int(*cond)(char), char **o) {
  char x;
  if (pcq_input_terminated(i)) { return 0; }
  x = pcq_input_getc(i);
  return cond(x) ? pcq_input_success(i, x, o) : pcq_input_failure(i, x);
}

static int pcq_input_string(pcq_input_t *i, const char *c, char **o) {

  const char *x = c;

  pcq_input_mark(i);
  while (*x) {
    if (!pcq_input_char(i, *x, NULL)) {
      pcq_input_rewind(i);
      return 0;
    }
    x++;
  }
  pcq_input_unmark(i);

  *o = pcq_malloc(i, strlen(c) + 1);
  strcpy(*o, c);
  return 1;
}

static int pcq_input_anchor(pcq_input_t* i, int(*f)(char,char), char **o) {
  *o = NULL;
  return f(i->last, pcq_input_peekc(i));
}

static int pcq_input_soi(pcq_input_t* i, char **o) {
  *o = NULL;
  return i->last == '\0';
}

static int pcq_input_eoi(pcq_input_t* i, char **o) {
  *o = NULL;
  if (i->state.term) {
    return 0;
  } else if (pcq_input_terminated(i)) {
    i->state.term = 1;
    return 1;
  } else {
    return 0;
  }
}

static pcq_state_t *pcq_input_state_copy(pcq_input_t *i) {
  pcq_state_t *r = pcq_malloc(i, sizeof(pcq_state_t));
  memcpy(r, &i->state, sizeof(pcq_state_t));
  return r;
}

/*
** Error Type
*/

void pcq_err_delete(pcq_err_t *x) {
  int i;
  for (i = 0; i < x->expected_num; i++) { free(x->expected[i]); }
  free(x->expected);
  free(x->filename);
  free(x->failure);
  free(x);
}

void pcq_err_print(pcq_err_t *x) {
  pcq_err_print_to(x, stdout);
}

void pcq_err_print_to(pcq_err_t *x, FILE *f) {
  char *str = pcq_err_string(x);
  fprintf(f, "%s", str);
  free(str);
}

static void pcq_err_string_cat(char *buffer, int *pos, int *max, char const *fmt, ...) {
  /* TODO: Error Checking on Length */
  int left = ((*max) - (*pos));
  va_list va;
  va_start(va, fmt);
  if (left < 0) { left = 0;}
  (*pos) += vsprintf(buffer + (*pos), fmt, va);
  va_end(va);
}

static char char_unescape_buffer[4];

static const char *pcq_err_char_unescape(char c) {

  char_unescape_buffer[0] = '\'';
  char_unescape_buffer[1] = ' ';
  char_unescape_buffer[2] = '\'';
  char_unescape_buffer[3] = '\0';

  switch (c) {
    case '\a': return "bell";
    case '\b': return "backspace";
    case '\f': return "formfeed";
    case '\r': return "carriage return";
    case '\v': return "vertical tab";
    case '\0': return "end of input";
    case '\n': return "newline";
    case '\t': return "tab";
    case ' ' : return "space";
    default:
      char_unescape_buffer[1] = c;
      return char_unescape_buffer;
  }

}

char *pcq_err_string(pcq_err_t *x) {

  int i;
  int pos = 0;
  int max = 1023;
  char *buffer = calloc(1, 1024);

  if (x->failure) {
    pcq_err_string_cat(buffer, &pos, &max,
    "%s: error: %s\n", x->filename, x->failure);
    return buffer;
  }

  pcq_err_string_cat(buffer, &pos, &max,
    "%s:%li:%li: error: expected ", x->filename, x->state.row+1, x->state.col+1);

  if (x->expected_num == 0) { pcq_err_string_cat(buffer, &pos, &max, "ERROR: NOTHING EXPECTED"); }
  if (x->expected_num == 1) { pcq_err_string_cat(buffer, &pos, &max, "%s", x->expected[0]); }
  if (x->expected_num >= 2) {

    for (i = 0; i < x->expected_num-2; i++) {
      pcq_err_string_cat(buffer, &pos, &max, "%s, ", x->expected[i]);
    }

    pcq_err_string_cat(buffer, &pos, &max, "%s or %s",
      x->expected[x->expected_num-2],
      x->expected[x->expected_num-1]);
  }

  pcq_err_string_cat(buffer, &pos, &max, " at ");
  pcq_err_string_cat(buffer, &pos, &max, pcq_err_char_unescape(x->received));
  pcq_err_string_cat(buffer, &pos, &max, "\n");

  return realloc(buffer, strlen(buffer) + 1);
}

static pcq_err_t *pcq_err_new(pcq_input_t *i, const char *expected) {
  pcq_err_t *x;
  if (i->suppress) { return NULL; }
  x = pcq_malloc(i, sizeof(pcq_err_t));
  x->filename = pcq_malloc(i, strlen(i->filename) + 1);
  strcpy(x->filename, i->filename);
  x->state = i->state;
  x->expected_num = 1;
  x->expected = pcq_malloc(i, sizeof(char*));
  x->expected[0] = pcq_malloc(i, strlen(expected) + 1);
  strcpy(x->expected[0], expected);
  x->failure = NULL;
  x->received = pcq_input_peekc(i);
  return x;
}

static pcq_err_t *pcq_err_fail(pcq_input_t *i, const char *failure) {
  pcq_err_t *x;
  if (i->suppress) { return NULL; }
  x = pcq_malloc(i, sizeof(pcq_err_t));
  x->filename = pcq_malloc(i, strlen(i->filename) + 1);
  strcpy(x->filename, i->filename);
  x->state = i->state;
  x->expected_num = 0;
  x->expected = NULL;
  x->failure = pcq_malloc(i, strlen(failure) + 1);
  strcpy(x->failure, failure);
  x->received = ' ';
  return x;
}

static pcq_err_t *pcq_err_file(const char *filename, const char *failure) {
  pcq_err_t *x;
  x = malloc(sizeof(pcq_err_t));
  x->filename = malloc(strlen(filename) + 1);
  strcpy(x->filename, filename);
  x->state = pcq_state_new();
  x->expected_num = 0;
  x->expected = NULL;
  x->failure = malloc(strlen(failure) + 1);
  strcpy(x->failure, failure);
  x->received = ' ';
  return x;
}

static void pcq_err_delete_internal(pcq_input_t *i, pcq_err_t *x) {
  int j;
  if (x == NULL) { return; }
  for (j = 0; j < x->expected_num; j++) { pcq_free(i, x->expected[j]); }
  pcq_free(i, x->expected);
  pcq_free(i, x->filename);
  pcq_free(i, x->failure);
  pcq_free(i, x);
}

static pcq_err_t *pcq_err_export(pcq_input_t *i, pcq_err_t *x) {
  int j;
  for (j = 0; j < x->expected_num; j++) {
    x->expected[j] = pcq_export(i, x->expected[j]);
  }
  x->expected = pcq_export(i, x->expected);
  x->filename = pcq_export(i, x->filename);
  x->failure = pcq_export(i, x->failure);
  return pcq_export(i, x);
}

static int pcq_err_contains_expected(pcq_input_t *i, pcq_err_t *x, char *expected) {
  int j;
  (void)i;
  for (j = 0; j < x->expected_num; j++) {
    if (strcmp(x->expected[j], expected) == 0) { return 1; }
  }
  return 0;
}

static void pcq_err_add_expected(pcq_input_t *i, pcq_err_t *x, char *expected) {
  (void)i;
  x->expected_num++;
  x->expected = pcq_realloc(i, x->expected, sizeof(char*) * x->expected_num);
  x->expected[x->expected_num-1] = pcq_malloc(i, strlen(expected) + 1);
  strcpy(x->expected[x->expected_num-1], expected);
}

static pcq_err_t *pcq_err_or(pcq_input_t *i, pcq_err_t** x, int n) {

  int j, k, fst;
  pcq_err_t *e;

  fst = -1;
  for (j = 0; j < n; j++) {
    if (x[j] != NULL) { fst = j; }
  }

  if (fst == -1) { return NULL; }

  e = pcq_malloc(i, sizeof(pcq_err_t));
  e->state = pcq_state_invalid();
  e->expected_num = 0;
  e->expected = NULL;
  e->failure = NULL;
  e->filename = pcq_malloc(i, strlen(x[fst]->filename)+1);
  strcpy(e->filename, x[fst]->filename);

  for (j = 0; j < n; j++) {
    if (x[j] == NULL) { continue; }
    if (x[j]->state.pos > e->state.pos) { e->state = x[j]->state; }
  }

  for (j = 0; j < n; j++) {
    if (x[j] == NULL) { continue; }
    if (x[j]->state.pos < e->state.pos) { continue; }

    if (x[j]->failure) {
      e->failure = pcq_malloc(i, strlen(x[j]->failure)+1);
      strcpy(e->failure, x[j]->failure);
      break;
    }

    e->received = x[j]->received;

    for (k = 0; k < x[j]->expected_num; k++) {
      if (!pcq_err_contains_expected(i, e, x[j]->expected[k])) {
        pcq_err_add_expected(i, e, x[j]->expected[k]);
      }
    }
  }

  for (j = 0; j < n; j++) {
    if (x[j] == NULL) { continue; }
    pcq_err_delete_internal(i, x[j]);
  }

  return e;
}

static pcq_err_t *pcq_err_repeat(pcq_input_t *i, pcq_err_t *x, const char *prefix) {

  int j = 0;
  size_t l = 0;
  char *expect = NULL;

  if (x == NULL) { return NULL; }

  if (x->expected_num == 0) {
    expect = pcq_calloc(i, 1, 1);
    x->expected_num = 1;
    x->expected = pcq_realloc(i, x->expected, sizeof(char*) * x->expected_num);
    x->expected[0] = expect;
    return x;
  }

  else if (x->expected_num == 1) {
    expect = pcq_malloc(i, strlen(prefix) + strlen(x->expected[0]) + 1);
    strcpy(expect, prefix);
    strcat(expect, x->expected[0]);
    pcq_free(i, x->expected[0]);
    x->expected[0] = expect;
    return x;
  }

  else if (x->expected_num > 1) {

    l += strlen(prefix);
    for (j = 0; j < x->expected_num-2; j++) {
      l += strlen(x->expected[j]) + strlen(", ");
    }
    l += strlen(x->expected[x->expected_num-2]);
    l += strlen(" or ");
    l += strlen(x->expected[x->expected_num-1]);

    expect = pcq_malloc(i, l + 1);

    strcpy(expect, prefix);
    for (j = 0; j < x->expected_num-2; j++) {
      strcat(expect, x->expected[j]); strcat(expect, ", ");
    }
    strcat(expect, x->expected[x->expected_num-2]);
    strcat(expect, " or ");
    strcat(expect, x->expected[x->expected_num-1]);

    for (j = 0; j < x->expected_num; j++) { pcq_free(i, x->expected[j]); }

    x->expected_num = 1;
    x->expected = pcq_realloc(i, x->expected, sizeof(char*) * x->expected_num);
    x->expected[0] = expect;
    return x;
  }

  return NULL;
}

static pcq_err_t *pcq_err_many1(pcq_input_t *i, pcq_err_t *x) {
  return pcq_err_repeat(i, x, "one or more of ");
}

static pcq_err_t *pcq_err_count(pcq_input_t *i, pcq_err_t *x, int n) {
  pcq_err_t *y;
  int digits = n/10 + 1;
  char *prefix;
  prefix = pcq_malloc(i, digits + strlen(" of ") + 1);
  sprintf(prefix, "%i of ", n);
  y = pcq_err_repeat(i, x, prefix);
  pcq_free(i, prefix);
  return y;
}

static pcq_err_t *pcq_err_merge(pcq_input_t *i, pcq_err_t *x, pcq_err_t *y) {
  pcq_err_t *errs[2];
  errs[0] = x;
  errs[1] = y;
  return pcq_err_or(i, errs, 2);
}

/*
** Parser Type
*/

enum {
  PCQ_TYPE_UNDEFINED  = 0,
  PCQ_TYPE_PASS       = 1,
  PCQ_TYPE_FAIL       = 2,
  PCQ_TYPE_LIFT       = 3,
  PCQ_TYPE_LIFT_VAL   = 4,
  PCQ_TYPE_EXPECT     = 5,
  PCQ_TYPE_ANCHOR     = 6,
  PCQ_TYPE_STATE      = 7,

  PCQ_TYPE_ANY        = 8,
  PCQ_TYPE_SINGLE     = 9,
  PCQ_TYPE_ONEOF      = 10,
  PCQ_TYPE_NONEOF     = 11,
  PCQ_TYPE_RANGE      = 12,
  PCQ_TYPE_SATISFY    = 13,
  PCQ_TYPE_STRING     = 14,

  PCQ_TYPE_APPLY      = 15,
  PCQ_TYPE_APPLY_TO   = 16,
  PCQ_TYPE_PREDICT    = 17,
  PCQ_TYPE_NOT        = 18,
  PCQ_TYPE_MAYBE      = 19,
  PCQ_TYPE_MANY       = 20,
  PCQ_TYPE_MANY1      = 21,
  PCQ_TYPE_COUNT      = 22,

  PCQ_TYPE_OR         = 23,
  PCQ_TYPE_AND        = 24,

  PCQ_TYPE_CHECK      = 25,
  PCQ_TYPE_CHECK_WITH = 26,

  PCQ_TYPE_SOI        = 27,
  PCQ_TYPE_EOI        = 28
};

typedef struct { char *m; } pcq_pdata_fail_t;
typedef struct { pcq_ctor_t lf; void *x; } pcq_pdata_lift_t;
typedef struct { pcq_parser_t *x; char *m; } pcq_pdata_expect_t;
typedef struct { int(*f)(char,char); } pcq_pdata_anchor_t;
typedef struct { char x; } pcq_pdata_single_t;
typedef struct { char x; char y; } pcq_pdata_range_t;
typedef struct { int(*f)(char); } pcq_pdata_satisfy_t;
typedef struct { char *x; } pcq_pdata_string_t;
typedef struct { pcq_parser_t *x; pcq_apply_t f; } pcq_pdata_apply_t;
typedef struct { pcq_parser_t *x; pcq_apply_to_t f; void *d; } pcq_pdata_apply_to_t;
typedef struct { pcq_parser_t *x; pcq_dtor_t dx; pcq_check_t f; char *e; } pcq_pdata_check_t;
typedef struct { pcq_parser_t *x; pcq_dtor_t dx; pcq_check_with_t f; void *d; char *e; } pcq_pdata_check_with_t;
typedef struct { pcq_parser_t *x; } pcq_pdata_predict_t;
typedef struct { pcq_parser_t *x; pcq_dtor_t dx; pcq_ctor_t lf; } pcq_pdata_not_t;
typedef struct { int n; pcq_fold_t f; pcq_parser_t *x; pcq_dtor_t dx; } pcq_pdata_repeat_t;
typedef struct { int n; pcq_parser_t **xs; } pcq_pdata_or_t;
typedef struct { int n; pcq_fold_t f; pcq_parser_t **xs; pcq_dtor_t *dxs;  } pcq_pdata_and_t;

typedef union {
  pcq_pdata_fail_t fail;
  pcq_pdata_lift_t lift;
  pcq_pdata_expect_t expect;
  pcq_pdata_anchor_t anchor;
  pcq_pdata_single_t single;
  pcq_pdata_range_t range;
  pcq_pdata_satisfy_t satisfy;
  pcq_pdata_string_t string;
  pcq_pdata_apply_t apply;
  pcq_pdata_apply_to_t apply_to;
  pcq_pdata_check_t check;
  pcq_pdata_check_with_t check_with;
  pcq_pdata_predict_t predict;
  pcq_pdata_not_t not;
  pcq_pdata_repeat_t repeat;
  pcq_pdata_and_t and;
  pcq_pdata_or_t or;
} pcq_pdata_t;

struct pcq_parser_t {
  char *name;
  pcq_pdata_t data;
  char type;
  char retained;
};

static pcq_val_t *pcqf_input_nth_free(pcq_input_t *i, int n, pcq_val_t **xs, int x) {
  int j;
  for (j = 0; j < n; j++) { if (j != x) { pcq_free(i, xs[j]); } }
  return xs[x];
}

static pcq_val_t *pcqf_input_fst_free(pcq_input_t *i, int n, pcq_val_t **xs) { return pcqf_input_nth_free(i, n, xs, 0); }
static pcq_val_t *pcqf_input_snd_free(pcq_input_t *i, int n, pcq_val_t **xs) { return pcqf_input_nth_free(i, n, xs, 1); }
static pcq_val_t *pcqf_input_trd_free(pcq_input_t *i, int n, pcq_val_t **xs) { return pcqf_input_nth_free(i, n, xs, 2); }

static pcq_val_t *pcqf_input_strfold(pcq_input_t *i, int n, pcq_val_t **xs) {
  int j;
  size_t l = 0;
  if (n == 0) { return pcq_calloc(i, 1, 1); }
  for (j = 0; j < n; j++) { l += strlen(xs[j]); }
  xs[0] = pcq_realloc(i, xs[0], l + 1);
  for (j = 1; j < n; j++) { strcat(xs[0], xs[j]); pcq_free(i, xs[j]); }
  return xs[0];
}

static pcq_val_t *pcqf_input_state_ast(pcq_input_t *i, int n, pcq_val_t **xs) {
  pcq_state_t *s = ((pcq_state_t**)xs)[0];
  pcq_ast_t *a = ((pcq_ast_t**)xs)[1];
  a = pcq_ast_state(a, *s);
  pcq_free(i, s);
  (void) n;
  return a;
}

static pcq_val_t *pcq_parse_fold(pcq_input_t *i, pcq_fold_t f, int n, pcq_val_t **xs) {
  int j;
  if (f == pcqf_null)      { return pcqf_null(n, xs); }
  if (f == pcqf_fst)       { return pcqf_fst(n, xs); }
  if (f == pcqf_snd)       { return pcqf_snd(n, xs); }
  if (f == pcqf_trd)       { return pcqf_trd(n, xs); }
  if (f == pcqf_fst_free)  { return pcqf_input_fst_free(i, n, xs); }
  if (f == pcqf_snd_free)  { return pcqf_input_snd_free(i, n, xs); }
  if (f == pcqf_trd_free)  { return pcqf_input_trd_free(i, n, xs); }
  if (f == pcqf_strfold)   { return pcqf_input_strfold(i, n, xs); }
  if (f == pcqf_state_ast) { return pcqf_input_state_ast(i, n, xs); }
  for (j = 0; j < n; j++) { xs[j] = pcq_export(i, xs[j]); }
  return f(j, xs);
}

static pcq_val_t *pcqf_input_free(pcq_input_t *i, pcq_val_t *x) {
  pcq_free(i, x);
  return NULL;
}

static pcq_val_t *pcqf_input_str_ast(pcq_input_t *i, pcq_val_t *c) {
  pcq_ast_t *a = pcq_ast_new("", c);
  pcq_free(i, c);
  return a;
}

static pcq_val_t *pcq_parse_apply(pcq_input_t *i, pcq_apply_t f, pcq_val_t *x) {
  if (f == pcqf_free)     { return pcqf_input_free(i, x); }
  if (f == pcqf_str_ast)  { return pcqf_input_str_ast(i, x); }
  return f(pcq_export(i, x));
}

static pcq_val_t *pcq_parse_apply_to(pcq_input_t *i, pcq_apply_to_t f, pcq_val_t *x, pcq_val_t *d) {
  return f(pcq_export(i, x), d);
}

static void pcq_parse_dtor(pcq_input_t *i, pcq_dtor_t d, pcq_val_t *x) {
  if (d == free) { pcq_free(i, x); return; }
  d(pcq_export(i, x));
}

enum {
  PCQ_PARSE_STACK_MIN = 4
};

#define PCQ_SUCCESS(x) r->output = x; return 1
#define PCQ_FAILURE(x) r->error = x; return 0
#define PCQ_PRIMITIVE(x) \
  if (x) { PCQ_SUCCESS(r->output); } \
  else { PCQ_FAILURE(NULL); }

#define PCQ_MAX_RECURSION_DEPTH 1000

static int pcq_parse_run(pcq_input_t *i, pcq_parser_t *p, pcq_result_t *r, pcq_err_t **e, int depth) {

  int j = 0, k = 0;
  pcq_result_t results_stk[PCQ_PARSE_STACK_MIN];
  pcq_result_t *results;
  int results_slots = PCQ_PARSE_STACK_MIN;

  if (depth == PCQ_MAX_RECURSION_DEPTH)
  {
    PCQ_FAILURE(pcq_err_fail(i, "Maximum recursion depth exceeded!"));
  }

  switch (p->type) {

    /* Basic Parsers */

    case PCQ_TYPE_ANY:     PCQ_PRIMITIVE(pcq_input_any(i, (char**)&r->output));
    case PCQ_TYPE_SINGLE:  PCQ_PRIMITIVE(pcq_input_char(i, p->data.single.x, (char**)&r->output));
    case PCQ_TYPE_RANGE:   PCQ_PRIMITIVE(pcq_input_range(i, p->data.range.x, p->data.range.y, (char**)&r->output));
    case PCQ_TYPE_ONEOF:   PCQ_PRIMITIVE(pcq_input_oneof(i, p->data.string.x, (char**)&r->output));
    case PCQ_TYPE_NONEOF:  PCQ_PRIMITIVE(pcq_input_noneof(i, p->data.string.x, (char**)&r->output));
    case PCQ_TYPE_SATISFY: PCQ_PRIMITIVE(pcq_input_satisfy(i, p->data.satisfy.f, (char**)&r->output));
    case PCQ_TYPE_STRING:  PCQ_PRIMITIVE(pcq_input_string(i, p->data.string.x, (char**)&r->output));
    case PCQ_TYPE_ANCHOR:  PCQ_PRIMITIVE(pcq_input_anchor(i, p->data.anchor.f, (char**)&r->output));
    case PCQ_TYPE_SOI:     PCQ_PRIMITIVE(pcq_input_soi(i, (char**)&r->output));
    case PCQ_TYPE_EOI:     PCQ_PRIMITIVE(pcq_input_eoi(i, (char**)&r->output));

    /* Other parsers */

    case PCQ_TYPE_UNDEFINED: PCQ_FAILURE(pcq_err_fail(i, "Parser Undefined!"));
    case PCQ_TYPE_PASS:      PCQ_SUCCESS(NULL);
    case PCQ_TYPE_FAIL:      PCQ_FAILURE(pcq_err_fail(i, p->data.fail.m));
    case PCQ_TYPE_LIFT:      PCQ_SUCCESS(p->data.lift.lf());
    case PCQ_TYPE_LIFT_VAL:  PCQ_SUCCESS(p->data.lift.x);
    case PCQ_TYPE_STATE:     PCQ_SUCCESS(pcq_input_state_copy(i));

    /* Application Parsers */

    case PCQ_TYPE_APPLY:
      if (pcq_parse_run(i, p->data.apply.x, r, e, depth+1)) {
        PCQ_SUCCESS(pcq_parse_apply(i, p->data.apply.f, r->output));
      } else {
        PCQ_FAILURE(r->output);
      }

    case PCQ_TYPE_APPLY_TO:
      if (pcq_parse_run(i, p->data.apply_to.x, r, e, depth+1)) {
        PCQ_SUCCESS(pcq_parse_apply_to(i, p->data.apply_to.f, r->output, p->data.apply_to.d));
      } else {
        PCQ_FAILURE(r->error);
      }

    case PCQ_TYPE_CHECK:
      if (pcq_parse_run(i, p->data.check.x, r, e, depth+1)) {
        if (p->data.check.f(&r->output)) {
          PCQ_SUCCESS(r->output);
        } else {
          pcq_parse_dtor(i, p->data.check.dx, r->output);
          PCQ_FAILURE(pcq_err_fail(i, p->data.check.e));
        }
      } else {
        PCQ_FAILURE(r->error);
      }

    case PCQ_TYPE_CHECK_WITH:
      if (pcq_parse_run(i, p->data.check_with.x, r, e, depth+1)) {
        if (p->data.check_with.f(&r->output, p->data.check_with.d)) {
          PCQ_SUCCESS(r->output);
        } else {
          pcq_parse_dtor(i, p->data.check.dx, r->output);
          PCQ_FAILURE(pcq_err_fail(i, p->data.check_with.e));
        }
      } else {
        PCQ_FAILURE(r->error);
      }

    case PCQ_TYPE_EXPECT:
      pcq_input_suppress_enable(i);
      if (pcq_parse_run(i, p->data.expect.x, r, e, depth+1)) {
        pcq_input_suppress_disable(i);
        PCQ_SUCCESS(r->output);
      } else {
        pcq_input_suppress_disable(i);
        PCQ_FAILURE(pcq_err_new(i, p->data.expect.m));
      }

    case PCQ_TYPE_PREDICT:
      pcq_input_backtrack_disable(i);
      if (pcq_parse_run(i, p->data.predict.x, r, e, depth+1)) {
        pcq_input_backtrack_enable(i);
        PCQ_SUCCESS(r->output);
      } else {
        pcq_input_backtrack_enable(i);
        PCQ_FAILURE(r->error);
      }

    /* Optional Parsers */

    /* TODO: Update Not Error Message */

    case PCQ_TYPE_NOT:
      pcq_input_mark(i);
      pcq_input_suppress_enable(i);
      if (pcq_parse_run(i, p->data.not.x, r, e, depth+1)) {
        pcq_input_rewind(i);
        pcq_input_suppress_disable(i);
        pcq_parse_dtor(i, p->data.not.dx, r->output);
        PCQ_FAILURE(pcq_err_new(i, "opposite"));
      } else {
        pcq_input_unmark(i);
        pcq_input_suppress_disable(i);
        PCQ_SUCCESS(p->data.not.lf());
      }

    case PCQ_TYPE_MAYBE:
      if (pcq_parse_run(i, p->data.not.x, r, e, depth+1)) {
        PCQ_SUCCESS(r->output);
      } else {
        *e = pcq_err_merge(i, *e, r->error);
        PCQ_SUCCESS(p->data.not.lf());
      }

    /* Repeat Parsers */

    case PCQ_TYPE_MANY:

      results = results_stk;

      while (pcq_parse_run(i, p->data.repeat.x, &results[j], e, depth+1)) {
        j++;
        if (j == PCQ_PARSE_STACK_MIN) {
          results_slots = j + j / 2;
          results = pcq_malloc(i, sizeof(pcq_result_t) * results_slots);
          memcpy(results, results_stk, sizeof(pcq_result_t) * PCQ_PARSE_STACK_MIN);
        } else if (j >= results_slots) {
          results_slots = j + j / 2;
          results = pcq_realloc(i, results, sizeof(pcq_result_t) * results_slots);
        }
      }

      *e = pcq_err_merge(i, *e, results[j].error);

      PCQ_SUCCESS(
        pcq_parse_fold(i, p->data.repeat.f, j, (pcq_val_t**)results);
        if (j >= PCQ_PARSE_STACK_MIN) { pcq_free(i, results); });

    case PCQ_TYPE_MANY1:

      results = results_stk;

      while (pcq_parse_run(i, p->data.repeat.x, &results[j], e, depth+1)) {
        j++;
        if (j == PCQ_PARSE_STACK_MIN) {
          results_slots = j + j / 2;
          results = pcq_malloc(i, sizeof(pcq_result_t) * results_slots);
          memcpy(results, results_stk, sizeof(pcq_result_t) * PCQ_PARSE_STACK_MIN);
        } else if (j >= results_slots) {
          results_slots = j + j / 2;
          results = pcq_realloc(i, results, sizeof(pcq_result_t) * results_slots);
        }
      }

      if (j == 0) {
        PCQ_FAILURE(
          pcq_err_many1(i, results[j].error);
          if (j >= PCQ_PARSE_STACK_MIN) { pcq_free(i, results); });
      } else {

        *e = pcq_err_merge(i, *e, results[j].error);

        PCQ_SUCCESS(
          pcq_parse_fold(i, p->data.repeat.f, j, (pcq_val_t**)results);
          if (j >= PCQ_PARSE_STACK_MIN) { pcq_free(i, results); });
      }

    case PCQ_TYPE_COUNT:

      results = p->data.repeat.n > PCQ_PARSE_STACK_MIN
        ? pcq_malloc(i, sizeof(pcq_result_t) * p->data.repeat.n)
        : results_stk;

      while (pcq_parse_run(i, p->data.repeat.x, &results[j], e, depth+1)) {
        j++;
        if (j == p->data.repeat.n) { break; }
      }

      if (j == p->data.repeat.n) {
        PCQ_SUCCESS(
          pcq_parse_fold(i, p->data.repeat.f, j, (pcq_val_t**)results);
          if (p->data.repeat.n > PCQ_PARSE_STACK_MIN) { pcq_free(i, results); });
      } else {
        for (k = 0; k < j; k++) {
          pcq_parse_dtor(i, p->data.repeat.dx, results[k].output);
        }
        PCQ_FAILURE(
          pcq_err_count(i, results[j].error, p->data.repeat.n);
          if (p->data.repeat.n > PCQ_PARSE_STACK_MIN) { pcq_free(i, results); });
      }

    /* Combinatory Parsers */

    case PCQ_TYPE_OR:

      if (p->data.or.n == 0) { PCQ_SUCCESS(NULL); }

      results = p->data.or.n > PCQ_PARSE_STACK_MIN
        ? pcq_malloc(i, sizeof(pcq_result_t) * p->data.or.n)
        : results_stk;

      for (j = 0; j < p->data.or.n; j++) {
        if (pcq_parse_run(i, p->data.or.xs[j], &results[j], e, depth+1)) {
          PCQ_SUCCESS(results[j].output;
            if (p->data.or.n > PCQ_PARSE_STACK_MIN) { pcq_free(i, results); });
        } else {
          *e = pcq_err_merge(i, *e, results[j].error);
        }
      }

      PCQ_FAILURE(NULL;
        if (p->data.or.n > PCQ_PARSE_STACK_MIN) { pcq_free(i, results); });

    case PCQ_TYPE_AND:

      if (p->data.and.n == 0) { PCQ_SUCCESS(NULL); }

      results = p->data.or.n > PCQ_PARSE_STACK_MIN
        ? pcq_malloc(i, sizeof(pcq_result_t) * p->data.or.n)
        : results_stk;

      pcq_input_mark(i);
      for (j = 0; j < p->data.and.n; j++) {
        if (!pcq_parse_run(i, p->data.and.xs[j], &results[j], e, depth+1)) {
          pcq_input_rewind(i);
          for (k = 0; k < j; k++) {
            pcq_parse_dtor(i, p->data.and.dxs[k], results[k].output);
          }
          PCQ_FAILURE(results[j].error;
            if (p->data.or.n > PCQ_PARSE_STACK_MIN) { pcq_free(i, results); });
        }
      }
      pcq_input_unmark(i);
      PCQ_SUCCESS(
        pcq_parse_fold(i, p->data.and.f, j, (pcq_val_t**)results);
        if (p->data.or.n > PCQ_PARSE_STACK_MIN) { pcq_free(i, results); });

    /* End */

    default:

      PCQ_FAILURE(pcq_err_fail(i, "Unknown Parser Type Id!"));
  }

  return 0;

}

#undef PCQ_SUCCESS
#undef PCQ_FAILURE
#undef PCQ_PRIMITIVE

int pcq_parse_input(pcq_input_t *i, pcq_parser_t *p, pcq_result_t *r) {
  int x;
  pcq_err_t *e = pcq_err_fail(i, "Unknown Error");
  e->state = pcq_state_invalid();
  x = pcq_parse_run(i, p, r, &e, 0);
  if (x) {
    pcq_err_delete_internal(i, e);
    r->output = pcq_export(i, r->output);
  } else {
    r->error = pcq_err_export(i, pcq_err_merge(i, e, r->error));
  }
  return x;
}

int pcq_parse(const char *filename, const char *string, pcq_parser_t *p, pcq_result_t *r) {
  int x;
  pcq_input_t *i = pcq_input_new_string(filename, string);
  x = pcq_parse_input(i, p, r);
  pcq_input_delete(i);
  return x;
}

int pcq_nparse(const char *filename, const char *string, size_t length, pcq_parser_t *p, pcq_result_t *r) {
  int x;
  pcq_input_t *i = pcq_input_new_nstring(filename, string, length);
  x = pcq_parse_input(i, p, r);
  pcq_input_delete(i);
  return x;
}

int pcq_parse_file(const char *filename, FILE *file, pcq_parser_t *p, pcq_result_t *r) {
  int x;
  pcq_input_t *i = pcq_input_new_file(filename, file);
  x = pcq_parse_input(i, p, r);
  pcq_input_delete(i);
  return x;
}

int pcq_parse_pipe(const char *filename, FILE *pipe, pcq_parser_t *p, pcq_result_t *r) {
  int x;
  pcq_input_t *i = pcq_input_new_pipe(filename, pipe);
  x = pcq_parse_input(i, p, r);
  pcq_input_delete(i);
  return x;
}

int pcq_parse_contents(const char *filename, pcq_parser_t *p, pcq_result_t *r) {

  FILE *f = fopen(filename, "rb");
  int res;

  if (f == NULL) {
    r->output = NULL;
    r->error = pcq_err_file(filename, "Unable to open file!");
    return 0;
  }

  res = pcq_parse_file(filename, f, p, r);
  fclose(f);
  return res;
}

/*
** Building a Parser
*/

static void pcq_undefine_unretained(pcq_parser_t *p, int force);

static void pcq_undefine_or(pcq_parser_t *p) {

  int i;
  for (i = 0; i < p->data.or.n; i++) {
    pcq_undefine_unretained(p->data.or.xs[i], 0);
  }
  free(p->data.or.xs);

}

static void pcq_undefine_and(pcq_parser_t *p) {

  int i;
  for (i = 0; i < p->data.and.n; i++) {
    pcq_undefine_unretained(p->data.and.xs[i], 0);
  }
  free(p->data.and.xs);
  free(p->data.and.dxs);

}

static void pcq_undefine_unretained(pcq_parser_t *p, int force) {

  if (p->retained && !force) { return; }

  switch (p->type) {

    case PCQ_TYPE_FAIL: free(p->data.fail.m); break;

    case PCQ_TYPE_ONEOF:
    case PCQ_TYPE_NONEOF:
    case PCQ_TYPE_STRING:
      free(p->data.string.x);
      break;

    case PCQ_TYPE_APPLY:    pcq_undefine_unretained(p->data.apply.x, 0);    break;
    case PCQ_TYPE_APPLY_TO: pcq_undefine_unretained(p->data.apply_to.x, 0); break;
    case PCQ_TYPE_PREDICT:  pcq_undefine_unretained(p->data.predict.x, 0);  break;

    case PCQ_TYPE_MAYBE:
    case PCQ_TYPE_NOT:
      pcq_undefine_unretained(p->data.not.x, 0);
      break;

    case PCQ_TYPE_EXPECT:
      pcq_undefine_unretained(p->data.expect.x, 0);
      free(p->data.expect.m);
      break;

    case PCQ_TYPE_MANY:
    case PCQ_TYPE_MANY1:
    case PCQ_TYPE_COUNT:
      pcq_undefine_unretained(p->data.repeat.x, 0);
      break;

    case PCQ_TYPE_OR:  pcq_undefine_or(p);  break;
    case PCQ_TYPE_AND: pcq_undefine_and(p); break;

    case PCQ_TYPE_CHECK:
      pcq_undefine_unretained(p->data.check.x, 0);
      free(p->data.check.e);
      break;

    case PCQ_TYPE_CHECK_WITH:
      pcq_undefine_unretained(p->data.check_with.x, 0);
      free(p->data.check_with.e);
      break;

    default: break;
  }

  if (!force) {
    free(p->name);
    free(p);
  }

}

void pcq_delete(pcq_parser_t *p) {
  if (p->retained) {

    if (p->type != PCQ_TYPE_UNDEFINED) {
      pcq_undefine_unretained(p, 0);
    }

    free(p->name);
    free(p);

  } else {
    pcq_undefine_unretained(p, 0);
  }
}

static void pcq_soft_delete(pcq_val_t *x) {
  pcq_undefine_unretained(x, 0);
}

static pcq_parser_t *pcq_undefined(void) {
  pcq_parser_t *p = calloc(1, sizeof(pcq_parser_t));
  p->retained = 0;
  p->type = PCQ_TYPE_UNDEFINED;
  p->name = NULL;
  return p;
}

pcq_parser_t *pcq_new(const char *name) {
  pcq_parser_t *p = pcq_undefined();
  p->retained = 1;
  p->name = realloc(p->name, strlen(name) + 1);
  strcpy(p->name, name);
  return p;
}

pcq_parser_t *pcq_copy(pcq_parser_t *a) {
  int i = 0;
  pcq_parser_t *p;

  if (a->retained) { return a; }

  p = pcq_undefined();
  p->retained = a->retained;
  p->type = a->type;
  p->data = a->data;

  if (a->name) {
    p->name = malloc(strlen(a->name)+1);
    strcpy(p->name, a->name);
  }

  switch (a->type) {

    case PCQ_TYPE_FAIL:
      p->data.fail.m = malloc(strlen(a->data.fail.m)+1);
      strcpy(p->data.fail.m, a->data.fail.m);
    break;

    case PCQ_TYPE_ONEOF:
    case PCQ_TYPE_NONEOF:
    case PCQ_TYPE_STRING:
      p->data.string.x = malloc(strlen(a->data.string.x)+1);
      strcpy(p->data.string.x, a->data.string.x);
      break;

    case PCQ_TYPE_APPLY:    p->data.apply.x    = pcq_copy(a->data.apply.x);    break;
    case PCQ_TYPE_APPLY_TO: p->data.apply_to.x = pcq_copy(a->data.apply_to.x); break;
    case PCQ_TYPE_PREDICT:  p->data.predict.x  = pcq_copy(a->data.predict.x);  break;

    case PCQ_TYPE_MAYBE:
    case PCQ_TYPE_NOT:
      p->data.not.x = pcq_copy(a->data.not.x);
      break;

    case PCQ_TYPE_EXPECT:
      p->data.expect.x = pcq_copy(a->data.expect.x);
      p->data.expect.m = malloc(strlen(a->data.expect.m)+1);
      strcpy(p->data.expect.m, a->data.expect.m);
      break;

    case PCQ_TYPE_MANY:
    case PCQ_TYPE_MANY1:
    case PCQ_TYPE_COUNT:
      p->data.repeat.x = pcq_copy(a->data.repeat.x);
      break;

    case PCQ_TYPE_OR:
      p->data.or.xs = malloc(a->data.or.n * sizeof(pcq_parser_t*));
      for (i = 0; i < a->data.or.n; i++) {
        p->data.or.xs[i] = pcq_copy(a->data.or.xs[i]);
      }
    break;
    case PCQ_TYPE_AND:
      p->data.and.xs = malloc(a->data.and.n * sizeof(pcq_parser_t*));
      for (i = 0; i < a->data.and.n; i++) {
        p->data.and.xs[i] = pcq_copy(a->data.and.xs[i]);
      }
      p->data.and.dxs = malloc((a->data.and.n-1) * sizeof(pcq_dtor_t));
      for (i = 0; i < a->data.and.n-1; i++) {
        p->data.and.dxs[i] = a->data.and.dxs[i];
      }
    break;

    case PCQ_TYPE_CHECK:
      p->data.check.x      = pcq_copy(a->data.check.x);
      p->data.check.e      = malloc(strlen(a->data.check.e)+1);
      strcpy(p->data.check.e, a->data.check.e);
      break;
    case PCQ_TYPE_CHECK_WITH:
      p->data.check_with.x = pcq_copy(a->data.check_with.x);
      p->data.check_with.e = malloc(strlen(a->data.check_with.e)+1);
      strcpy(p->data.check_with.e, a->data.check_with.e);
      break;

    default: break;
  }


  return p;
}

pcq_parser_t *pcq_undefine(pcq_parser_t *p) {
  pcq_undefine_unretained(p, 1);
  p->type = PCQ_TYPE_UNDEFINED;
  return p;
}

pcq_parser_t *pcq_define(pcq_parser_t *p, pcq_parser_t *a) {

  if (p->retained) {
    p->type = a->type;
    p->data = a->data;
  } else {
    pcq_parser_t *a2 = pcq_failf("Attempt to assign to Unretained Parser!");
    p->type = a2->type;
    p->data = a2->data;
    free(a2);
  }

  free(a);
  return p;
}

void pcq_cleanup(int n, ...) {
  int i;
  pcq_parser_t **list = malloc(sizeof(pcq_parser_t*) * n);

  va_list va;
  va_start(va, n);
  for (i = 0; i < n; i++) { list[i] = va_arg(va, pcq_parser_t*); }
  for (i = 0; i < n; i++) { pcq_undefine(list[i]); }
  for (i = 0; i < n; i++) { pcq_delete(list[i]); }
  va_end(va);

  free(list);
}

pcq_parser_t *pcq_pass(void) {
  pcq_parser_t *p = pcq_undefined();
  p->type = PCQ_TYPE_PASS;
  return p;
}

pcq_parser_t *pcq_fail(const char *m) {
  pcq_parser_t *p = pcq_undefined();
  p->type = PCQ_TYPE_FAIL;
  p->data.fail.m = malloc(strlen(m) + 1);
  strcpy(p->data.fail.m, m);
  return p;
}

/*
** As `snprintf` is not ANSI standard this
** function `pcq_failf` should be considered
** unsafe.
**
** You have a few options if this is going to be
** trouble.
**
** - Ensure the format string does not exceed
**   the buffer length using precision specifiers
**   such as `%.512s`.
**
** - Patch this function in your code base to
**   use `snprintf` or whatever variant your
**   system supports.
**
** - Avoid it altogether.
**
*/

pcq_parser_t *pcq_failf(const char *fmt, ...) {

  va_list va;
  char *buffer;

  pcq_parser_t *p = pcq_undefined();
  p->type = PCQ_TYPE_FAIL;

  va_start(va, fmt);
  buffer = malloc(2048);
  vsprintf(buffer, fmt, va);
  va_end(va);

  buffer = realloc(buffer, strlen(buffer) + 1);
  p->data.fail.m = buffer;
  return p;

}

pcq_parser_t *pcq_lift_val(pcq_val_t *x) {
  pcq_parser_t *p = pcq_undefined();
  p->type = PCQ_TYPE_LIFT_VAL;
  p->data.lift.x = x;
  return p;
}

pcq_parser_t *pcq_lift(pcq_ctor_t lf) {
  pcq_parser_t *p = pcq_undefined();
  p->type = PCQ_TYPE_LIFT;
  p->data.lift.lf = lf;
  return p;
}

pcq_parser_t *pcq_anchor(int(*f)(char,char)) {
  pcq_parser_t *p = pcq_undefined();
  p->type = PCQ_TYPE_ANCHOR;
  p->data.anchor.f = f;
  return pcq_expect(p, "anchor");
}

pcq_parser_t *pcq_state(void) {
  pcq_parser_t *p = pcq_undefined();
  p->type = PCQ_TYPE_STATE;
  return p;
}

pcq_parser_t *pcq_expect(pcq_parser_t *a, const char *expected) {
  pcq_parser_t *p = pcq_undefined();
  p->type = PCQ_TYPE_EXPECT;
  p->data.expect.x = a;
  p->data.expect.m = malloc(strlen(expected) + 1);
  strcpy(p->data.expect.m, expected);
  return p;
}

/*
** As `snprintf` is not ANSI standard this
** function `pcq_expectf` should be considered
** unsafe.
**
** You have a few options if this is going to be
** trouble.
**
** - Ensure the format string does not exceed
**   the buffer length using precision specifiers
**   such as `%.512s`.
**
** - Patch this function in your code base to
**   use `snprintf` or whatever variant your
**   system supports.
**
** - Avoid it altogether.
**
*/

pcq_parser_t *pcq_expectf(pcq_parser_t *a, const char *fmt, ...) {
  va_list va;
  char *buffer;

  pcq_parser_t *p = pcq_undefined();
  p->type = PCQ_TYPE_EXPECT;

  va_start(va, fmt);
  buffer = malloc(2048);
  vsprintf(buffer, fmt, va);
  va_end(va);

  buffer = realloc(buffer, strlen(buffer) + 1);
  p->data.expect.x = a;
  p->data.expect.m = buffer;
  return p;
}

/*
** Basic Parsers
*/

pcq_parser_t *pcq_any(void) {
  pcq_parser_t *p = pcq_undefined();
  p->type = PCQ_TYPE_ANY;
  return pcq_expect(p, "any character");
}

pcq_parser_t *pcq_char(char c) {
  pcq_parser_t *p = pcq_undefined();
  p->type = PCQ_TYPE_SINGLE;
  p->data.single.x = c;
  return pcq_expectf(p, "'%c'", c);
}

pcq_parser_t *pcq_range(char s, char e) {
  pcq_parser_t *p = pcq_undefined();
  p->type = PCQ_TYPE_RANGE;
  p->data.range.x = s;
  p->data.range.y = e;
  return pcq_expectf(p, "character between '%c' and '%c'", s, e);
}

pcq_parser_t *pcq_oneof(const char *s) {
  pcq_parser_t *p = pcq_undefined();
  p->type = PCQ_TYPE_ONEOF;
  p->data.string.x = malloc(strlen(s) + 1);
  strcpy(p->data.string.x, s);
  return pcq_expectf(p, "one of '%s'", s);
}

pcq_parser_t *pcq_noneof(const char *s) {
  pcq_parser_t *p = pcq_undefined();
  p->type = PCQ_TYPE_NONEOF;
  p->data.string.x = malloc(strlen(s) + 1);
  strcpy(p->data.string.x, s);
  return pcq_expectf(p, "none of '%s'", s);

}

pcq_parser_t *pcq_satisfy(int(*f)(char)) {
  pcq_parser_t *p = pcq_undefined();
  p->type = PCQ_TYPE_SATISFY;
  p->data.satisfy.f = f;
  return pcq_expectf(p, "character satisfying function %p", f);
}

pcq_parser_t *pcq_string(const char *s) {
  pcq_parser_t *p = pcq_undefined();
  p->type = PCQ_TYPE_STRING;
  p->data.string.x = malloc(strlen(s) + 1);
  strcpy(p->data.string.x, s);
  return pcq_expectf(p, "\"%s\"", s);
}

/*
** Core Parsers
*/

pcq_parser_t *pcq_apply(pcq_parser_t *a, pcq_apply_t f) {
  pcq_parser_t *p = pcq_undefined();
  p->type = PCQ_TYPE_APPLY;
  p->data.apply.x = a;
  p->data.apply.f = f;
  return p;
}

pcq_parser_t *pcq_apply_to(pcq_parser_t *a, pcq_apply_to_t f, void *x) {
  pcq_parser_t *p = pcq_undefined();
  p->type = PCQ_TYPE_APPLY_TO;
  p->data.apply_to.x = a;
  p->data.apply_to.f = f;
  p->data.apply_to.d = x;
  return p;
}

pcq_parser_t *pcq_check(pcq_parser_t *a, pcq_dtor_t da, pcq_check_t f, const char *e) {
  pcq_parser_t  *p = pcq_undefined();
  p->type = PCQ_TYPE_CHECK;
  p->data.check.x = a;
  p->data.check.dx = da;
  p->data.check.f = f;
  p->data.check.e = malloc(strlen(e) + 1);
  strcpy(p->data.check.e, e);
  return p;
}

pcq_parser_t *pcq_check_with(pcq_parser_t *a, pcq_dtor_t da, pcq_check_with_t f, void *x, const char *e) {
  pcq_parser_t  *p = pcq_undefined();
  p->type = PCQ_TYPE_CHECK_WITH;
  p->data.check_with.x = a;
  p->data.check_with.dx = da;
  p->data.check_with.f = f;
  p->data.check_with.d = x;
  p->data.check_with.e = malloc(strlen(e) + 1);
  strcpy(p->data.check_with.e, e);
  return p;
}

pcq_parser_t *pcq_checkf(pcq_parser_t *a, pcq_dtor_t da, pcq_check_t f, const char *fmt, ...) {
  va_list va;
  char *buffer;
  pcq_parser_t *p;

  va_start(va, fmt);
  buffer = malloc(2048);
  vsprintf(buffer, fmt, va);
  va_end(va);

  p = pcq_check(a, da, f, buffer);
  free(buffer);

  return p;
}

pcq_parser_t *pcq_check_withf(pcq_parser_t *a, pcq_dtor_t da, pcq_check_with_t f, void *x, const char *fmt, ...) {
  va_list va;
  char *buffer;
  pcq_parser_t *p;

  va_start(va, fmt);
  buffer = malloc(2048);
  vsprintf(buffer, fmt, va);
  va_end(va);

  p = pcq_check_with(a, da, f, x, buffer);
  free(buffer);

  return p;
}

pcq_parser_t *pcq_predictive(pcq_parser_t *a) {
  pcq_parser_t *p = pcq_undefined();
  p->type = PCQ_TYPE_PREDICT;
  p->data.predict.x = a;
  return p;
}

pcq_parser_t *pcq_not_lift(pcq_parser_t *a, pcq_dtor_t da, pcq_ctor_t lf) {
  pcq_parser_t *p = pcq_undefined();
  p->type = PCQ_TYPE_NOT;
  p->data.not.x = a;
  p->data.not.dx = da;
  p->data.not.lf = lf;
  return p;
}

pcq_parser_t *pcq_not(pcq_parser_t *a, pcq_dtor_t da) {
  return pcq_not_lift(a, da, pcqf_ctor_null);
}

pcq_parser_t *pcq_maybe_lift(pcq_parser_t *a, pcq_ctor_t lf) {
  pcq_parser_t *p = pcq_undefined();
  p->type = PCQ_TYPE_MAYBE;
  p->data.not.x = a;
  p->data.not.lf = lf;
  return p;
}

pcq_parser_t *pcq_maybe(pcq_parser_t *a) {
  return pcq_maybe_lift(a, pcqf_ctor_null);
}

pcq_parser_t *pcq_many(pcq_fold_t f, pcq_parser_t *a) {
  pcq_parser_t *p = pcq_undefined();
  p->type = PCQ_TYPE_MANY;
  p->data.repeat.x = a;
  p->data.repeat.f = f;
  return p;
}

pcq_parser_t *pcq_many1(pcq_fold_t f, pcq_parser_t *a) {
  pcq_parser_t *p = pcq_undefined();
  p->type = PCQ_TYPE_MANY1;
  p->data.repeat.x = a;
  p->data.repeat.f = f;
  return p;
}

pcq_parser_t *pcq_count(int n, pcq_fold_t f, pcq_parser_t *a, pcq_dtor_t da) {
  pcq_parser_t *p = pcq_undefined();
  p->type = PCQ_TYPE_COUNT;
  p->data.repeat.n = n;
  p->data.repeat.f = f;
  p->data.repeat.x = a;
  p->data.repeat.dx = da;
  return p;
}

pcq_parser_t *pcq_or(int n, ...) {

  int i;
  va_list va;

  pcq_parser_t *p = pcq_undefined();

  p->type = PCQ_TYPE_OR;
  p->data.or.n = n;
  p->data.or.xs = malloc(sizeof(pcq_parser_t*) * n);

  va_start(va, n);
  for (i = 0; i < n; i++) {
    p->data.or.xs[i] = va_arg(va, pcq_parser_t*);
  }
  va_end(va);

  return p;
}

pcq_parser_t *pcq_and(int n, pcq_fold_t f, ...) {

  int i;
  va_list va;

  pcq_parser_t *p = pcq_undefined();

  p->type = PCQ_TYPE_AND;
  p->data.and.n = n;
  p->data.and.f = f;
  p->data.and.xs = malloc(sizeof(pcq_parser_t*) * n);
  p->data.and.dxs = malloc(sizeof(pcq_dtor_t) * (n-1));

  va_start(va, f);
  for (i = 0; i < n; i++) {
    p->data.and.xs[i] = va_arg(va, pcq_parser_t*);
  }
  for (i = 0; i < (n-1); i++) {
    p->data.and.dxs[i] = va_arg(va, pcq_dtor_t);
  }
  va_end(va);

  return p;
}

/*
** Common Parsers
*/

pcq_parser_t *pcq_soi(void) {
  pcq_parser_t *p = pcq_undefined();
  p->type = PCQ_TYPE_SOI;
  return pcq_expect(p, "start of input");
}

pcq_parser_t *pcq_eoi(void) {
  pcq_parser_t *p = pcq_undefined();
  p->type = PCQ_TYPE_EOI;
  return pcq_expect(p, "end of input");
}

static int pcq_boundary_anchor(char prev, char next) {
  const char* word = "abcdefghijklmnopqrstuvwxyz"
                     "ABCDEFGHIJKLMNOPQRSTUVWXYZ"
                     "0123456789_";
  if ( strchr(word, next) &&  prev == '\0') { return 1; }
  if ( strchr(word, prev) &&  next == '\0') { return 1; }
  if ( strchr(word, next) && !strchr(word, prev)) { return 1; }
  if (!strchr(word, next) &&  strchr(word, prev)) { return 1; }
  return 0;
}

static int pcq_boundary_newline_anchor(char prev, char next) {
  (void)next;
  return prev == '\n';
}

pcq_parser_t *pcq_boundary(void) { return pcq_expect(pcq_anchor(pcq_boundary_anchor), "word boundary"); }
pcq_parser_t *pcq_boundary_newline(void) { return pcq_expect(pcq_anchor(pcq_boundary_newline_anchor), "start of newline"); }

pcq_parser_t *pcq_whitespace(void) { return pcq_expect(pcq_oneof(" \f\n\r\t\v"), "whitespace"); }
pcq_parser_t *pcq_whitespaces(void) { return pcq_expect(pcq_many(pcqf_strfold, pcq_whitespace()), "spaces"); }
pcq_parser_t *pcq_blank(void) { return pcq_expect(pcq_apply(pcq_whitespaces(), pcqf_free), "whitespace"); }

pcq_parser_t *pcq_newline(void) { return pcq_expect(pcq_char('\n'), "newline"); }
pcq_parser_t *pcq_tab(void) { return pcq_expect(pcq_char('\t'), "tab"); }
pcq_parser_t *pcq_escape(void) { return pcq_and(2, pcqf_strfold, pcq_char('\\'), pcq_any(), free); }

pcq_parser_t *pcq_digit(void) { return pcq_expect(pcq_oneof("0123456789"), "digit"); }
pcq_parser_t *pcq_hexdigit(void) { return pcq_expect(pcq_oneof("0123456789ABCDEFabcdef"), "hex digit"); }
pcq_parser_t *pcq_octdigit(void) { return pcq_expect(pcq_oneof("01234567"), "oct digit"); }
pcq_parser_t *pcq_digits(void) { return pcq_expect(pcq_many1(pcqf_strfold, pcq_digit()), "digits"); }
pcq_parser_t *pcq_hexdigits(void) { return pcq_expect(pcq_many1(pcqf_strfold, pcq_hexdigit()), "hex digits"); }
pcq_parser_t *pcq_octdigits(void) { return pcq_expect(pcq_many1(pcqf_strfold, pcq_octdigit()), "oct digits"); }

pcq_parser_t *pcq_lower(void) { return pcq_expect(pcq_oneof("abcdefghijklmnopqrstuvwxyz"), "lowercase letter"); }
pcq_parser_t *pcq_upper(void) { return pcq_expect(pcq_oneof("ABCDEFGHIJKLMNOPQRSTUVWXYZ"), "uppercase letter"); }
pcq_parser_t *pcq_alpha(void) { return pcq_expect(pcq_oneof("abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ"), "letter"); }
pcq_parser_t *pcq_underscore(void) { return pcq_expect(pcq_char('_'), "underscore"); }
pcq_parser_t *pcq_alphanum(void) { return pcq_expect(pcq_or(3, pcq_alpha(), pcq_digit(), pcq_underscore()), "alphanumeric"); }

pcq_parser_t *pcq_int(void) { return pcq_expect(pcq_apply(pcq_digits(), pcqf_int), "integer"); }
pcq_parser_t *pcq_hex(void) { return pcq_expect(pcq_apply(pcq_hexdigits(), pcqf_hex), "hexadecimal"); }
pcq_parser_t *pcq_oct(void) { return pcq_expect(pcq_apply(pcq_octdigits(), pcqf_oct), "octadecimal"); }
pcq_parser_t *pcq_number(void) { return pcq_expect(pcq_or(3, pcq_int(), pcq_hex(), pcq_oct()), "number"); }

pcq_parser_t *pcq_real(void) {

  /* [+-]?\d+(\.\d+)?([eE][+-]?[0-9]+)? */

  pcq_parser_t *p0, *p1, *p2, *p30, *p31, *p32, *p3;

  p0 = pcq_maybe_lift(pcq_oneof("+-"), pcqf_ctor_str);
  p1 = pcq_digits();
  p2 = pcq_maybe_lift(pcq_and(2, pcqf_strfold, pcq_char('.'), pcq_digits(), free), pcqf_ctor_str);
  p30 = pcq_oneof("eE");
  p31 = pcq_maybe_lift(pcq_oneof("+-"), pcqf_ctor_str);
  p32 = pcq_digits();
  p3 = pcq_maybe_lift(pcq_and(3, pcqf_strfold, p30, p31, p32, free, free), pcqf_ctor_str);

  return pcq_expect(pcq_and(4, pcqf_strfold, p0, p1, p2, p3, free, free, free), "real");

}

pcq_parser_t *pcq_float(void) {
  return pcq_expect(pcq_apply(pcq_real(), pcqf_float), "float");
}

pcq_parser_t *pcq_char_lit(void) {
  return pcq_expect(pcq_between(pcq_or(2, pcq_escape(), pcq_any()), free, "'", "'"), "char");
}

pcq_parser_t *pcq_string_lit(void) {
  pcq_parser_t *strchar = pcq_or(2, pcq_escape(), pcq_noneof("\""));
  return pcq_expect(pcq_between(pcq_many(pcqf_strfold, strchar), free, "\"", "\""), "string");
}

pcq_parser_t *pcq_regex_lit(void) {
  pcq_parser_t *regexchar = pcq_or(2, pcq_escape(), pcq_noneof("/"));
  return pcq_expect(pcq_between(pcq_many(pcqf_strfold, regexchar), free, "/", "/"), "regex");
}

pcq_parser_t *pcq_ident(void) {
  pcq_parser_t *p0, *p1;
  p0 = pcq_or(2, pcq_alpha(), pcq_underscore());
  p1 = pcq_many(pcqf_strfold, pcq_alphanum());
  return pcq_and(2, pcqf_strfold, p0, p1, free);
}

/*
** Useful Parsers
*/

pcq_parser_t *pcq_startwith(pcq_parser_t *a) { return pcq_and(2, pcqf_snd, pcq_soi(), a, pcqf_dtor_null); }
pcq_parser_t *pcq_endwith(pcq_parser_t *a, pcq_dtor_t da) { return pcq_and(2, pcqf_fst, a, pcq_eoi(), da); }
pcq_parser_t *pcq_whole(pcq_parser_t *a, pcq_dtor_t da) { return pcq_and(3, pcqf_snd, pcq_soi(), a, pcq_eoi(), pcqf_dtor_null, da); }

pcq_parser_t *pcq_stripl(pcq_parser_t *a) { return pcq_and(2, pcqf_snd, pcq_blank(), a, pcqf_dtor_null); }
pcq_parser_t *pcq_stripr(pcq_parser_t *a) { return pcq_and(2, pcqf_fst, a, pcq_blank(), pcqf_dtor_null); }
pcq_parser_t *pcq_strip(pcq_parser_t *a) { return pcq_and(3, pcqf_snd, pcq_blank(), a, pcq_blank(), pcqf_dtor_null, pcqf_dtor_null); }
pcq_parser_t *pcq_tok(pcq_parser_t *a) { return pcq_and(2, pcqf_fst, a, pcq_blank(), pcqf_dtor_null); }
pcq_parser_t *pcq_sym(const char *s) { return pcq_tok(pcq_string(s)); }

pcq_parser_t *pcq_total(pcq_parser_t *a, pcq_dtor_t da) { return pcq_whole(pcq_strip(a), da); }

pcq_parser_t *pcq_between(pcq_parser_t *a, pcq_dtor_t ad, const char *o, const char *c) {
  return pcq_and(3, pcqf_snd_free,
    pcq_string(o), a, pcq_string(c),
    free, ad);
}

pcq_parser_t *pcq_parens(pcq_parser_t *a, pcq_dtor_t ad)   { return pcq_between(a, ad, "(", ")"); }
pcq_parser_t *pcq_braces(pcq_parser_t *a, pcq_dtor_t ad)   { return pcq_between(a, ad, "<", ">"); }
pcq_parser_t *pcq_brackets(pcq_parser_t *a, pcq_dtor_t ad) { return pcq_between(a, ad, "{", "}"); }
pcq_parser_t *pcq_squares(pcq_parser_t *a, pcq_dtor_t ad)  { return pcq_between(a, ad, "[", "]"); }

pcq_parser_t *pcq_tok_between(pcq_parser_t *a, pcq_dtor_t ad, const char *o, const char *c) {
  return pcq_and(3, pcqf_snd_free,
    pcq_sym(o), pcq_tok(a), pcq_sym(c),
    free, ad);
}

pcq_parser_t *pcq_tok_parens(pcq_parser_t *a, pcq_dtor_t ad)   { return pcq_tok_between(a, ad, "(", ")"); }
pcq_parser_t *pcq_tok_braces(pcq_parser_t *a, pcq_dtor_t ad)   { return pcq_tok_between(a, ad, "<", ">"); }
pcq_parser_t *pcq_tok_brackets(pcq_parser_t *a, pcq_dtor_t ad) { return pcq_tok_between(a, ad, "{", "}"); }
pcq_parser_t *pcq_tok_squares(pcq_parser_t *a, pcq_dtor_t ad)  { return pcq_tok_between(a, ad, "[", "]"); }

/*
** Regular Expression Parsers
*/

/*
** So here is a cute bootstrapping.
**
** I'm using the previously defined
** pcq constructs and functions to
** parse the user regex string and
** construct a parser from it.
**
** As it turns out lots of the standard
** pcq functions look a lot like `fold`
** functions and so can be used indirectly
** by many of the parsing functions to build
** a parser directly - as we are parsing.
**
** This is certainly something that
** would be less elegant/interesting
** in a two-phase parser which first
** builds an AST and then traverses it
** to generate the object.
**
** This whole thing acts as a great
** case study for how trivial it can be
** to write a great parser in a few
** lines of code using pcq.
*/

/*
**
**  ### Regular Expression Grammar
**
**      <regex> : <term> | (<term> "|" <regex>)
**
**      <term> : <factor>*
**
**      <factor> : <base>
**               | <base> "*"
**               | <base> "+"
**               | <base> "?"
**               | <base> "{" <digits> "}"
**
**      <base> : <char>
**             | "\" <char>
**             | "(" <regex> ")"
**             | "[" <range> "]"
*/

static pcq_val_t *pcqf_re_or(int n, pcq_val_t **xs) {
  (void) n;
  if (xs[1] == NULL) { return xs[0]; }
  else { return pcq_or(2, xs[0], xs[1]); }
}

static pcq_val_t *pcqf_re_and(int n, pcq_val_t **xs) {
  int i;
  pcq_parser_t *p = pcq_lift(pcqf_ctor_str);
  for (i = 0; i < n; i++) {
    p = pcq_and(2, pcqf_strfold, p, xs[i], free);
  }
  return p;
}

static pcq_val_t *pcqf_re_repeat(int n, pcq_val_t **xs) {
  int num;
  (void) n;
  if (xs[1] == NULL) { return xs[0]; }
  switch(((char*)xs[1])[0])
  {
    case '*': { free(xs[1]); return pcq_many(pcqf_strfold, xs[0]); }; break;
    case '+': { free(xs[1]); return pcq_many1(pcqf_strfold, xs[0]); }; break;
    case '?': { free(xs[1]); return pcq_maybe_lift(xs[0], pcqf_ctor_str); }; break;
    default:
      num = *(int*)xs[1];
      free(xs[1]);
  }

  return pcq_count(num, pcqf_strfold, xs[0], free);
}

static pcq_parser_t *pcq_re_escape_char(char c) {
  switch (c) {
    case 'a': return pcq_char('\a');
    case 'f': return pcq_char('\f');
    case 'n': return pcq_char('\n');
    case 'r': return pcq_char('\r');
    case 't': return pcq_char('\t');
    case 'v': return pcq_char('\v');
    case 'b': return pcq_and(2, pcqf_snd, pcq_boundary(), pcq_lift(pcqf_ctor_str), free);
    case 'B': return pcq_not_lift(pcq_boundary(), free, pcqf_ctor_str);
    case 'A': return pcq_and(2, pcqf_snd, pcq_soi(), pcq_lift(pcqf_ctor_str), free);
    case 'Z': return pcq_and(2, pcqf_snd, pcq_eoi(), pcq_lift(pcqf_ctor_str), free);
    case 'd': return pcq_digit();
    case 'D': return pcq_not_lift(pcq_digit(), free, pcqf_ctor_str);
    case 's': return pcq_whitespace();
    case 'S': return pcq_not_lift(pcq_whitespace(), free, pcqf_ctor_str);
    case 'w': return pcq_alphanum();
    case 'W': return pcq_not_lift(pcq_alphanum(), free, pcqf_ctor_str);
    default: return NULL;
  }
}

static pcq_val_t *pcqf_re_escape(pcq_val_t *x, void* data) {

  int mode = *((int*)data);
  char *s = x;
  pcq_parser_t *p;

  /* Any Character */
  if (s[0] == '.') {
    free(s);
    if (mode & PCQ_RE_DOTALL) {
      return pcq_any();
    } else {
      return pcq_expect(pcq_noneof("\n"), "any character except a newline");
    }
  }

  /* Start of Input */
  if (s[0] == '^') {
    free(s);
    if (mode & PCQ_RE_MULTILINE) {
      return pcq_and(2, pcqf_snd, pcq_or(2, pcq_soi(), pcq_boundary_newline()), pcq_lift(pcqf_ctor_str), free);
    } else {
      return pcq_and(2, pcqf_snd, pcq_soi(), pcq_lift(pcqf_ctor_str), free);
    }
  }

  /* End of Input */
  if (s[0] == '$') {
    free(s);
    if (mode & PCQ_RE_MULTILINE) {
      return pcq_or(2,
        pcq_newline(),
        pcq_and(2, pcqf_snd, pcq_eoi(), pcq_lift(pcqf_ctor_str), free));
    } else {
      return pcq_or(2,
        pcq_and(2, pcqf_fst, pcq_newline(), pcq_eoi(), free),
        pcq_and(2, pcqf_snd, pcq_eoi(), pcq_lift(pcqf_ctor_str), free));
    }
  }

  /* Regex Escape */
  if (s[0] == '\\') {
    p = pcq_re_escape_char(s[1]);
    p = (p == NULL) ? pcq_char(s[1]) : p;
    free(s);
    return p;
  }

  /* Regex Standard */
  p = pcq_char(s[0]);
  free(s);
  return p;
}

static const char *pcq_re_range_escape_char(char c) {
  switch (c) {
    case '-': return "-";
    case 'a': return "\a";
    case 'f': return "\f";
    case 'n': return "\n";
    case 'r': return "\r";
    case 't': return "\t";
    case 'v': return "\v";
    case 'b': return "\b";
    case 'd': return "0123456789";
    case 's': return " \f\n\r\t\v";
    case 'w': return "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789_";
    default: return NULL;
  }
}

static pcq_val_t *pcqf_re_range(pcq_val_t *x) {

  pcq_parser_t *out;
  size_t i, j;
  size_t start, end;
  const char *tmp = NULL;
  const char *s = x;
  int comp = s[0] == '^' ? 1 : 0;
  char *range = calloc(1,1);

  if (s[0] == '\0') { free(range); free(x); return pcq_fail("Invalid Regex Range Expression"); }
  if (s[0] == '^' &&
      s[1] == '\0') { free(range); free(x); return pcq_fail("Invalid Regex Range Expression"); }

  for (i = comp; i < strlen(s); i++){

    /* Regex Range Escape */
    if (s[i] == '\\') {
      tmp = pcq_re_range_escape_char(s[i+1]);
      if (tmp != NULL) {
        range = realloc(range, strlen(range) + strlen(tmp) + 1);
        strcat(range, tmp);
      } else {
        range = realloc(range, strlen(range) + 1 + 1);
        range[strlen(range) + 1] = '\0';
        range[strlen(range) + 0] = s[i+1];
      }
      i++;
    }

    /* Regex Range...Range */
    else if (s[i] == '-') {
      if (s[i+1] == '\0' || i == 0) {
          range = realloc(range, strlen(range) + strlen("-") + 1);
          strcat(range, "-");
      } else {
        start = s[i-1]+1;
        end = s[i+1]-1;
        for (j = start; j <= end; j++) {
          range = realloc(range, strlen(range) + 1 + 1 + 1);
          range[strlen(range) + 1] = '\0';
          range[strlen(range) + 0] = (char)j;
        }
      }
    }

    /* Regex Range Normal */
    else {
      range = realloc(range, strlen(range) + 1 + 1);
      range[strlen(range) + 1] = '\0';
      range[strlen(range) + 0] = s[i];
    }

  }

  out = comp == 1 ? pcq_noneof(range) : pcq_oneof(range);

  free(x);
  free(range);

  return out;
}

pcq_parser_t *pcq_re(const char *re) {
  return pcq_re_mode(re, PCQ_RE_DEFAULT);
}

pcq_parser_t *pcq_re_mode(const char *re, int mode) {

  char *err_msg;
  pcq_parser_t *err_out;
  pcq_result_t r;
  pcq_parser_t *Regex, *Term, *Factor, *Base, *Range, *RegexEnclose;

  Regex  = pcq_new("regex");
  Term   = pcq_new("term");
  Factor = pcq_new("factor");
  Base   = pcq_new("base");
  Range  = pcq_new("range");

  pcq_define(Regex, pcq_and(2, pcqf_re_or,
    Term,
    pcq_maybe(pcq_and(2, pcqf_snd_free, pcq_char('|'), Regex, free)),
    (pcq_dtor_t)pcq_delete
  ));

  pcq_define(Term, pcq_many(pcqf_re_and, Factor));

  pcq_define(Factor, pcq_and(2, pcqf_re_repeat,
    Base,
    pcq_or(5,
      pcq_char('*'), pcq_char('+'), pcq_char('?'),
      pcq_brackets(pcq_int(), free),
      pcq_pass()),
    (pcq_dtor_t)pcq_delete
  ));

  pcq_define(Base, pcq_or(4,
    pcq_parens(Regex, (pcq_dtor_t)pcq_delete),
    pcq_squares(Range, (pcq_dtor_t)pcq_delete),
    pcq_apply_to(pcq_escape(), pcqf_re_escape, &mode),
    pcq_apply_to(pcq_noneof(")|"), pcqf_re_escape, &mode)
  ));

  pcq_define(Range, pcq_apply(
    pcq_many(pcqf_strfold, pcq_or(2, pcq_escape(), pcq_noneof("]"))),
    pcqf_re_range
  ));

  RegexEnclose = pcq_whole(pcq_predictive(Regex), (pcq_dtor_t)pcq_delete);

  pcq_optimise(RegexEnclose);
  pcq_optimise(Regex);
  pcq_optimise(Term);
  pcq_optimise(Factor);
  pcq_optimise(Base);
  pcq_optimise(Range);

  if(!pcq_parse("<pcq_re_compiler>", re, RegexEnclose, &r)) {
    err_msg = pcq_err_string(r.error);
    err_out = pcq_failf("Invalid Regex: %s", err_msg);
    pcq_err_delete(r.error);
    free(err_msg);
    r.output = err_out;
  }

  pcq_cleanup(6, RegexEnclose, Regex, Term, Factor, Base, Range);

  pcq_optimise(r.output);

  return r.output;

}

/*
** Common Fold Functions
*/

void pcqf_dtor_null(pcq_val_t *x) { (void) x; return; }

pcq_val_t *pcqf_ctor_null(void) { return NULL; }
pcq_val_t *pcqf_ctor_str(void) { return calloc(1, 1); }
pcq_val_t *pcqf_free(pcq_val_t *x) { free(x); return NULL; }

pcq_val_t *pcqf_int(pcq_val_t *x) {
  int *y = malloc(sizeof(int));
  *y = strtol(x, NULL, 10);
  free(x);
  return y;
}

pcq_val_t *pcqf_hex(pcq_val_t *x) {
  int *y = malloc(sizeof(int));
  *y = strtol(x, NULL, 16);
  free(x);
  return y;
}

pcq_val_t *pcqf_oct(pcq_val_t *x) {
  int *y = malloc(sizeof(int));
  *y = strtol(x, NULL, 8);
  free(x);
  return y;
}

pcq_val_t *pcqf_float(pcq_val_t *x) {
  float *y = malloc(sizeof(float));
  *y = strtod(x, NULL);
  free(x);
  return y;
}

pcq_val_t *pcqf_strtriml(pcq_val_t *x) {
  char *s = x;
  while (isspace((unsigned char)*s)) {
    memmove(s, s+1, strlen(s));
  }
  return s;
}

pcq_val_t *pcqf_strtrimr(pcq_val_t *x) {
  char *s = x;
  size_t l = strlen(s);
  while (l > 0 && isspace((unsigned char)s[l-1])) {
    s[l-1] = '\0'; l--;
  }
  return s;
}

pcq_val_t *pcqf_strtrim(pcq_val_t *x) {
  return pcqf_strtriml(pcqf_strtrimr(x));
}

static const char pcq_escape_input_c[]  = {
  '\a', '\b', '\f', '\n', '\r',
  '\t', '\v', '\\', '\'', '\"', '\0'};

static const char *pcq_escape_output_c[] = {
  "\\a", "\\b", "\\f", "\\n", "\\r", "\\t",
  "\\v", "\\\\", "\\'", "\\\"", "\\0", NULL};

static const char pcq_escape_input_raw_re[] = { '/' };
static const char *pcq_escape_output_raw_re[] = { "\\/", NULL };

static const char pcq_escape_input_raw_cstr[] = { '"' };
static const char *pcq_escape_output_raw_cstr[] = { "\\\"", NULL };

static const char pcq_escape_input_raw_cchar[] = { '\'' };
static const char *pcq_escape_output_raw_cchar[] = { "\\'", NULL };

static pcq_val_t *pcqf_escape_new(pcq_val_t *x, const char *input, const char **output) {

  int i;
  int found;
  char buff[2];
  char *s = x;
  char *y = calloc(1, 1);

  while (*s) {

    i = 0;
    found = 0;

    while (output[i]) {
      if (*s == input[i]) {
        y = realloc(y, strlen(y) + strlen(output[i]) + 1);
        strcat(y, output[i]);
        found = 1;
        break;
      }
      i++;
    }

    if (!found) {
      y = realloc(y, strlen(y) + 2);
      buff[0] = *s; buff[1] = '\0';
      strcat(y, buff);
    }

    s++;
  }


  return y;
}

static pcq_val_t *pcqf_unescape_new(pcq_val_t *x, const char *input, const char **output) {

  int i;
  int found = 0;
  char buff[2];
  char *s = x;
  char *y = calloc(1, 1);

  while (*s) {

    i = 0;
    found = 0;

    while (output[i]) {
      if ((*(s+0)) == output[i][0] &&
          (*(s+1)) == output[i][1]) {
        y = realloc(y, strlen(y) + 1 + 1);
        buff[0] = input[i]; buff[1] = '\0';
        strcat(y, buff);
        found = 1;
        s++;
        break;
      }
      i++;
    }

    if (!found) {
      y = realloc(y, strlen(y) + 1 + 1);
      buff[0] = *s; buff[1] = '\0';
      strcat(y, buff);
    }

    if (*s == '\0') { break; }
    else { s++; }
  }

  return y;

}

pcq_val_t *pcqf_escape(pcq_val_t *x) {
  pcq_val_t *y = pcqf_escape_new(x, pcq_escape_input_c, pcq_escape_output_c);
  free(x);
  return y;
}

pcq_val_t *pcqf_unescape(pcq_val_t *x) {
  pcq_val_t *y = pcqf_unescape_new(x, pcq_escape_input_c, pcq_escape_output_c);
  free(x);
  return y;
}

pcq_val_t *pcqf_escape_regex(pcq_val_t *x) {
  pcq_val_t *y = pcqf_escape_new(x, pcq_escape_input_raw_re, pcq_escape_output_raw_re);
  free(x);
  return y;
}

pcq_val_t *pcqf_unescape_regex(pcq_val_t *x) {
  pcq_val_t *y = pcqf_unescape_new(x, pcq_escape_input_raw_re, pcq_escape_output_raw_re);
  free(x);
  return y;
}

pcq_val_t *pcqf_escape_string_raw(pcq_val_t *x) {
  pcq_val_t *y = pcqf_escape_new(x, pcq_escape_input_raw_cstr, pcq_escape_output_raw_cstr);
  free(x);
  return y;
}

pcq_val_t *pcqf_unescape_string_raw(pcq_val_t *x) {
  pcq_val_t *y = pcqf_unescape_new(x, pcq_escape_input_raw_cstr, pcq_escape_output_raw_cstr);
  free(x);
  return y;
}

pcq_val_t *pcqf_escape_char_raw(pcq_val_t *x) {
  pcq_val_t *y = pcqf_escape_new(x, pcq_escape_input_raw_cchar, pcq_escape_output_raw_cchar);
  free(x);
  return y;
}

pcq_val_t *pcqf_unescape_char_raw(pcq_val_t *x) {
  pcq_val_t *y = pcqf_unescape_new(x, pcq_escape_input_raw_cchar, pcq_escape_output_raw_cchar);
  free(x);
  return y;
}

pcq_val_t *pcqf_null(int n, pcq_val_t** xs) { (void) n; (void) xs; return NULL; }
pcq_val_t *pcqf_fst(int n, pcq_val_t **xs) { (void) n; return xs[0]; }
pcq_val_t *pcqf_snd(int n, pcq_val_t **xs) { (void) n; return xs[1]; }
pcq_val_t *pcqf_trd(int n, pcq_val_t **xs) { (void) n; return xs[2]; }

static pcq_val_t *pcqf_nth_free(int n, pcq_val_t **xs, int x) {
  int i;
  for (i = 0; i < n; i++) {
    if (i != x) { free(xs[i]); }
  }
  return xs[x];
}

pcq_val_t *pcqf_fst_free(int n, pcq_val_t **xs) { return pcqf_nth_free(n, xs, 0); }
pcq_val_t *pcqf_snd_free(int n, pcq_val_t **xs) { return pcqf_nth_free(n, xs, 1); }
pcq_val_t *pcqf_trd_free(int n, pcq_val_t **xs) { return pcqf_nth_free(n, xs, 2); }

pcq_val_t *pcqf_freefold(int n, pcq_val_t **xs) {
  int i;
  for (i = 0; i < n; i++) {
    free(xs[i]);
  }
  return NULL;
}

pcq_val_t *pcqf_strfold(int n, pcq_val_t **xs) {
  int i;
  size_t l = 0;

  if (n == 0) { return calloc(1, 1); }

  for (i = 0; i < n; i++) { l += strlen(xs[i]); }

  xs[0] = realloc(xs[0], l + 1);

  for (i = 1; i < n; i++) {
    strcat(xs[0], xs[i]); free(xs[i]);
  }

  return xs[0];
}

pcq_val_t *pcqf_maths(int n, pcq_val_t **xs) {
  int **vs = (int**)xs;
  (void) n;

  switch(((char*)xs[1])[0])
  {
    case '*': { *vs[0] *= *vs[2]; }; break;
    case '/': { *vs[0] /= *vs[2]; }; break;
    case '%': { *vs[0] %= *vs[2]; }; break;
    case '+': { *vs[0] += *vs[2]; }; break;
    case '-': { *vs[0] -= *vs[2]; }; break;
    default: break;
  }

  free(xs[1]); free(xs[2]);

  return xs[0];
}

/*
** Printing
*/

static void pcq_print_unretained(pcq_parser_t *p, int force) {

  /* TODO: Print Everything Escaped */

  int i;
  char *s, *e;
  char buff[2];

  if (p->retained && !force) {;
    if (p->name) { printf("<%s>", p->name); }
    else { printf("<anon>"); }
    return;
  }

  if (p->type == PCQ_TYPE_UNDEFINED) { printf("<?>"); }
  if (p->type == PCQ_TYPE_PASS)   { printf("<:>"); }
  if (p->type == PCQ_TYPE_FAIL)   { printf("<!>"); }
  if (p->type == PCQ_TYPE_LIFT)   { printf("<#>"); }
  if (p->type == PCQ_TYPE_STATE)  { printf("<S>"); }
  if (p->type == PCQ_TYPE_ANCHOR) { printf("<@>"); }
  if (p->type == PCQ_TYPE_EXPECT) {
    printf("%s", p->data.expect.m);
    /*pcq_print_unretained(p->data.expect.x, 0);*/
  }

  if (p->type == PCQ_TYPE_ANY) { printf("<.>"); }
  if (p->type == PCQ_TYPE_SATISFY) { printf("<f>"); }

  if (p->type == PCQ_TYPE_SINGLE) {
    buff[0] = p->data.single.x; buff[1] = '\0';
    s = pcqf_escape_new(
      buff,
      pcq_escape_input_c,
      pcq_escape_output_c);
    printf("'%s'", s);
    free(s);
  }

  if (p->type == PCQ_TYPE_RANGE) {
    buff[0] = p->data.range.x; buff[1] = '\0';
    s = pcqf_escape_new(
      buff,
      pcq_escape_input_c,
      pcq_escape_output_c);
    buff[0] = p->data.range.y; buff[1] = '\0';
    e = pcqf_escape_new(
      buff,
      pcq_escape_input_c,
      pcq_escape_output_c);
    printf("[%s-%s]", s, e);
    free(s);
    free(e);
  }

  if (p->type == PCQ_TYPE_ONEOF) {
    s = pcqf_escape_new(
      p->data.string.x,
      pcq_escape_input_c,
      pcq_escape_output_c);
    printf("[%s]", s);
    free(s);
  }

  if (p->type == PCQ_TYPE_NONEOF) {
    s = pcqf_escape_new(
      p->data.string.x,
      pcq_escape_input_c,
      pcq_escape_output_c);
    printf("[^%s]", s);
    free(s);
  }

  if (p->type == PCQ_TYPE_STRING) {
    s = pcqf_escape_new(
      p->data.string.x,
      pcq_escape_input_c,
      pcq_escape_output_c);
    printf("\"%s\"", s);
    free(s);
  }

  if (p->type == PCQ_TYPE_APPLY)    { pcq_print_unretained(p->data.apply.x, 0); }
  if (p->type == PCQ_TYPE_APPLY_TO) { pcq_print_unretained(p->data.apply_to.x, 0); }
  if (p->type == PCQ_TYPE_PREDICT)  { pcq_print_unretained(p->data.predict.x, 0); }

  if (p->type == PCQ_TYPE_NOT)   { pcq_print_unretained(p->data.not.x, 0); printf("!"); }
  if (p->type == PCQ_TYPE_MAYBE) { pcq_print_unretained(p->data.not.x, 0); printf("?"); }

  if (p->type == PCQ_TYPE_MANY)  { pcq_print_unretained(p->data.repeat.x, 0); printf("*"); }
  if (p->type == PCQ_TYPE_MANY1) { pcq_print_unretained(p->data.repeat.x, 0); printf("+"); }
  if (p->type == PCQ_TYPE_COUNT) { pcq_print_unretained(p->data.repeat.x, 0); printf("{%i}", p->data.repeat.n); }

  if (p->type == PCQ_TYPE_OR) {
    printf("(");
    for(i = 0; i < p->data.or.n-1; i++) {
      pcq_print_unretained(p->data.or.xs[i], 0);
      printf(" | ");
    }
    pcq_print_unretained(p->data.or.xs[p->data.or.n-1], 0);
    printf(")");
  }

  if (p->type == PCQ_TYPE_AND) {
    printf("(");
    for(i = 0; i < p->data.and.n-1; i++) {
      pcq_print_unretained(p->data.and.xs[i], 0);
      printf(" ");
    }
    pcq_print_unretained(p->data.and.xs[p->data.and.n-1], 0);
    printf(")");
  }

  if (p->type == PCQ_TYPE_CHECK) {
    pcq_print_unretained(p->data.check.x, 0);
    printf("->?");
  }
  if (p->type == PCQ_TYPE_CHECK_WITH) {
    pcq_print_unretained(p->data.check_with.x, 0);
    printf("->?");
  }

}

void pcq_print(pcq_parser_t *p) {
  pcq_print_unretained(p, 1);
  printf("\n");
}

/*
** Testing
*/

/*
** These functions are slightly unwieldy and
** also the whole of the testing suite for pcq
** pcq is pretty shaky.
**
** It could do with a lot more tests and more
** precision. Currently I am only really testing
** changes off of the examples.
**
*/

int pcq_test_fail(pcq_parser_t *p, const char *s, const void *d,
  int(*tester)(const void*, const void*),
  pcq_dtor_t destructor,
  void(*printer)(const void*)) {
  pcq_result_t r;
  (void) printer;
  if (pcq_parse("<test>", s, p, &r)) {

    if (tester(r.output, d)) {
      destructor(r.output);
      return 0;
    } else {
      destructor(r.output);
      return 1;
    }

  } else {
    pcq_err_delete(r.error);
    return 1;
  }

}

int pcq_test_pass(pcq_parser_t *p, const char *s, const void *d,
  int(*tester)(const void*, const void*),
  pcq_dtor_t destructor,
  void(*printer)(const void*)) {

  pcq_result_t r;
  if (pcq_parse("<test>", s, p, &r)) {

    if (tester(r.output, d)) {
      destructor(r.output);
      return 1;
    } else {
      printf("Got "); printer(r.output); printf("\n");
      printf("Expected "); printer(d); printf("\n");
      destructor(r.output);
      return 0;
    }

  } else {
    pcq_err_print(r.error);
    pcq_err_delete(r.error);
    return 0;

  }

}


/*
** AST
*/

void pcq_ast_delete(pcq_ast_t *a) {

  int i;

  if (a == NULL) { return; }

  for (i = 0; i < a->children_num; i++) {
    pcq_ast_delete(a->children[i]);
  }

  free(a->children);
  free(a->tag);
  free(a->contents);
  free(a);

}

static void pcq_ast_delete_no_children(pcq_ast_t *a) {
  free(a->children);
  free(a->tag);
  free(a->contents);
  free(a);
}

pcq_ast_t *pcq_ast_new(const char *tag, const char *contents) {

  pcq_ast_t *a = malloc(sizeof(pcq_ast_t));

  a->tag = malloc(strlen(tag) + 1);
  strcpy(a->tag, tag);

  a->contents = malloc(strlen(contents) + 1);
  strcpy(a->contents, contents);

  a->state = pcq_state_new();

  a->children_num = 0;
  a->children = NULL;
  return a;

}

pcq_ast_t *pcq_ast_build(int n, const char *tag, ...) {

  pcq_ast_t *a = pcq_ast_new(tag, "");

  int i;
  va_list va;
  va_start(va, tag);

  for (i = 0; i < n; i++) {
    pcq_ast_add_child(a, va_arg(va, pcq_ast_t*));
  }

  va_end(va);

  return a;

}

pcq_ast_t *pcq_ast_add_root(pcq_ast_t *a) {

  pcq_ast_t *r;

  if (a == NULL) { return a; }
  if (a->children_num == 0) { return a; }
  if (a->children_num == 1) { return a; }

  r = pcq_ast_new(">", "");
  pcq_ast_add_child(r, a);
  return r;
}

int pcq_ast_eq(pcq_ast_t *a, pcq_ast_t *b) {

  int i;

  if (strcmp(a->tag, b->tag) != 0) { return 0; }
  if (strcmp(a->contents, b->contents) != 0) { return 0; }
  if (a->children_num != b->children_num) { return 0; }

  for (i = 0; i < a->children_num; i++) {
    if (!pcq_ast_eq(a->children[i], b->children[i])) { return 0; }
  }

  return 1;
}

pcq_ast_t *pcq_ast_add_child(pcq_ast_t *r, pcq_ast_t *a) {
  r->children_num++;
  r->children = realloc(r->children, sizeof(pcq_ast_t*) * r->children_num);
  r->children[r->children_num-1] = a;
  return r;
}

pcq_ast_t *pcq_ast_add_tag(pcq_ast_t *a, const char *t) {
  if (a == NULL) { return a; }
  a->tag = realloc(a->tag, strlen(t) + 1 + strlen(a->tag) + 1);
  memmove(a->tag + strlen(t) + 1, a->tag, strlen(a->tag)+1);
  memmove(a->tag, t, strlen(t));
  memmove(a->tag + strlen(t), "|", 1);
  return a;
}

pcq_ast_t *pcq_ast_add_root_tag(pcq_ast_t *a, const char *t) {
  if (a == NULL) { return a; }
  a->tag = realloc(a->tag, (strlen(t)-1) + strlen(a->tag) + 1);
  memmove(a->tag + (strlen(t)-1), a->tag, strlen(a->tag)+1);
  memmove(a->tag, t, (strlen(t)-1));
  return a;
}

pcq_ast_t *pcq_ast_tag(pcq_ast_t *a, const char *t) {
  a->tag = realloc(a->tag, strlen(t) + 1);
  strcpy(a->tag, t);
  return a;
}

pcq_ast_t *pcq_ast_state(pcq_ast_t *a, pcq_state_t s) {
  if (a == NULL) { return a; }
  a->state = s;
  return a;
}

static void pcq_ast_print_depth(pcq_ast_t *a, int d, FILE *fp) {

  int i;

  if (a == NULL) {
    fprintf(fp, "NULL\n");
    return;
  }

  for (i = 0; i < d; i++) { fprintf(fp, "  "); }

  if (strlen(a->contents)) {
    fprintf(fp, "%s:%lu:%lu '%s'\n", a->tag,
      (long unsigned int)(a->state.row+1),
      (long unsigned int)(a->state.col+1),
      a->contents);
  } else {
    fprintf(fp, "%s \n", a->tag);
  }

  for (i = 0; i < a->children_num; i++) {
    pcq_ast_print_depth(a->children[i], d+1, fp);
  }

}

void pcq_ast_print(pcq_ast_t *a) {
  pcq_ast_print_depth(a, 0, stdout);
}

void pcq_ast_print_to(pcq_ast_t *a, FILE *fp) {
  pcq_ast_print_depth(a, 0, fp);
}

int pcq_ast_get_index(pcq_ast_t *ast, const char *tag) {
  return pcq_ast_get_index_lb(ast, tag, 0);
}

int pcq_ast_get_index_lb(pcq_ast_t *ast, const char *tag, int lb) {
  int i;

  for(i=lb; i<ast->children_num; i++) {
    if(strcmp(ast->children[i]->tag, tag) == 0) {
      return i;
    }
  }

  return -1;
}

pcq_ast_t *pcq_ast_get_child(pcq_ast_t *ast, const char *tag) {
  return pcq_ast_get_child_lb(ast, tag, 0);
}

pcq_ast_t *pcq_ast_get_child_lb(pcq_ast_t *ast, const char *tag, int lb) {
  int i;

  for(i=lb; i<ast->children_num; i++) {
    if(strcmp(ast->children[i]->tag, tag) == 0) {
      return ast->children[i];
    }
  }

  return NULL;
}

pcq_ast_trav_t *pcq_ast_traverse_start(pcq_ast_t *ast,
                                       pcq_ast_trav_order_t order)
{
  pcq_ast_trav_t *trav, *n_trav;
  pcq_ast_t *cnode = ast;

  /* Create the traversal structure */
  trav = malloc(sizeof(pcq_ast_trav_t));
  trav->curr_node = cnode;
  trav->parent = NULL;
  trav->curr_child = 0;
  trav->order = order;

  /* Get start node */
  switch(order) {
    case pcq_ast_trav_order_pre:
      /* Nothing else is needed for pre order start */
      break;

    case pcq_ast_trav_order_post:
      while(cnode->children_num > 0) {
        cnode = cnode->children[0];

        n_trav = malloc(sizeof(pcq_ast_trav_t));
        n_trav->curr_node = cnode;
        n_trav->parent = trav;
        n_trav->curr_child = 0;
        n_trav->order = order;

        trav = n_trav;
      }

      break;

    default:
      /* Unreachable, but compiler complaints */
      break;
  }

  return trav;
}

pcq_ast_t *pcq_ast_traverse_next(pcq_ast_trav_t **trav) {
  pcq_ast_trav_t *n_trav, *to_free;
  pcq_ast_t *ret = NULL;
  int cchild;

  /* The end of traversal was reached */
  if(*trav == NULL) return NULL;

  switch((*trav)->order) {
    case pcq_ast_trav_order_pre:
      ret = (*trav)->curr_node;

      /* If there aren't any more children, go up */
      while(*trav != NULL &&
        (*trav)->curr_child >= (*trav)->curr_node->children_num)
      {
        to_free = *trav;
        *trav = (*trav)->parent;
        free(to_free);
      }

      /* If trav is NULL, the end was reached */
      if(*trav == NULL) {
        break;
      }

      /* Go to next child */
      n_trav = malloc(sizeof(pcq_ast_trav_t));

      cchild = (*trav)->curr_child;
      n_trav->curr_node = (*trav)->curr_node->children[cchild];
      n_trav->parent = *trav;
      n_trav->curr_child = 0;
      n_trav->order = (*trav)->order;

      (*trav)->curr_child++;
      *trav = n_trav;

      break;

    case pcq_ast_trav_order_post:
      ret = (*trav)->curr_node;

      /* Move up tree to the parent If the parent doesn't have any more nodes,
       * then this is the current node. If it does, move down to its left most
       * child. Also, free the previous traversal node */
      to_free = *trav;
      *trav = (*trav)->parent;
      free(to_free);

      if(*trav == NULL)
        break;

      /* Next child */
      (*trav)->curr_child++;

      /* If there aren't any more children, this is the next node */
      if((*trav)->curr_child >= (*trav)->curr_node->children_num) {
        break;
      }

      /* If there are still more children, find the leftmost child from this
       * node */
      while((*trav)->curr_node->children_num > 0) {
        n_trav = malloc(sizeof(pcq_ast_trav_t));

        cchild = (*trav)->curr_child;
        n_trav->curr_node = (*trav)->curr_node->children[cchild];
        n_trav->parent = *trav;
        n_trav->curr_child = 0;
        n_trav->order = (*trav)->order;

        *trav = n_trav;
      }

    default:
      /* Unreachable, but compiler complaints */
      break;
  }

  return ret;
}

void pcq_ast_traverse_free(pcq_ast_trav_t **trav) {
  pcq_ast_trav_t *n_trav;

  /* Go through parents until all are free */
  while(*trav != NULL) {
      n_trav = (*trav)->parent;
      free(*trav);
      *trav = n_trav;
  }
}

pcq_val_t *pcqf_fold_ast(int n, pcq_val_t **xs) {

  int i, j;
  pcq_ast_t** as = (pcq_ast_t**)xs;
  pcq_ast_t *r;

  if (n == 0) { return NULL; }
  if (n == 1) { return xs[0]; }
  if (n == 2 && xs[1] == NULL) { return xs[0]; }
  if (n == 2 && xs[0] == NULL) { return xs[1]; }

  r = pcq_ast_new(">", "");

  for (i = 0; i < n; i++) {

    if (as[i] == NULL) { continue; }

    if        (as[i] && as[i]->children_num == 0) {
      pcq_ast_add_child(r, as[i]);
    } else if (as[i] && as[i]->children_num == 1) {
      pcq_ast_add_child(r, pcq_ast_add_root_tag(as[i]->children[0], as[i]->tag));
      pcq_ast_delete_no_children(as[i]);
    } else if (as[i] && as[i]->children_num >= 2) {
      for (j = 0; j < as[i]->children_num; j++) {
        pcq_ast_add_child(r, as[i]->children[j]);
      }
      pcq_ast_delete_no_children(as[i]);
    }

  }

  if (r->children_num) {
    r->state = r->children[0]->state;
  }

  return r;
}

pcq_val_t *pcqf_str_ast(pcq_val_t *c) {
  pcq_ast_t *a = pcq_ast_new("", c);
  free(c);
  return a;
}

pcq_val_t *pcqf_state_ast(int n, pcq_val_t **xs) {
  pcq_state_t *s = ((pcq_state_t**)xs)[0];
  pcq_ast_t *a = ((pcq_ast_t**)xs)[1];
  (void)n;
  a = pcq_ast_state(a, *s);
  free(s);
  return a;
}

pcq_parser_t *pcqa_state(pcq_parser_t *a) {
  return pcq_and(2, pcqf_state_ast, pcq_state(), a, free);
}

pcq_parser_t *pcqa_tag(pcq_parser_t *a, const char *t) {
  return pcq_apply_to(a, (pcq_apply_to_t)pcq_ast_tag, (void*)t);
}

pcq_parser_t *pcqa_add_tag(pcq_parser_t *a, const char *t) {
  return pcq_apply_to(a, (pcq_apply_to_t)pcq_ast_add_tag, (void*)t);
}

pcq_parser_t *pcqa_root(pcq_parser_t *a) {
  return pcq_apply(a, (pcq_apply_t)pcq_ast_add_root);
}

pcq_parser_t *pcqa_not(pcq_parser_t *a) { return pcq_not(a, (pcq_dtor_t)pcq_ast_delete); }
pcq_parser_t *pcqa_maybe(pcq_parser_t *a) { return pcq_maybe(a); }
pcq_parser_t *pcqa_many(pcq_parser_t *a) { return pcq_many(pcqf_fold_ast, a); }
pcq_parser_t *pcqa_many1(pcq_parser_t *a) { return pcq_many1(pcqf_fold_ast, a); }
pcq_parser_t *pcqa_count(int n, pcq_parser_t *a) { return pcq_count(n, pcqf_fold_ast, a, (pcq_dtor_t)pcq_ast_delete); }

pcq_parser_t *pcqa_or(int n, ...) {

  int i;
  va_list va;

  pcq_parser_t *p = pcq_undefined();

  p->type = PCQ_TYPE_OR;
  p->data.or.n = n;
  p->data.or.xs = malloc(sizeof(pcq_parser_t*) * n);

  va_start(va, n);
  for (i = 0; i < n; i++) {
    p->data.or.xs[i] = va_arg(va, pcq_parser_t*);
  }
  va_end(va);

  return p;

}

pcq_parser_t *pcqa_and(int n, ...) {

  int i;
  va_list va;

  pcq_parser_t *p = pcq_undefined();

  p->type = PCQ_TYPE_AND;
  p->data.and.n = n;
  p->data.and.f = pcqf_fold_ast;
  p->data.and.xs = malloc(sizeof(pcq_parser_t*) * n);
  p->data.and.dxs = malloc(sizeof(pcq_dtor_t) * (n-1));

  va_start(va, n);
  for (i = 0; i < n; i++) {
    p->data.and.xs[i] = va_arg(va, pcq_parser_t*);
  }
  for (i = 0; i < (n-1); i++) {
    p->data.and.dxs[i] = (pcq_dtor_t)pcq_ast_delete;
  }
  va_end(va);

  return p;
}

pcq_parser_t *pcqa_total(pcq_parser_t *a) { return pcq_total(a, (pcq_dtor_t)pcq_ast_delete); }

/*
** Grammar Parser
*/

/*
** This is another interesting bootstrapping.
**
** Having a general purpose AST type allows
** users to specify the grammar alone and
** let all fold rules be automatically taken
** care of by existing functions.
**
** You don't get to control the type spat
** out but this means you can make a nice
** parser to take in some grammar in nice
** syntax and spit out a parser that works.
**
** The grammar for this looks surprisingly
** like regex but the main difference is that
** it is now whitespace insensitive and the
** base type takes literals of some form.
*/

/*
**
**  ### Grammar Grammar
**
**      <grammar> : (<term> "|" <grammar>) | <term>
**
**      <term> : <factor>*
**
**      <factor> : <base>
**               | <base> "*"
**               | <base> "+"
**               | <base> "?"
**               | <base> "{" <digits> "}"
**
**      <base> : "<" (<digits> | <ident>) ">"
**             | <string_lit>
**             | <char_lit>
**             | <regex_lit> <regex_mode>
**             | "(" <grammar> ")"
*/

typedef struct {
  va_list *va;
  int parsers_num;
  pcq_parser_t **parsers;
  int flags;
} pcqa_grammar_st_t;

static pcq_val_t *pcqaf_grammar_or(int n, pcq_val_t **xs) {
  (void) n;
  if (xs[1] == NULL) { return xs[0]; }
  else { return pcqa_or(2, xs[0], xs[1]); }
}

static pcq_val_t *pcqaf_grammar_and(int n, pcq_val_t **xs) {
  int i;
  pcq_parser_t *p = pcq_pass();
  for (i = 0; i < n; i++) {
    if (xs[i] != NULL) { p = pcqa_and(2, p, xs[i]); }
  }
  return p;
}

static pcq_val_t *pcqaf_grammar_repeat(int n, pcq_val_t **xs) {
  int num;
  (void) n;
  if (xs[1] == NULL) { return xs[0]; }
  switch(((char*)xs[1])[0])
  {
    case '*': { free(xs[1]); return pcqa_many(xs[0]); }; break;
    case '+': { free(xs[1]); return pcqa_many1(xs[0]); }; break;
    case '?': { free(xs[1]); return pcqa_maybe(xs[0]); }; break;
    case '!': { free(xs[1]); return pcqa_not(xs[0]); }; break;
    default:
      num = *((int*)xs[1]);
      free(xs[1]);
  }
  return pcqa_count(num, xs[0]);
}

static pcq_val_t *pcqaf_grammar_string(pcq_val_t *x, void *s) {
  pcqa_grammar_st_t *st = s;
  char *y = pcqf_unescape(x);
  pcq_parser_t *p = (st->flags & PCQA_LANG_WHITESPACE_SENSITIVE) ? pcq_string(y) : pcq_tok(pcq_string(y));
  free(y);
  return pcqa_state(pcqa_tag(pcq_apply(p, pcqf_str_ast), "string"));
}

static pcq_val_t *pcqaf_grammar_char(pcq_val_t *x, void *s) {
  pcqa_grammar_st_t *st = s;
  char *y = pcqf_unescape(x);
  pcq_parser_t *p = (st->flags & PCQA_LANG_WHITESPACE_SENSITIVE) ? pcq_char(y[0]) : pcq_tok(pcq_char(y[0]));
  free(y);
  return pcqa_state(pcqa_tag(pcq_apply(p, pcqf_str_ast), "char"));
}

static pcq_val_t *pcqaf_fold_regex(int n, pcq_val_t **xs) {
  char *y = xs[0];
  char *m = xs[1];
  pcqa_grammar_st_t *st = xs[2];
  pcq_parser_t *p;
  int mode = PCQ_RE_DEFAULT;

  (void)n;
  if (strchr(m, 'm')) { mode |= PCQ_RE_MULTILINE; }
  if (strchr(m, 's')) { mode |= PCQ_RE_DOTALL; }
  y = pcqf_unescape_regex(y);
  p = (st->flags & PCQA_LANG_WHITESPACE_SENSITIVE) ? pcq_re_mode(y, mode) : pcq_tok(pcq_re_mode(y, mode));
  free(y);
  free(m);

  return pcqa_state(pcqa_tag(pcq_apply(p, pcqf_str_ast), "regex"));
}

/* Should this just use `isdigit` instead? */
static int is_number(const char* s) {
  size_t i;
  for (i = 0; i < strlen(s); i++) { if (!strchr("0123456789", s[i])) { return 0; } }
  return 1;
}

static pcq_parser_t *pcqa_grammar_find_parser(char *x, pcqa_grammar_st_t *st) {

  int i;
  pcq_parser_t *p;

  /* Case of Number */
  if (is_number(x)) {

    i = strtol(x, NULL, 10);

    while (st->parsers_num <= i) {
      st->parsers_num++;
      st->parsers = realloc(st->parsers, sizeof(pcq_parser_t*) * st->parsers_num);
      st->parsers[st->parsers_num-1] = va_arg(*st->va, pcq_parser_t*);
      if (st->parsers[st->parsers_num-1] == NULL) {
        return pcq_failf("No Parser in position %i! Only supplied %i Parsers!", i, st->parsers_num);
      }
    }

    return st->parsers[st->parsers_num-1];

  /* Case of Identifier */
  } else {

    /* Search Existing Parsers */
    for (i = 0; i < st->parsers_num; i++) {
      pcq_parser_t *q = st->parsers[i];
      if (q == NULL) { return pcq_failf("Unknown Parser '%s'!", x); }
      if (q->name && strcmp(q->name, x) == 0) { return q; }
    }

    /* Search New Parsers */
    while (1) {

      p = va_arg(*st->va, pcq_parser_t*);

      st->parsers_num++;
      st->parsers = realloc(st->parsers, sizeof(pcq_parser_t*) * st->parsers_num);
      st->parsers[st->parsers_num-1] = p;

      if (p == NULL || p->name == NULL) { return pcq_failf("Unknown Parser '%s'!", x); }
      if (p->name && strcmp(p->name, x) == 0) { return p; }

    }

  }

}

static pcq_val_t *pcqaf_grammar_id(pcq_val_t *x, void *s) {

  pcqa_grammar_st_t *st = s;
  pcq_parser_t *p = pcqa_grammar_find_parser(x, st);
  free(x);

  if (p->name) {
    return pcqa_state(pcqa_root(pcqa_add_tag(p, p->name)));
  } else {
    return pcqa_state(pcqa_root(p));
  }
}

pcq_parser_t *pcqa_grammar_st(const char *grammar, pcqa_grammar_st_t *st) {

  char *err_msg;
  pcq_parser_t *err_out;
  pcq_result_t r;
  pcq_parser_t *GrammarTotal, *Grammar, *Term, *Factor, *Base;

  GrammarTotal = pcq_new("grammar_total");
  Grammar = pcq_new("grammar");
  Term = pcq_new("term");
  Factor = pcq_new("factor");
  Base = pcq_new("base");

  pcq_define(GrammarTotal,
    pcq_predictive(pcq_total(Grammar, pcq_soft_delete))
  );

  pcq_define(Grammar, pcq_and(2, pcqaf_grammar_or,
    Term,
    pcq_maybe(pcq_and(2, pcqf_snd_free, pcq_sym("|"), Grammar, free)),
    pcq_soft_delete
  ));

  pcq_define(Term, pcq_many1(pcqaf_grammar_and, Factor));

  pcq_define(Factor, pcq_and(2, pcqaf_grammar_repeat,
    Base,
      pcq_or(6,
        pcq_sym("*"),
        pcq_sym("+"),
        pcq_sym("?"),
        pcq_sym("!"),
        pcq_tok_brackets(pcq_int(), free),
        pcq_pass()),
    pcq_soft_delete
  ));

  pcq_define(Base, pcq_or(5,
    pcq_apply_to(pcq_tok(pcq_string_lit()), pcqaf_grammar_string, st),
    pcq_apply_to(pcq_tok(pcq_char_lit()),   pcqaf_grammar_char, st),
    pcq_tok(pcq_and(3, pcqaf_fold_regex, pcq_regex_lit(), pcq_many(pcqf_strfold, pcq_oneof("ms")), pcq_lift_val(st), free, free)),
    pcq_apply_to(pcq_tok_braces(pcq_or(2, pcq_digits(), pcq_ident()), free), pcqaf_grammar_id, st),
    pcq_tok_parens(Grammar, pcq_soft_delete)
  ));

  pcq_optimise(GrammarTotal);
  pcq_optimise(Grammar);
  pcq_optimise(Factor);
  pcq_optimise(Term);
  pcq_optimise(Base);

  if(!pcq_parse("<pcq_grammar_compiler>", grammar, GrammarTotal, &r)) {
    err_msg = pcq_err_string(r.error);
    err_out = pcq_failf("Invalid Grammar: %s", err_msg);
    pcq_err_delete(r.error);
    free(err_msg);
    r.output = err_out;
  }

  pcq_cleanup(5, GrammarTotal, Grammar, Term, Factor, Base);

  pcq_optimise(r.output);

  return (st->flags & PCQA_LANG_PREDICTIVE) ? pcq_predictive(r.output) : r.output;

}

pcq_parser_t *pcqa_grammar(int flags, const char *grammar, ...) {
  pcqa_grammar_st_t st;
  pcq_parser_t *res;
  va_list va;
  va_start(va, grammar);

  st.va = &va;
  st.parsers_num = 0;
  st.parsers = NULL;
  st.flags = flags;

  res = pcqa_grammar_st(grammar, &st);
  free(st.parsers);
  va_end(va);
  return res;
}

typedef struct {
  char *ident;
  char *name;
  pcq_parser_t *grammar;
} pcqa_stmt_t;

static pcq_val_t *pcqa_stmt_afold(int n, pcq_val_t **xs) {
  pcqa_stmt_t *stmt = malloc(sizeof(pcqa_stmt_t));
  stmt->ident = ((char**)xs)[0];
  stmt->name = ((char**)xs)[1];
  stmt->grammar = ((pcq_parser_t**)xs)[3];
  (void) n;
  free(((char**)xs)[2]);
  free(((char**)xs)[4]);

  return stmt;
}

static pcq_val_t *pcqa_stmt_fold(int n, pcq_val_t **xs) {

  int i;
  pcqa_stmt_t **stmts = malloc(sizeof(pcqa_stmt_t*) * (n+1));

  for (i = 0; i < n; i++) {
    stmts[i] = xs[i];
  }
  stmts[n] = NULL;

  return stmts;
}

static void pcqa_stmt_list_delete(pcq_val_t *x) {

  pcqa_stmt_t **stmts = x;

  while(*stmts) {
    pcqa_stmt_t *stmt = *stmts;
    free(stmt->ident);
    free(stmt->name);
    pcq_soft_delete(stmt->grammar);
    free(stmt);
    stmts++;
  }
  free(x);

}

static pcq_val_t *pcqa_stmt_list_apply_to(pcq_val_t *x, void *s) {

  pcqa_grammar_st_t *st = s;
  pcqa_stmt_t *stmt;
  pcqa_stmt_t **stmts = x;
  pcq_parser_t *left;

  while(*stmts) {
    stmt = *stmts;
    left = pcqa_grammar_find_parser(stmt->ident, st);
    if (st->flags & PCQA_LANG_PREDICTIVE) { stmt->grammar = pcq_predictive(stmt->grammar); }
    if (stmt->name) { stmt->grammar = pcq_expect(stmt->grammar, stmt->name); }
    pcq_optimise(stmt->grammar);
    pcq_define(left, stmt->grammar);
    free(stmt->ident);
    free(stmt->name);
    free(stmt);
    stmts++;
  }

  free(x);

  return NULL;
}

static pcq_err_t *pcqa_lang_st(pcq_input_t *i, pcqa_grammar_st_t *st) {

  pcq_result_t r;
  pcq_err_t *e;
  pcq_parser_t *Lang, *Stmt, *Grammar, *Term, *Factor, *Base;

  Lang    = pcq_new("lang");
  Stmt    = pcq_new("stmt");
  Grammar = pcq_new("grammar");
  Term    = pcq_new("term");
  Factor  = pcq_new("factor");
  Base    = pcq_new("base");

  pcq_define(Lang, pcq_apply_to(
    pcq_total(pcq_predictive(pcq_many(pcqa_stmt_fold, Stmt)), pcqa_stmt_list_delete),
    pcqa_stmt_list_apply_to, st
  ));

  pcq_define(Stmt, pcq_and(5, pcqa_stmt_afold,
    pcq_tok(pcq_ident()), pcq_maybe(pcq_tok(pcq_string_lit())), pcq_sym(":"), Grammar, pcq_sym(";"),
    free, free, free, pcq_soft_delete
  ));

  pcq_define(Grammar, pcq_and(2, pcqaf_grammar_or,
      Term,
      pcq_maybe(pcq_and(2, pcqf_snd_free, pcq_sym("|"), Grammar, free)),
      pcq_soft_delete
  ));

  pcq_define(Term, pcq_many1(pcqaf_grammar_and, Factor));

  pcq_define(Factor, pcq_and(2, pcqaf_grammar_repeat,
    Base,
      pcq_or(6,
        pcq_sym("*"),
        pcq_sym("+"),
        pcq_sym("?"),
        pcq_sym("!"),
        pcq_tok_brackets(pcq_int(), free),
        pcq_pass()),
    pcq_soft_delete
  ));

  pcq_define(Base, pcq_or(5,
    pcq_apply_to(pcq_tok(pcq_string_lit()), pcqaf_grammar_string, st),
    pcq_apply_to(pcq_tok(pcq_char_lit()),   pcqaf_grammar_char, st),
    pcq_tok(pcq_and(3, pcqaf_fold_regex, pcq_regex_lit(), pcq_many(pcqf_strfold, pcq_oneof("ms")), pcq_lift_val(st), free, free)),
    pcq_apply_to(pcq_tok_braces(pcq_or(2, pcq_digits(), pcq_ident()), free), pcqaf_grammar_id, st),
    pcq_tok_parens(Grammar, pcq_soft_delete)
  ));

  pcq_optimise(Lang);
  pcq_optimise(Stmt);
  pcq_optimise(Grammar);
  pcq_optimise(Term);
  pcq_optimise(Factor);
  pcq_optimise(Base);

  if (!pcq_parse_input(i, Lang, &r)) {
    e = r.error;
  } else {
    e = NULL;
  }

  pcq_cleanup(6, Lang, Stmt, Grammar, Term, Factor, Base);

  return e;
}

pcq_err_t *pcqa_lang_file(int flags, FILE *f, ...) {
  pcqa_grammar_st_t st;
  pcq_input_t *i;
  pcq_err_t *err;

  va_list va;
  va_start(va, f);

  st.va = &va;
  st.parsers_num = 0;
  st.parsers = NULL;
  st.flags = flags;

  i = pcq_input_new_file("<pcqa_lang_file>", f);
  err = pcqa_lang_st(i, &st);
  pcq_input_delete(i);

  free(st.parsers);
  va_end(va);
  return err;
}

pcq_err_t *pcqa_lang_pipe(int flags, FILE *p, ...) {
  pcqa_grammar_st_t st;
  pcq_input_t *i;
  pcq_err_t *err;

  va_list va;
  va_start(va, p);

  st.va = &va;
  st.parsers_num = 0;
  st.parsers = NULL;
  st.flags = flags;

  i = pcq_input_new_pipe("<pcqa_lang_pipe>", p);
  err = pcqa_lang_st(i, &st);
  pcq_input_delete(i);

  free(st.parsers);
  va_end(va);
  return err;
}

pcq_err_t *pcqa_lang(int flags, const char *language, ...) {

  pcqa_grammar_st_t st;
  pcq_input_t *i;
  pcq_err_t *err;

  va_list va;
  va_start(va, language);

  st.va = &va;
  st.parsers_num = 0;
  st.parsers = NULL;
  st.flags = flags;

  i = pcq_input_new_string("<pcqa_lang>", language);
  err = pcqa_lang_st(i, &st);
  pcq_input_delete(i);

  free(st.parsers);
  va_end(va);
  return err;
}

pcq_err_t *pcqa_lang_contents(int flags, const char *filename, ...) {

  pcqa_grammar_st_t st;
  pcq_input_t *i;
  pcq_err_t *err;

  va_list va;

  FILE *f = fopen(filename, "rb");

  if (f == NULL) {
    err = pcq_err_file(filename, "Unable to open file!");
    return err;
  }

  va_start(va, filename);

  st.va = &va;
  st.parsers_num = 0;
  st.parsers = NULL;
  st.flags = flags;

  i = pcq_input_new_file(filename, f);
  err = pcqa_lang_st(i, &st);
  pcq_input_delete(i);

  free(st.parsers);
  va_end(va);

  fclose(f);

  return err;
}

static int pcq_nodecount_unretained(pcq_parser_t* p, int force) {

  int i, total;

  if (p->retained && !force) { return 0; }

  if (p->type == PCQ_TYPE_EXPECT) { return 1 + pcq_nodecount_unretained(p->data.expect.x, 0); }

  if (p->type == PCQ_TYPE_APPLY)    { return 1 + pcq_nodecount_unretained(p->data.apply.x, 0); }
  if (p->type == PCQ_TYPE_APPLY_TO) { return 1 + pcq_nodecount_unretained(p->data.apply_to.x, 0); }
  if (p->type == PCQ_TYPE_PREDICT)  { return 1 + pcq_nodecount_unretained(p->data.predict.x, 0); }

  if (p->type == PCQ_TYPE_CHECK)    { return 1 + pcq_nodecount_unretained(p->data.check.x, 0); }
  if (p->type == PCQ_TYPE_CHECK_WITH) { return 1 + pcq_nodecount_unretained(p->data.check_with.x, 0); }

  if (p->type == PCQ_TYPE_NOT)   { return 1 + pcq_nodecount_unretained(p->data.not.x, 0); }
  if (p->type == PCQ_TYPE_MAYBE) { return 1 + pcq_nodecount_unretained(p->data.not.x, 0); }

  if (p->type == PCQ_TYPE_MANY)  { return 1 + pcq_nodecount_unretained(p->data.repeat.x, 0); }
  if (p->type == PCQ_TYPE_MANY1) { return 1 + pcq_nodecount_unretained(p->data.repeat.x, 0); }
  if (p->type == PCQ_TYPE_COUNT) { return 1 + pcq_nodecount_unretained(p->data.repeat.x, 0); }

  if (p->type == PCQ_TYPE_OR) {
    total = 1;
    for(i = 0; i < p->data.or.n; i++) {
      total += pcq_nodecount_unretained(p->data.or.xs[i], 0);
    }
    return total;
  }

  if (p->type == PCQ_TYPE_AND) {
    total = 1;
    for(i = 0; i < p->data.and.n; i++) {
      total += pcq_nodecount_unretained(p->data.and.xs[i], 0);
    }
    return total;
  }

  return 1;

}

void pcq_stats(pcq_parser_t* p) {
  printf("Stats\n");
  printf("=====\n");
  printf("Node Count: %i\n", pcq_nodecount_unretained(p, 1));
}

static void pcq_optimise_unretained(pcq_parser_t *p, int force) {

  int i, n, m;
  pcq_parser_t *t;

  if (p->retained && !force) { return; }

  /* Optimise Subexpressions */

  if (p->type == PCQ_TYPE_EXPECT)     { pcq_optimise_unretained(p->data.expect.x, 0); }
  if (p->type == PCQ_TYPE_APPLY)      { pcq_optimise_unretained(p->data.apply.x, 0); }
  if (p->type == PCQ_TYPE_APPLY_TO)   { pcq_optimise_unretained(p->data.apply_to.x, 0); }
  if (p->type == PCQ_TYPE_CHECK)      { pcq_optimise_unretained(p->data.check.x, 0); }
  if (p->type == PCQ_TYPE_CHECK_WITH) { pcq_optimise_unretained(p->data.check_with.x, 0); }
  if (p->type == PCQ_TYPE_PREDICT)    { pcq_optimise_unretained(p->data.predict.x, 0); }
  if (p->type == PCQ_TYPE_NOT)        { pcq_optimise_unretained(p->data.not.x, 0); }
  if (p->type == PCQ_TYPE_MAYBE)      { pcq_optimise_unretained(p->data.not.x, 0); }
  if (p->type == PCQ_TYPE_MANY)       { pcq_optimise_unretained(p->data.repeat.x, 0); }
  if (p->type == PCQ_TYPE_MANY1)      { pcq_optimise_unretained(p->data.repeat.x, 0); }
  if (p->type == PCQ_TYPE_COUNT)      { pcq_optimise_unretained(p->data.repeat.x, 0); }

  if (p->type == PCQ_TYPE_OR) {
    for(i = 0; i < p->data.or.n; i++) {
      pcq_optimise_unretained(p->data.or.xs[i], 0);
    }
  }

  if (p->type == PCQ_TYPE_AND) {
    for(i = 0; i < p->data.and.n; i++) {
      pcq_optimise_unretained(p->data.and.xs[i], 0);
    }
  }

  /* Perform optimisations */

  while (1) {

    /* Merge rhs `or` */
    if (p->type == PCQ_TYPE_OR
    &&  p->data.or.xs[p->data.or.n-1]->type == PCQ_TYPE_OR
    && !p->data.or.xs[p->data.or.n-1]->retained) {
      t = p->data.or.xs[p->data.or.n-1];
      n = p->data.or.n; m = t->data.or.n;
      p->data.or.n = n + m - 1;
      p->data.or.xs = realloc(p->data.or.xs, sizeof(pcq_parser_t*) * (n + m -1));
      memmove(p->data.or.xs + n - 1, t->data.or.xs, m * sizeof(pcq_parser_t*));
      free(t->data.or.xs); free(t->name); free(t);
      continue;
    }

    /* Merge lhs `or` */
    if (p->type == PCQ_TYPE_OR
    &&  p->data.or.xs[0]->type == PCQ_TYPE_OR
    && !p->data.or.xs[0]->retained) {
      t = p->data.or.xs[0];
      n = p->data.or.n; m = t->data.or.n;
      p->data.or.n = n + m - 1;
      p->data.or.xs = realloc(p->data.or.xs, sizeof(pcq_parser_t*) * (n + m -1));
      memmove(p->data.or.xs + m, p->data.or.xs + 1, (n - 1) * sizeof(pcq_parser_t*));
      memmove(p->data.or.xs, t->data.or.xs, m * sizeof(pcq_parser_t*));
      free(t->data.or.xs); free(t->name); free(t);
      continue;
    }

    /* Remove ast `pass` */
    if (p->type == PCQ_TYPE_AND
    &&  p->data.and.n == 2
    &&  p->data.and.xs[0]->type == PCQ_TYPE_PASS
    && !p->data.and.xs[0]->retained
    &&  p->data.and.f == pcqf_fold_ast) {
      t = p->data.and.xs[1];
      pcq_delete(p->data.and.xs[0]);
      free(p->data.and.xs); free(p->data.and.dxs); free(p->name);
      memcpy(p, t, sizeof(pcq_parser_t));
      free(t);
      continue;
    }

    /* Merge ast lhs `and` */
    if (p->type == PCQ_TYPE_AND
    &&  p->data.and.f == pcqf_fold_ast
    &&  p->data.and.xs[0]->type == PCQ_TYPE_AND
    && !p->data.and.xs[0]->retained
    &&  p->data.and.xs[0]->data.and.f == pcqf_fold_ast) {
      t = p->data.and.xs[0];
      n = p->data.and.n; m = t->data.and.n;
      p->data.and.n = n + m - 1;
      p->data.and.xs = realloc(p->data.and.xs, sizeof(pcq_parser_t*) * (n + m - 1));
      p->data.and.dxs = realloc(p->data.and.dxs, sizeof(pcq_dtor_t) * (n + m - 1 - 1));
      memmove(p->data.and.xs + m, p->data.and.xs + 1, (n - 1) * sizeof(pcq_parser_t*));
      memmove(p->data.and.xs, t->data.and.xs, m * sizeof(pcq_parser_t*));
      for (i = 0; i < p->data.and.n-1; i++) { p->data.and.dxs[i] = (pcq_dtor_t)pcq_ast_delete; }
      free(t->data.and.xs); free(t->data.and.dxs); free(t->name); free(t);
      continue;
    }

    /* Merge ast rhs `and` */
    if (p->type == PCQ_TYPE_AND
    &&  p->data.and.f == pcqf_fold_ast
    &&  p->data.and.xs[p->data.and.n-1]->type == PCQ_TYPE_AND
    && !p->data.and.xs[p->data.and.n-1]->retained
    &&  p->data.and.xs[p->data.and.n-1]->data.and.f == pcqf_fold_ast) {
      t = p->data.and.xs[p->data.and.n-1];
      n = p->data.and.n; m = t->data.and.n;
      p->data.and.n = n + m - 1;
      p->data.and.xs = realloc(p->data.and.xs, sizeof(pcq_parser_t*) * (n + m -1));
      p->data.and.dxs = realloc(p->data.and.dxs, sizeof(pcq_dtor_t) * (n + m - 1 - 1));
      memmove(p->data.and.xs + n - 1, t->data.and.xs, m * sizeof(pcq_parser_t*));
      for (i = 0; i < p->data.and.n-1; i++) { p->data.and.dxs[i] = (pcq_dtor_t)pcq_ast_delete; }
      free(t->data.and.xs); free(t->data.and.dxs); free(t->name); free(t);
      continue;
    }

    /* Remove re `lift` */
    if (p->type == PCQ_TYPE_AND
    &&  p->data.and.n == 2
    &&  p->data.and.xs[0]->type == PCQ_TYPE_LIFT
    &&  p->data.and.xs[0]->data.lift.lf == pcqf_ctor_str
    && !p->data.and.xs[0]->retained
    &&  p->data.and.f == pcqf_strfold) {
      t = p->data.and.xs[1];
      pcq_delete(p->data.and.xs[0]);
      free(p->data.and.xs); free(p->data.and.dxs); free(p->name);
      memcpy(p, t, sizeof(pcq_parser_t));
      free(t);
      continue;
    }

    /* Merge re lhs `and` */
    if (p->type == PCQ_TYPE_AND
    &&  p->data.and.f == pcqf_strfold
    &&  p->data.and.xs[0]->type == PCQ_TYPE_AND
    && !p->data.and.xs[0]->retained
    &&  p->data.and.xs[0]->data.and.f == pcqf_strfold) {
      t = p->data.and.xs[0];
      n = p->data.and.n; m = t->data.and.n;
      p->data.and.n = n + m - 1;
      p->data.and.xs = realloc(p->data.and.xs, sizeof(pcq_parser_t*) * (n + m - 1));
      p->data.and.dxs = realloc(p->data.and.dxs, sizeof(pcq_dtor_t) * (n + m - 1 - 1));
      memmove(p->data.and.xs + m, p->data.and.xs + 1, (n - 1) * sizeof(pcq_parser_t*));
      memmove(p->data.and.xs, t->data.and.xs, m * sizeof(pcq_parser_t*));
      for (i = 0; i < p->data.and.n-1; i++) { p->data.and.dxs[i] = free; }
      free(t->data.and.xs); free(t->data.and.dxs); free(t->name); free(t);
      continue;
    }

    /* Merge re rhs `and` */
    if (p->type == PCQ_TYPE_AND
    &&  p->data.and.f == pcqf_strfold
    &&  p->data.and.xs[p->data.and.n-1]->type == PCQ_TYPE_AND
    && !p->data.and.xs[p->data.and.n-1]->retained
    &&  p->data.and.xs[p->data.and.n-1]->data.and.f == pcqf_strfold) {
      t = p->data.and.xs[p->data.and.n-1];
      n = p->data.and.n; m = t->data.and.n;
      p->data.and.n = n + m - 1;
      p->data.and.xs = realloc(p->data.and.xs, sizeof(pcq_parser_t*) * (n + m -1));
      p->data.and.dxs = realloc(p->data.and.dxs, sizeof(pcq_dtor_t) * (n + m - 1 - 1));
      memmove(p->data.and.xs + n - 1, t->data.and.xs, m * sizeof(pcq_parser_t*));
      for (i = 0; i < p->data.and.n-1; i++) { p->data.and.dxs[i] = free; }
      free(t->data.and.xs); free(t->data.and.dxs); free(t->name); free(t);
      continue;
    }

    return;

  }

}

void pcq_optimise(pcq_parser_t *p) {
  pcq_optimise_unretained(p, 1);
}

