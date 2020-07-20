#ifndef LEXER_H
#define LEXER_H

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include "vec.h"

// @TODO: move tokens into own file(s)

enum token_type {
  t_return,
  t_identifier,
  t_literal, // only numbers for now, TODO: split into multiple types
  t_lparen,
  t_rparen,
  t_lbrace,
  t_rbrace,
  t_comma,
  t_EOF,
  t_unknown
};

typedef struct token_pos {
  char *filename;
  size_t row; // AKA line
  size_t col; // AKA character
  size_t line_start;
  size_t len;
} token_pos;

// TODO: add file index
typedef struct token {
  enum token_type type;
  token_pos pos;
  union {
    vec *str;   // identifer, str literal
    int integer; // num literal
  } val;
} token;

char *token_location(token t);
char *token_str(token t);
char *token_type_str(enum token_type t);
vec *lex(char *filename);

#endif
