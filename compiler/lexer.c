#include "lexer.h"
#include "compiler_compat.h"

int tp = 0;
char* source = 0;
Token* tokens = 0;
int cap = 128;
int comp_debug = 0;

void pushToken(int Type, int ind, int len, int line)
{
  tokens[tp].type = Type;
  tokens[tp].ind = ind;
  tokens[tp].len = len;
  tokens[tp].line = line;
  tp++;
  if(tp + 1 >= cap)
  {
    cap *= 2;
    tokens = (Token*)realloc(tokens, sizeof(Token) * cap);
  }
}

int p = 0;
int line = 1;

int findlen(int i, char* code)
{
  char c = code[i];
  if(c == '-' && code[i+1] == '>') return 2;
  if((c == '<' || c == '>' || c == '=' || c == '!'|| c == '+' || c == '-') && code[i+1] == '=') return 2;
  else if((c == '+' && code[i+1] == '+') || (c == '-' && code[i+1] == '-') || (c == '>' && code[i+1] == '>') || (c == '<' && code[i+1] == '<')) return 2;
  else if(c == '(' || c == ')' || c == '{' || c == '}' || c == ';') return 1;
  else if(c == '"')
  {
    int l = 1;
    while(code[i+l] != '"' && code[i+l] != '\0') l++;
    return l + 1;
  }
  else if(c == '\'')
  {
    int l = 1;
    while(code[i+l] != '\'' && code[i+l] != '\0') l++;
    return l + 1;
  }
  else if(isdigit(c))
  {
    int l = 0;
    while(isdigit(code[i+l])) l++;
    return l;
  }
  else if(isalpha(c) || c == '_') 
  {
    int l = 0;
    while(isalpha(code[i+l]) || code[i+l] == '_' || isdigit(code[i+l])) l++;
    return l;
  }
  return 1;
}

int checknum(char* c, int i, int l)
{
  int j = 0;
  while(j <= l && isdigit(c[i+j])) j++;
  if(j > l) return 1;
  return 0;
}

TokenType tokenize(char* c, int i, int l)
{
  if(c[i] == '(' || c[i] == ')' || c[i] == '{' || c[i] == '}' || c[i] == ';')
  {
    switch(c[i])
    {
      case '(' : return '(';
      case ')' : return ')';
      case '{' : return '{';
      case '}' : return '}';
      case ';' : return ';';
      default : break;
    }
  }
  else if(c[i] == '+' && c[i+1] == '+') return T_INC;
  else if(c[i] == '-' && c[i+1] == '-') return T_DEC;
  else if((c[i] == '<' || c[i] == '>' || c[i] == '=' || c[i] == '!' || c[i] == '+' || c[i] == '-') && c[i+1] == '=')
  {
    switch(c[i])
    {
      case '<': return T_LEQ;
      case '>': return T_GEQ;
      case '=': return T_EQ;
      case '!': return T_NEQ;
    }
  }
  else if(c[i] == '"') return T_STRING;
  else if(c[i] == '\'') return T_CHAR;
  else
  {
    if(isdigit(c[i]))
    {
      int k = checknum(c, i, l);
      if(k) return T_INT;
    }
    else 
    {
      switch(l)
      {
        case 2:
          if(c[i] == 'i' && c[i+1] == 'f') return K_IF;
          if(c[i] == 'd' && c[i+1] == 'o') return K_DO;
          return T_IDENTIFIER;
        case 3:
          if(c[i] == 'f' && c[i+1] == 'o' && c[i+2] == 'r') return K_FOR;
          if(c[i] == 'i' && c[i+1] == 'n' && c[i+2] == 't') return K_INT;
          return T_IDENTIFIER;
        case 4:
          if (c[i] == 'v' && c[i+1] == 'o' && c[i+2] == 'i' && c[i+3] == 'd') return K_VOID;
          if (c[i] == 'c' && c[i+1] == 'h' && c[i+2] == 'a' && c[i+3] == 'r') return K_CHAR;
          if (c[i] == 'e' && c[i+1] == 'l' && c[i+2] == 's' && c[i+3] == 'e') return K_ELSE;
          if (c[i] == 'c' && c[i+1] == 'a' && c[i+2] == 's' && c[i+3] == 'e') return K_CASE;
          if (c[i] == 'g' && c[i+1] == 'o' && c[i+2] == 't' && c[i+3] == 'o') return K_GOTO;
          if (c[i] == 'l' && c[i+1] == 'o' && c[i+2] == 'n' && c[i+3] == 'g') return K_LONG;
          if (c[i] == 'a' && c[i+1] == 'u' && c[i+2] == 't' && c[i+3] == 'o') return K_AUTO;
          return T_IDENTIFIER;
        case 5:
          if (c[i] == 'w' && c[i+1] == 'h' && c[i+2] == 'i' && c[i+3] == 'l' && c[i+4] == 'e') return K_WHILE;
          if (c[i] == 'b' && c[i+1] == 'r' && c[i+2] == 'e' && c[i+3] == 'a' && c[i+4] == 'k') return K_BREAK;
          if (c[i] == 's' && c[i+1] == 'h' && c[i+2] == 'o' && c[i+3] == 'r' && c[i+4] == 't') return K_SHORT;
          if (c[i] == 'c' && c[i+1] == 'o' && c[i+2] == 'n' && c[i+3] == 's' && c[i+4] == 't') return K_CONST;
          return T_IDENTIFIER;
        case 6:
          if (c[i] == 'r' && c[i+1] == 'e' && c[i+2] == 't' && c[i+3] == 'u' && c[i+4] == 'r' && c[i+5] == 'n') return K_RETURN;
          if (c[i] == 'e' && c[i+1] == 'x' && c[i+2] == 't' && c[i+3] == 'e' && c[i+4] == 'r' && c[i+5] == 'n') return K_EXTERN;
          if (c[i] == 's' && c[i+1] == 't' && c[i+2] == 'a' && c[i+3] == 't' && c[i+4] == 'i' && c[i+5] == 'c') return K_STATIC;
          if (c[i] == 's' && c[i+1] == 't' && c[i+2] == 'r' && c[i+3] == 'u' && c[i+4] == 'c' && c[i+5] == 't') return K_STRUCT;
          if (c[i] == 's' && c[i+1] == 'w' && c[i+2] == 'i' && c[i+3] == 't' && c[i+4] == 'c' && c[i+5] == 'h') return K_SWITCH;
          if (c[i] == 's' && c[i+1] == 'i' && c[i+2] == 'g' && c[i+3] == 'n' && c[i+4] == 'e' && c[i+5] == 'd') return K_SIGNED;
          return T_IDENTIFIER;
        case 7:
          if (c[i] == 'd' && c[i+1] == 'e' && c[i+2] == 'f' && c[i+3] == 'a' && c[i+4] == 'u' && c[i+5] == 'l' && c[i+6] == 't') return K_DEFAULT;
          if (c[i] == 't' && c[i+1] == 'y' && c[i+2] == 'p' && c[i+3] == 'e' && c[i+4] == 'd' && c[i+5] == 'e' && c[i+6] == 'f') return K_TYPEDEF;
          return T_IDENTIFIER;
        case 8:
          if (c[i] == 'u' && c[i+1] == 'n' && c[i+2] == 's' && c[i+3] == 'i' && c[i+4] == 'g' && c[i+5] == 'n' && c[i+6] == 'e' && c[i+7] == 'd') return K_UNSIGNED;
          if (c[i] == 'v' && c[i+1] == 'o' && c[i+2] == 'l' && c[i+3] == 'a' && c[i+4] == 't' && c[i+5] == 'i' && c[i+6] == 'l' && c[i+7] == 'e') return K_VOLATILE;
          if (c[i] == 'r' && c[i+1] == 'e' && c[i+2] == 'g' && c[i+3] == 'i' && c[i+4] == 's' && c[i+5] == 't' && c[i+6] == 'e' && c[i+7] == 'r') return K_REGISTER;
          if (c[i] == 'c' && c[i+1] == 'o' && c[i+2] == 'n' && c[i+3] == 't' && c[i+4] == 'i' && c[i+5] == 'n' && c[i+6] == 'u' && c[i+7] == 'e') return K_CONTINUE;
          return T_IDENTIFIER;
        default:
          return T_IDENTIFIER;
      }
    }
  }
}

char* gettokenname(Token* t) {
    char* r = (char*)malloc(t->len + 1); 
    if (!r) return 0;
    memcpy(r, source + t->ind, t->len);
    r[t->len] = '\0';
    return r; 
}

Token* init_lexer(char* c)
{
  source = c;
  p = 0;
  line = 1;
  tp = 0;
  cap = 128;

  tokens = (Token*)malloc(sizeof(Token) * cap);

  while(c[p] != '\0')
  {
    if (c[p] == ' ' || c[p] == '\t' || c[p] == '\r') {
        p++;
        continue;
    }
    if (c[p] == '\n') {
        line++;
        p++;
        continue;
    }
    // Skip single-line comments // ...
    if (c[p] == '/' && c[p+1] == '/') {
        p += 2;
        while (c[p] != '\n' && c[p] != '\0') p++;
        continue;
    }
    int s = p;
    int l = findlen(p, c);
    int type = -1;

    if(c[p] == '+' && c[p+1] == '+') type = T_INC;
    else if(c[p] == '-' && c[p+1] == '-') type = T_DEC;
    else if(c[p] == '-' && c[p+1] == '>') type = T_ARROW;
    else if(isdigit(c[p]))
    {
      checknum(c, p, l);
      type = T_INT;
    }
    else if((c[p] == '<' || c[p] == '>' || c[p] == '=' || c[p] == '!') && c[p+1] == '=')
    {
      switch(c[p])
      {
        case '<': type = T_LEQ; break;
        case '>': type = T_GEQ; break;
        case '=': type = T_EQ;  break;
        case '!': type = T_NEQ; break;
      }
    }
    else if(isalpha(c[p]) || c[p] == '_')
    {
      type = tokenize(c, p, l);
    }
    else if(c[p] == '\'') type = T_CHAR;
    else 
    {  
      type = c[p];
    }

    pushToken(type, p, l, line);
    p += l;
  }
  pushToken(T_EOF, p, 1, line);

  return tokens;
}
