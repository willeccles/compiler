#include "parser.h"
#include "lexer.h"
#include "misc.h"
#include "options.h"

#include <gc.h>
#include <stdbool.h>

typedef struct token_buf {
  token *tokens;
  size_t idx;
} token_buf;

// @Todo: check for EOF
token tb_pop(token_buf *tb) {
  tb->idx++;
  return tb->tokens[tb->idx - 1];
}

token tb_peek(token_buf *tb) { return tb->tokens[tb->idx]; }

// @Bug: will make idx negative if used at start of array
void tb_unpop(token_buf *tb) { tb->idx--; }

// @Todo: truncate multiline tokens
// @Bug: Doesn't handle eof
int compare_types(token t, enum token_type ts[], char *desc) {
  size_t len = 0;
  for (int i = 0; ts[i] != t_EOF; i++) {
    if (ts[i] == t.type) {
      return 1;
    }
    len++;
  }
  // Didn't match any of the types
  if (len == 1) {
    if (desc == NULL) {
      printf("%s Expected %s, got %s\n", token_location(t),
             token_type_str(ts[0]), token_type_str(t.type));
    } else {
      printf("%s Expected %s (%s), got %s\n", token_location(t), desc,
             token_type_str(ts[0]), token_type_str(t.type));
    }
  } else {
    if (desc == NULL)
      printf("%s Expected ", token_location(t));
    else
      printf("%s Expected %s (", token_location(t), desc);
    char *spacer = ", ";
    for (int i = 0; i < len; i++) {
      // @Cleanup: dumb
      if (i + 2 == len) {
        spacer = " or ";
      } else if (i + 1 == len) {
        spacer = "";
      }
      printf("%s%s", token_type_str(ts[i]), spacer);
    }
    if (desc != NULL)
      printf(")");
    printf(", got %s\n", token_type_str(t.type));
  }
  print_token_loc(t);
  return 0;
}

// @Todo: remove the macros
// Sets T to the token
#define try_pop_types(TB, T, DESC, TYPES...)                                   \
  T = tb_pop(TB);                                                              \
  if (!compare_types(T, (enum token_type[]){TYPES, t_EOF}, DESC))              \
    exit(1);
#define try_pop_type try_pop_types

expression parse_expression(token_buf *tb, bool statement, int rbp);

expression parse_fn_call(token_buf *tb) {
  expression e;
  e.type = e_fn_call;
  struct fn_call *fnc = malloc(sizeof(struct fn_call));
  token t = tb_pop(tb);
  fnc->name = t;
  // @Todo: quit if not lparen
  tb_pop(tb); // skip lparen
  vec_init(&fnc->params);
  if (tb_pop(tb).type != t_rparen) {
    tb_unpop(tb);
    do {
      vec_push(&fnc->params, parse_expression(tb, false, 0));
      try_pop_types(tb, t, NULL, t_comma, t_rparen);
    } while (t.type != t_rparen);
  }
  e.fn_call = fnc;
  return e;
}

void statement_mode_error(bool statement, token t, char *found) {
  char *mode;
  if (statement)
    mode = "statement";
  else
    mode = "expression";

  printf("%s expected %s, found %s\n", token_location(t), mode, found);
  print_token_loc(t);
  exit(1);
}

int get_lbp(token t) {
  switch (t.str.data[0]) {
  case '+':
    return 10;
  case '*':
    return 20;
  }
}

int get_rbp(token t) {
  // placeholder, eventually we will have different rbp values for some
  // operations
  return get_lbp(t);
}

expression parse_expression(token_buf *tb, bool statement, int rbp) {
  expression e;
  token t = tb_pop(tb);
  switch (t.type) {
  // @Cleanup
  case t_if:
    if (!statement)
      statement_mode_error(statement, t, "if statement");
    // initialize struct to be an if statment
    e.type = e_if;
    e.if_stmt = malloc(sizeof(struct if_stmt));
    vec_init(&e.if_stmt->body);
    vec_init(&e.if_stmt->else_body);

    e.if_stmt->cond = parse_expression(tb, false, 0);

    token bracket;
    try_pop_type(tb, bracket, "'{'", t_lbrace);
    do {
      vec_push(&e.if_stmt->body, parse_expression(tb, true, 0));
    } while (tb_peek(tb).type != t_rbrace);
    tb_pop(tb); // pop closing brace

    if (tb_peek(tb).type == t_else) {
      tb_pop(tb); // pop else
      token bracket;
      try_pop_type(tb, bracket, "'{'", t_lbrace);
      do {
        vec_push(&e.if_stmt->else_body, parse_expression(tb, true, 0));
      } while (tb_peek(tb).type != t_rbrace);
      tb_pop(tb); // pop closing brace
    }
    break;
  case t_let:
    if (!statement)
      statement_mode_error(statement, t, "declaration");
    e.type = e_declaration;
    e.decl = malloc(sizeof(struct declaration));
    e.decl->is_const = false;
    try_pop_type(tb, e.decl->name, "variable name", t_identifier);
    try_pop_type(tb, e.decl->type_tok, "type", t_identifier);
    break;
  case t_return:
    if (!statement)
      statement_mode_error(statement, t, "return statement");
    e.type = e_return;
    expression *return_value = malloc(sizeof(expression));
    *return_value = parse_expression(tb, false, 0);
    e.exp = return_value;
    break;
  case t_identifier:                            // fn call or var
    if (tb->tokens[tb->idx].type == t_lparen) { // fn call
      tb_unpop(tb);
      e = parse_fn_call(tb);
    } else { // variable
      if (tb_peek(tb).type == t_equals) {
        if (!statement)
          statement_mode_error(statement, t, "variable assignment");
        tb_pop(tb);
        struct assignment *a = malloc(sizeof(struct assignment));
        a->name = t;
        a->value = parse_expression(tb, false, 0);

        e.type = e_assign;
        e.assign = a;
      } else {
        if (statement)
          statement_mode_error(statement, t, "variable reference");
        e.type = e_reference;
        e.tok = t;
      }
    }
    break;
  case t_literal:
    if (statement)
      statement_mode_error(statement, t, "integer literal");
    e.type = e_integer_literal;
    e.tok = t;
    break;
  default:;
    printf("??? %s %s\n", token_type_str(t.type),
           token_str(t)); // @Todo: proper error message
    exit(1);
  }

  // don't check for infix operators in a statement
  if (statement)
    return e;

  // check for infix operators
  while (tb_peek(tb).type == t_operator && get_lbp(tb_peek(tb)) > rbp) {
    token op = tb_pop(tb);

    expression op_expr;
    op_expr.type = e_fn_call;

    struct fn_call *fnc = malloc(sizeof(struct fn_call));
    fnc->name = op;

    vec_init(&fnc->params);
    vec_push(&fnc->params, e);
    vec_push(&fnc->params, parse_expression(tb, false, get_rbp(op)));

    op_expr.fn_call = fnc;
    e = op_expr;
  }
  return e;
}

// @Cleanup: clean recursive expression printer
void print_expression(expression e) {
  switch (e.type) {
  case e_declaration:
    printf("let %s %s %s\n", e.decl->name.str.data, e.decl->type_tok.str.data);
    break;
  case e_assign:
    printf("%s = ", e.assign->name.str.data,
           token_type_str(e.assign->name.type));
    print_expression(e.assign->value);
    printf("\n");
    break;
  case e_if:
    printf("if (");
    print_expression(e.if_stmt->cond);
    printf(") {\n");
    for (int i = 0; i < e.if_stmt->body.length; i++) {
      print_expression(e.if_stmt->body.data[i]);
    }
    printf("}\n");
    if (e.if_stmt->else_body.length > 0) {
      printf("else {\n");
      for (int i = 0; i < e.if_stmt->else_body.length; i++) {
        print_expression(e.if_stmt->else_body.data[i]);
      }
      printf("}\n");
    }
    break;
  case e_return:
    printf("return ");
    print_expression(*e.exp);
    printf("\n");
    break;
  case e_integer_literal:
    printf("%d", e.tok.integer);
    break;
  case e_fn_call:
    printf("%s(", e.fn_call->name.str);
    for (int i = 0; i < e.fn_call->params.length; i++) {
      print_expression(e.fn_call->params.data[i]);
      if (i < e.fn_call->params.length - 1)
        printf(", ");
    }
    printf(")");
    break;
  case e_reference:
    printf(e.tok.str.data);
    break;
  default:
    printf("???\n");
  }
}

// @Cleanup: use token_str
void print_function(function fn) {
  printf("\nname: %s\nreturn type: %s\nparams:\n", fn.name.str.data,
         fn.return_type_tok.str.data);
  for (int i = 0; i < fn.params.length; i++) {
    printf("  %s %s\n", fn.params.data[i].type_tok.str.data,
           fn.params.data[i].name.str.data);
  }
  printf("body:\n");
  for (int i = 0; i < fn.body.length; i++) {
    print_expression(fn.body.data[i]);
  }
}
// @Bug: handle EOF
vec_function parse(vec_token tokens) {
  // @Cleanup: super ugly
  token_buf tb_pre = {(token *)tokens.data, 0};
  token_buf *tb = &tb_pre;
  // Continually try to parse functions
  token t;

  vec_function fl;
  vec_init(&fl);

  while (tb->tokens[tb->idx].type != t_EOF) {
    function fn;
    vec_init(&fn.params);
    try_pop_type(tb, fn.return_type_tok, "return type", t_identifier);
    try_pop_type(tb, fn.name, "function name", t_identifier);
    try_pop_type(tb, t, "opening parenthesis", t_lparen); // opening paren

    // if next token is not closing paren, read params
    if (tb_pop(tb).type != t_rparen) {
      tb_unpop(tb);
      // Read parameters
      struct param_pair p;
      do {
        try_pop_type(tb, p.type_tok, "parameter type", t_identifier);
        try_pop_type(tb, p.name, "parameter name", t_identifier);
        vec_push(&fn.params, p);
        try_pop_types(tb, t, NULL, t_comma, t_rparen);
      } while (t.type != t_rparen);
    }

    // Start parsing the function body
    try_pop_type(tb, t, NULL, t_lbrace);
    // @Cleanup: maybe restructure into do while?
    vec_init(&fn.body);
    while (tb->tokens[tb->idx].type != t_rbrace) {
      vec_push(&fn.body, parse_expression(tb, true, 0));
    }
    tb_pop(tb);
    if (debug_ast)
      print_function(fn);
    vec_push(&fl, fn);
  }
  return fl;
}
