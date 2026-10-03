#ifndef LEXER_H_
#define LEXER_H_

#ifdef __cplusplus
extern "C" {
#endif

extern char* source;
extern int tp;

typedef enum 
{
  T_IDENTIFIER = 256,
  T_INT,
  P_INT,
  P_CHAR,
  T_CHAR,
  T_STRING,
  T_VOID,
  T_LEQ,
  T_NEQ,
  T_GEQ,
  T_EQ,
  T_ARROW,
  T_INC,
  T_DEC,
  K_IF,
  K_DO,
  K_INT,
  K_FOR,
  K_VOID,
  K_CHAR,
  K_ELSE,
  K_CASE,
  K_GOTO,
  K_LONG,
  K_AUTO,
  K_WHILE,
  K_BREAK,
  K_CONTINUE,
  K_SHORT,
  K_CONST,
  K_RETURN,
  K_EXTERN,
  K_STATIC,
  K_STRUCT,
  K_SWITCH,
  K_SIGNED,
  K_DEFAULT,
  K_TYPEDEF,
  K_UNSIGNED,
  K_VOLATILE,
  K_REGISTER,
  
  T_EOF = 0
} TokenType;

typedef struct {
  int type;
  int ind;
  int len;
  int line;
} Token;

extern Token* tokens;

Token* init_lexer(char* source_code);
char* gettokenname(Token* t);
Token next_token();

#ifdef __cplusplus
}
#endif

#endif // LEXER_H_
