#include "parser.h"
#include "lexer.h"
#include "compiler_compat.h"

int tp1 = 0;
int gbr = 0x100000;
int bp;
int sp;
Globalentry global_table[256];
int gt_count;
Record symtab[1024];
Structentry struct_table[256];
int st_count = -1;

Token* peek()
{
  return &tokens[tp1];
}

int match(int t)
{
  if(t == tokens[tp1].type)return 1;
  return 0;
}

Token* advance()
{
  if(tokens[tp1].type != T_EOF)
  {
    tp1++;
    printf("Advancing from %d\n",tokens[tp1-1].type);
    return &tokens[tp1-1];
  }
  //need to handel the else part 
}
Token* consume(int t)
{
  if(match(t))
  {
    tp1++;
    printf("consuming %d\n",t);
    return &tokens[tp1-1];  
  }
  printf("Parser Error: Expected token type %d but got %d at index %d\n", t, tokens[tp1].type, tp1);
  exit(1);
  //need to handle the error part properly instead of just exiting 
}


void add_to_symtab(Token* t, int bp, int isg, int gt,int data_type,int lvl)
{
  strcpy(symtab[sp].name, gettokenname(t));
  symtab[sp].type = data_type;
  symtab[sp].offset = bp;
  symtab[sp].gt_index = gt;
  symtab[sp].is_global = isg;
  symtab[sp].ptrlvl = lvl;
  sp++;
}

int find_struct(char* name) {
    for (int i = 0; i <= st_count; i++) {
        if (strcmp(struct_table[i].name, name) == 0) return i;
    }
    return -1;
}


Node* new_node(NodeType t) {
    Node* node = calloc(1,sizeof(Node)); 
    node->type = t;
    return node;
}

Node* new_bin_node(int op, Node* left, Node* right) {
    Node* node = new_node(NODE_BIN);
    node->bin.op = op;
    node->bin.left = left;
    node->bin.right = right;
    return node;
}

int is_type(int t)
{
  if(t == K_INT || t == K_CHAR || t == K_VOID || t== K_STRUCT)return 1;
  if(t == T_IDENTIFIER)
  {
    char* name = gettokenname(peek());
    if(find_struct(name) != -1) {
      free(name);
      return 1;
    }
    free(name);
  }
  return 0;
}

void append_node(Node* node)
{
  Node* n = node;
  while (n && n->type == NODE_POINTER) {
      n = n->unary.expr;
  }
  if (n->type == NODE_FUNC || n->type == NODE_GVAR) 
  {
    strcpy(global_table[gt_count].name, n->func.name);
    global_table[gt_count].p = n;
    global_table[gt_count].id = gt_count;
    gt_count++;
  }
  else 
  {
    printf("Error : Type not Listed \n");
  }
}


int find_symbol(char* name) 
{
  for (int i = sp - 1; i >= 0; i--) 
  {
    if (strcmp(symtab[i].name, name) == 0) return i; 
  }
  return -1; 
}

int get_precedence(int t) {
  switch(t) {
    case ')':
    case ';': 
    case '}':
    case 0:
      return 0;
    case T_INC:
    case T_DEC:
    case '(':
    case '[':
    case '.':
    case T_ARROW:  
      return 80;
    case '*':
    case '/':
    case '%':
      return 60;
    case '+':
    case '-':
      return 40;
    case '<':
    case '>':
    case T_LEQ:   
    case T_GEQ:   
      return 30;
    case T_EQ:     
    case T_NEQ:    
      return 20;
    case '=':
      return 0;
     
    default:
      return 0;
  }
}


Node* nud()
{
  printf("Got into nud!\n");
  Token* t = peek();
  if(t->type == '*')
  {
    advance();
    Node* n = new_node(NODE_POINTER);
    n->token_id = tp1-1;
    n->unary.expr = parse_expression(90);
    return n; 
  }
  else if(t->type == '-')
  {
    advance();
    Node* n = new_node(NODE_BIN);
    n->bin.op = '-';
    Node* zero = new_node(NODE_INT);
    zero->int_val = 0;
    n->bin.left = zero;
    n->bin.right = parse_expression(90); 
    return n;
  }
  else if(t->type == T_INC || t->type == T_DEC)
  {
    advance();
    Node* n = new_node(t->type == T_INC ? NODE_PREINC : NODE_PREDEC);
    n->token_id = tp1 - 1;
    n->unary.expr = parse_expression(70);
    return n;
  }
  else if(t->type == '&')
  {
    advance();
    Node* n = new_node(NODE_ADDR);
    n->token_id = tp1-1;
    n->unary.expr = parse_expression(90);
    return n;
  }
  if(t->type == T_INT)
  {
    
    Node* n = new_node(NODE_INT);
    n->token_id = tp1;
    n->int_val = atoi(gettokenname(t));
    advance();
    
    return n;
  }
  if(t->type == '(')
  {
    advance(); // Consume the opening '(' 
    Node* inner_expr = parse_expression(0); 
    consume(')'); // Ensure there is a matching closing ')'
    return inner_expr; // Return the inner expression node directly!
  }
  if(t->type == T_CHAR)
  {
    Node* n = new_node(NODE_INT);
    n->token_id = tp1;
    char* raw_char = gettokenname(t);
    if (raw_char[1] == '\\') 
    {
      if (raw_char[2] == 'n') n->int_val = 10;  
      else if (raw_char[2] == 't') n->int_val = 9;  
      else if (raw_char[2] == '0') n->int_val = 0;  
      else n->int_val = raw_char[2];
    }
    else n->int_val = raw_char[1];
    free(raw_char);
    advance();
    
    return n;
  }
  if(t->type == T_IDENTIFIER)
  {  
    printf("in nud -> is T_IDENTIFIER\n");
    int id = find_symbol(gettokenname(t));
    Node* n;
    if (id != -1 && symtab[id].is_global) {
      n = new_node(NODE_GVAR);
      n->gvar.offset = symtab[id].offset;
      n->gvar.name = _strdup(symtab[id].name);
      n->gvar.dt = symtab[id].type;
    }
    else 
    {
      n = new_node(NODE_VAR);
      if (id == -1) { n->var.offset = 0; }
      else {n->var.offset = symtab[id].offset;n->var.dt = symtab[id].type;}
    }
    n->token_id = tp1;
    advance();
    Node* current_node = n;
    
    while(match('['))
    {
      consume('[');
      Node* array_node = new_node(NODE_ARR_ACCESS);
      current_node->indnxt = array_node;
      array_node->unary.expr = parse_expression(0);
      consume(']');
      current_node = array_node;
    }
    
    return n;
  }
  if(t->type == '"' || t->type == T_STRING)
  {
    printf("in nud -> is T_STRING\n");
    Node* n = new_node(NODE_STR);
    n->token_id = tp1;     
    n->str.string = strdup(gettokenname(t));
    advance(); 
    return n;
  }
  printf("Parser Panic: Unexpected token in nud() -> %d\n", t->type);
  exit(1);
}

void print_tree(Node* n, int depth) {
    if (!n) return;

    // Indentation for scannable visualization
    for (int i = 0; i < depth; i++) printf("  ");
    printf("DEBUG: Node Type is %d | ", n->type);

    switch (n->type) {
        case NODE_FUNC:
            printf("[FUNCTION DEF] Name: %s (Ret Type: %d)\n", n->func.name, n->func.returntype);
            if (n->func.args) {
                for (int i = 0; i < depth + 1; i++) printf("  ");
                printf("Params:\n");
                print_tree(n->func.args, depth + 2);
            }
            print_tree(n->func.body, depth + 1);
            break;

        case NODE_INT:
            printf("[INT] Value: %ld\n", n->int_val);
            break;

        case NODE_GVAR:
            printf("[GVAR] Name: %s Type: %d Offset: 0x%X (TokenID: %d)\n", n->gvar.name, n->gvar.dt, n->gvar.offset, n->token_id);
            if (n->gvar.value) print_tree(n->gvar.value, depth + 1);
            break;

        case NODE_VAR:
            printf("[VAR] DataType: %d | Offset: %d (TokenID: %d)\n", n->var.dt, n->var.offset, n->token_id);
            if (n->var.value) print_tree(n->var.value, depth + 1);
            break;

        case NODE_BIN:
            printf("[BIN_OP] Operator: '%c' (Type: %d)\n", n->bin.op, n->bin.op);
            print_tree(n->bin.left, depth + 1);
            print_tree(n->bin.right, depth + 1);
            break;

        case NODE_CALL:
            printf("[CALL] Function: %s\n", n->func.name);
            if (n->func.args) {
                for (int i = 0; i < depth + 1; i++) printf("  ");
                printf("Args:\n");
                print_tree(n->func.args, depth + 2);
            }
            break;

        case NODE_BLOCK:
            // Blocks just hold the head of a statement list. 
            // We just print a marker and let the sequential chain print the children.
            printf("[BLOCK]\n");
            print_tree(n->func.body, depth + 1); 
            break;

        case NODE_IF:
            printf("[IF STATEMENT]\n");
            for (int i = 0; i < depth + 1; i++) printf("  ");
            printf("Condition:\n");
            print_tree(n->flow.cond, depth + 2);
            
            for (int i = 0; i < depth + 1; i++) printf("  ");
            printf("Then:\n");
            print_tree(n->flow.then_stmt, depth + 2);
            
            if (n->flow.else_stmt) {
                for (int i = 0; i < depth + 1; i++) printf("  ");
                printf("Else:\n");
                print_tree(n->flow.else_stmt, depth + 2);
            }
            break;

        case NODE_WHILE:
            printf("[WHILE LOOP]\n");
            for (int i = 0; i < depth + 1; i++) printf("  ");
            printf("Condition:\n");
            print_tree(n->flow.cond, depth + 2);
            
            for (int i = 0; i < depth + 1; i++) printf("  ");
            printf("Body:\n");
            print_tree(n->flow.then_stmt, depth + 2);
            break;

        case NODE_ASSIGN:
            printf("[ASSIGN] Var (TokenID: %d) Offset: %d\n", n->token_id, n->var.offset);
            print_tree(n->var.value, depth + 1);
            break;

        case NODE_RETURN:
            printf("[RETURN]\n");
            if (n->ret.value) {
                print_tree(n->ret.value, depth + 1);
            }
            break;

        case NODE_STR:
            printf("[STRING] TokenID: %d\n", n->token_id);
            break;

        default:
            printf("[NODE] Unknown Type: %d\n", n->type);
            break;
    }

    // Traverses sibling statements sequentially.
    // If the child was a block, its statements are linked via next, so they flow down nicely.
    if (n->next) {
        print_tree(n->next, depth);
    }
}

Node* led(Node* left)
{
  printf("Got into lud!\n");
  Token* op = advance();

  if(op->type == '(')
  {
    Node* node = new_node(NODE_CALL);
    Token* name_token = &tokens[left->token_id]; 
    node->func.name = _strdup(gettokenname(name_token));
    Node** args = &(node->func.args);
    int call_arg_count = 0;
    while (!match(')')) 
    {
      call_arg_count++;
      *args  = parse_expression(0);
      args = &((*args)->next);
      if(match(','))consume(',');
    }
    consume(')');
    node->func.argcount = call_arg_count;
    return node;
  }
  else if(op->type == '.' || op->type == T_ARROW)
  {
    Node* node = new_node(NODE_MEM_ACCESS);
    node->token_id = left->token_id;
    node->mem.op = op->type;
    node->mem.par_expr = left;

    Token* memtok = consume(T_IDENTIFIER);
    char* name = gettokenname(memtok);

    int structid = -1;
    if(left->type == NODE_VAR || left->type == NODE_GVAR)
    {
      char* varname = gettokenname(&tokens[left->token_id]);
      int symidx = find_symbol(varname);
      if(symidx != -1 && symtab[symidx].struct_id!=-1)
      {
        structid = symtab[symidx].struct_id;
      }
    }
    else if(left->type == NODE_MEM_ACCESS)
    {
      structid = left->mem.struct_id;
    }
    if(structid == -1) 
    {
      printf("Error: Left side is not a struct object.\n");
      exit(1);
    }
    int memidx = -1;
    for(int i =0;i< struct_table[structid].count; i++)
    {
      if(strcmp(struct_table[structid].mem_name[i],name) == 0)
      {
        memidx = i;
        break;
      }
    }
    node->mem.offset = struct_table[structid].offset[memidx];
    if (struct_table[structid].mem_type[memidx] == K_STRUCT) 
    {
      node->mem.struct_id = find_struct(struct_table[structid].mem_struct_name[memidx]);
      node->mem.dt = K_STRUCT;  
    }
    else{ 
      node->mem.struct_id = -1;
      node->mem.dt = struct_table[structid].mem_type[memidx];
    }
    
    return node;
  }
  else if(op->type == T_INC || op->type == T_DEC)
  {
    Node* n = new_node(op->type == T_INC ? NODE_POSTINC : NODE_POSTDEC);
    n->token_id = op->type;
    n->unary.expr = left;
    return n;
  }
  else 
  {
    Node* node = new_node(NODE_BIN);
    node->bin.op = op->type;
    node->bin.left = left;
    printf(" currently in led ,get_precedence for the token %d is %d\n",op->type, get_precedence(op->type));
    node->bin.right = parse_expression(get_precedence(op->type));
    if(node->bin.left) node->bin.left->next = NULL;
    return node;
  }
}

Node* parse_expression(int p)
{
  printf("Entering parse_expression\n");
  Node* left = nud();
  printf("Current in parse_expression,  precedence for %d is %d\n ", peek()->type,get_precedence(peek()->type));
  while(get_precedence(peek()->type) > p)left = led(left);
  return left;
}



Node* parse_block()
{
  printf("enterign parse block\n");
  consume('{');
  Node* block = new_node(NODE_BLOCK);
  Node* head = NULL;
  Node* curr = NULL;

  while(!match('}'))
  {
    Node* s = parse_statement();
    if (head == NULL) 
    {
      head = s;
      curr = head;
      while(curr->next)curr = curr->next;
    } 
    else 
    {
      curr->next = s; 
      curr = s;
      while(curr->next)curr = curr->next;
    }
  }
  consume('}');
  block->func.body = head;
  printf("Ending block\n");
  
  return block;
}

Node* parse_return()
{
  consume(K_RETURN);
  Node* n = new_node(NODE_RETURN);
  n->ret.value = NULL;
  if(!match(';'))
  {
    n->ret.value = parse_expression(0);
  }
  consume(';');
  return n;
}


Node* parse_function() 
{
    printf("Entering parse func\n");
    if (!is_type(peek()->type)) exit(1); 
    Token* rt = advance(); 
    Token* f_tok = consume(T_IDENTIFIER); 
    Node* n = new_node(NODE_FUNC);
    n->func.name = gettokenname(f_tok);
    n->func.returntype = rt->type;
    add_to_symtab(f_tok, gt_count, 1,gt_count,rt->type,0);
    bp = -12; 
    consume('(');
    Node** arg_ptr = &(n->func.args); 
    int curr_gt_ind = gt_count;
    int argcount = 0;
    while (!match(')')) {
        if (is_type(peek()->type)) {
            Token* type_tok = advance(); 

            int struct_id = -1;
            if(type_tok->type == K_STRUCT || type_tok->type == T_IDENTIFIER)
            {
              if(type_tok->type == K_STRUCT)type_tok= consume(T_IDENTIFIER);
              struct_id = find_struct(gettokenname(type_tok));
              if (struct_id == -1) 
              {
                printf("Error: struct %s undefined in function parameters\n", gettokenname(type_tok));
                exit(1);
              }
            }
            
            int count = 0;
            while(match('*'))
            {
                consume('*');
                count++;
            }
            
            Token* a_tok = consume(T_IDENTIFIER);
            
            add_to_symtab(a_tok, bp,0,curr_gt_ind,type_tok->type,count);
             int id = find_symbol(gettokenname(a_tok));
            if (id != -1){symtab[id].struct_id = struct_id;}
            bp -= 4;

            *arg_ptr = new_node(NODE_VAR);
            (*arg_ptr)->token_id = tp1-1;
            (*arg_ptr)->var.dt = type_tok->type;            
            arg_ptr = &((*arg_ptr)->next);
        }
        argcount++;
        if (match(',')) advance(); 
    }
    consume(')');
    n->func.argcount = argcount;
    if(match(';'))
    {  consume(';');n->func.body = NULL;}
    else{
      bp = 0;
      n->func.body = parse_block(); 
      n->func.localvarbyte = bp;
    }
    return n;
}

Node* parse_declaration()
{
  printf("Entering parse dec\n");
  if(!is_type(peek()->type)) exit(1);
   
  Token* rt = advance();
  int base_type = rt->type;
  int curroffset = 0;
  Node* head_node = NULL;
  Node* prev_node = NULL;
  int base_size = 4;
  int is_struct =0;
  int struct_id = -1;
  if(base_type == K_STRUCT || base_type == T_IDENTIFIER)
  {
    is_struct = 1;
    if(base_type == K_STRUCT)
    {
      rt = consume(T_IDENTIFIER);
    }
    struct_id = find_struct(gettokenname(rt));
    if(struct_id == -1)exit(1);  
    base_size = struct_table[struct_id].size;
  }
  
  while(1)
  {
    int count = 0;
    int ft = base_type;
    while(match('*'))
    {
      consume('*');
      count++;
    }
    if(count > 0)base_size = 4;
    Token* d_tok = consume(T_IDENTIFIER); 
    int tid = tp1-1;
    Node* n = new_node(NODE_VAR);
    int isarray = 0;
    int dc = 0;
    int te = 1;
    int thisoffset = bp + curroffset;
    n->var.dt = ft;
    n->var.offset = thisoffset;
    n->token_id = tid; // Index of d_tok

    
    
    add_to_symtab(d_tok,thisoffset,0,gt_count,ft,count);
    int id = find_symbol(gettokenname(d_tok));

    while(match('['))
    {
      consume('[');
      if(match(']'))
      {
        if (dc > 0) {
          printf("Multidimensional arrays must specify concrete bounds for inner dimensions", peek()->line);
          exit(0);
        }
        consume(']');
        symtab[id].dim_size[0]  = 0;
        dc++;
      }
      else 
      {
        int currsize = atoi(gettokenname(consume(T_INT)));
        consume(']');
        symtab[id].dim_size[dc] = currsize;
        dc++;
        te = te*currsize;
      }
      isarray = 1;
    }
    
    
    if(peek()->type == '=')
    {
      advance();
      if(match('{'))
      {
        consume('{');
        int valcount = 0;
        Node* head = NULL; 
        Node* curr = NULL;
        while(!match('}'))
        {
          Node* temp = new_node(NODE_INT);
          temp->unary.expr = parse_expression(0);
          if(match(','))consume(',');
          
          if(head == NULL)
          {
            head = temp;
            curr = temp;
          }
          else
          {
            curr->next = temp;
            curr = temp;
          }
          
          valcount++;
        }
        if(symtab[id].dim_size[0]==0)
        {
          symtab[id].dim_size[0] = valcount;
          te = valcount;
        }
        consume('}');
        n->var.value = head;
      }
      else n->var.value = parse_expression(0);
    }
    curroffset += base_size*te;
    
    symtab[id].dim_cnt = dc;
    symtab[id].size = te;
    symtab[id].isarray = isarray;
    symtab[id].offset = thisoffset;
    symtab[id].struct_id = struct_id;
    n->next = NULL;
    if(head_node == NULL) {
       head_node = n;
     } else {
       prev_node->next = n;
     }
     prev_node = n;

     if(match(',')) 
     {
       consume(',');
     } 
     else if(peek()->type == ';') 
     {
       break; 
     } 
     else 
     {
       printf("Parser Error: Expected ',' or ';' in declaration list\n");
       exit(1);
     }
    
  }
  bp+=curroffset;
  
  consume(';');
  prev_node->next = NULL;
  return head_node;
}

Node* parse_global_declaration()
{
  printf("Entering parse global dec\n");
  if(!is_type(peek()->type)) exit(1); 
  Token* rt = advance();
  int base_type = rt->type;

  Node* head_node = NULL;
  Node* prev_node = NULL;
  int base_size = 4;
  int is_struct = 0;
  int struct_id = -1;
  if(base_type == K_STRUCT || base_type == T_IDENTIFIER)
  {
    if(base_type == K_STRUCT)rt = consume(T_IDENTIFIER);
    struct_id = find_struct(gettokenname(rt));
    if(struct_id == -1) {
      printf("Error: struct undefined\n");
      exit(1);
    }
    base_size = struct_table[struct_id].size;
  }

  while(1)
  {
    int count = 0;
    while(match('*'))
    {
      consume('*');
      count++;
    }
    if(count>0)base_size=4;
    Token* d_tok = consume(T_IDENTIFIER); 
    Node* n = new_node(NODE_GVAR);
    n->gvar.name = gettokenname(d_tok);
    n->gvar.dt = rt->type;
    n->gvar.offset = gbr;
    n->token_id = tp1 - 1; // Index of d_tok
    add_to_symtab(d_tok,gbr,1,gt_count,base_type,count);
    int id = find_symbol(gettokenname(d_tok));
    int isarray = 0;
    int dc = 0;
    int te = 1;

    while(match('['))
    {
      consume('[');
      if(match(T_INT))
      {
        int currsize = atoi(gettokenname(consume(T_INT)));
        symtab[id].dim_size[dc] = currsize;
        te = te*currsize;
      }
      else 
      {
        symtab[id].dim_size[dc] = 0;
      }
      dc++;
      consume(']');
      
      isarray = 1;
      
    }
  
  
    if(peek()->type == '=')
    {
      advance();
      if(match('{'))
      {
        consume('{');
        int valcount = 0;
        Node* head = NULL;
        Node* curr = NULL;
        while(!match('}'))
        {
          Node* temp = new_node(NODE_INT);
          temp->unary.expr = parse_expression(0);
          if(match(','))consume(',');
          if(head == NULL)
          {
            head = temp;
            curr = temp;
          }
          else
          {
            curr->next = temp;
            curr = temp;
          }
          valcount++;
        }
        n->gvar.value = head;
        if(symtab[id].dim_size[0] == 0)
        {
          symtab[id].dim_size[0] = valcount;
          te = valcount;
        }
        consume('}');
      }
      else 
      {
        n->gvar.value = parse_expression(0);
      }
    }
    symtab[id].dim_cnt = dc;
    symtab[id].size = te;
    symtab[id].isarray = isarray;
    symtab[id].struct_id = struct_id; 
    
    
    gbr+=base_size*te;
    n->next = NULL;

    if(head_node == NULL) {
       head_node = n;
     } else {
       prev_node->next = n;
     }
     prev_node = n;

     if(match(',')) 
     {
       consume(',');
     } 
     else if(peek()->type == ';') 
     {
       break; 
     } 
     else 
     {
       printf("Parser Error: Expected ',' or ';' in declaration list\n");
       exit(1);
     }
  }
  
  
  consume(';');
  prev_node->next = NULL;
  return head_node;
}

Node* parse_if_stmt()
{
    consume(K_IF);
    Node* n = new_node(NODE_IF);
    n->token_id = tp1 - 1; // Index of K_IF
    consume('(');
    n->flow.cond = parse_expression(0);
    consume(')');
    n->flow.then_stmt = parse_statement();
    if(match(K_ELSE))
    {
      consume(K_ELSE);
      n->flow.else_stmt = parse_statement();
    }
    else n->flow.else_stmt = NULL;
    return n;
}


Node* parse_while()
{
  consume(K_WHILE);
  Node* n = new_node(NODE_WHILE);
  n->token_id = tp1 - 1; // Index of K_WHILE
  consume('(');
  n->flow.cond = parse_expression(0);
  consume(')');
  n->flow.then_stmt = parse_statement();
  n->flow.else_stmt = NULL;
  return n;
}

Node* parse_for()
{
  consume(K_FOR);
  int for_idx = tp1 - 1;
  consume('(');
  Node* init = parse_statement();
  Node* cond = parse_expression(0);
  consume(';');
  Node* post_expr = NULL;
  Node* left = parse_expression(0);
  if (peek()->type == '=')
  {
    consume('=');
    post_expr = new_node(NODE_ASSIGN);
    post_expr->token_id = left->token_id;
    char* var_name = gettokenname(&tokens[post_expr->token_id]);
    int sym_idx = find_symbol(var_name);
    if (sym_idx != -1) {
          post_expr->var.offset = symtab[sym_idx].offset;
    }
    post_expr->bin.left = left;
    post_expr->bin.op = '=';
    post_expr->bin.right = parse_expression(0);
    
  }
  else 
  post_expr = left;
  
  
  consume(')');

  Node* body = parse_statement();

  Node* wnode = new_node(NODE_WHILE);
  wnode->token_id = for_idx;
  wnode->flow.cond = cond;
  wnode->flow.isfor = 1;

  if (body->type != NODE_BLOCK) {
      Node* nblock = new_node(NODE_BLOCK);
      nblock->func.body = body;
      body = nblock;
  }

  Node* t = body->func.body;
  if (!t) {
      body->func.body = post_expr;
  } else {
      while (t->next) t = t->next;
      t->next = post_expr;
  }

  wnode->flow.then_stmt = body;
  init->next = wnode;

  Node* outer_block = new_node(NODE_BLOCK);
  outer_block->func.body = init;
  return outer_block;
}

void parse_struct_def()
{
  st_count++;
  consume(K_STRUCT);
  int f1 = 0;
  if(match(T_IDENTIFIER))
  {
    Token* name = consume(T_IDENTIFIER);
    f1 = 1;
    struct_table[st_count].name = gettokenname(name);
  }
  else 
  {
    struct_table[st_count].name = NULL;
  }
  int memcount = -1;
  int curroff = 0;
  consume('{');
  while(!match('}'))
  {
    memcount++;
    int e_size = 0;
    int isptr = 0;
    Token* type_tok = advance();
    struct_table[st_count].mem_type[memcount] = type_tok->type;
    if(type_tok->type == K_STRUCT)
    {
      Token* tname = consume(T_IDENTIFIER);
      struct_table[st_count].mem_struct_name[memcount] = gettokenname(tname);
    }
    while(match('*'))
    {
      consume('*');
      isptr += 1; 
    }
    struct_table[st_count].isptr[memcount] = isptr;
    if(isptr)e_size = 4;
    else if(type_tok->type == K_STRUCT)
    {
      int s_idx = find_struct(struct_table[st_count].mem_struct_name[memcount]);
      if (s_idx != -1) {
        e_size = struct_table[s_idx].size;
      }
      else
      {
        printf("Error: struct %s undefined\n", struct_table[st_count].mem_struct_name[memcount]);
        exit(1);
      }
    }
    else e_size = 4;
    Token* e_name = consume(T_IDENTIFIER);
    struct_table[st_count].mem_name[memcount] = gettokenname(e_name);
    struct_table[st_count].offset[memcount] = curroff;
    while(match('['))
    {
      int arr_size = 0;
      consume('[');
      while(!match(']'))
      {
        Token* t = consume(T_INT);
        arr_size = atoi(gettokenname(t));
      }
      consume(']');
      e_size *= arr_size;
    }
    consume(';');
    e_size = (e_size + 3)&~3;
    curroff += e_size;    
  }
  consume('}');
  struct_table[st_count].size = curroff;
  struct_table[st_count].count = memcount+1;
}



void parse_top_level() 
{
  init_parser();
  while (tokens[tp1].type != T_EOF) {
    if(tokens[tp1].type == K_STRUCT && (tokens[tp1 + 2].type == '{'))
    {
      parse_struct_def();
    }
    else if(tokens[tp1].type == K_TYPEDEF )
    {
      consume(K_TYPEDEF);
      if(tokens[tp1].type == K_STRUCT)
      {
        parse_struct_def();
        Token* alias = consume(T_IDENTIFIER);
        consume(';');

        if(struct_table[st_count].name == NULL || strlen(struct_table[st_count].name) == 0)
        {
          struct_table[st_count].name = gettokenname(alias);
        }
        else 
        {
          int orgid = st_count++;
          struct_table[st_count] = struct_table[orgid];
          struct_table[st_count].name = gettokenname(alias);
        }
      }
      else 
      {
        printf("Unsupported typedef type (structs only for now)\n");
        exit(1);
      }
    }
    else if (is_type(tokens[tp1].type)) {
        printf("Token is a type , enting next phase \n");
        if (tokens[tp1 + 2].type == '(') 
          append_node(parse_function()); 
        else if(tokens[tp1+2].type == '=')
        {
          append_node(parse_global_declaration());
        }
        else 
        {
          append_node(parse_global_declaration()); 
        }
    }
    else 
        tp1++;
  }
}



Node* parse_statement()
{
  printf("Got into parse statement \n");
  if (is_type(tokens[tp1].type))
  {
     return parse_declaration();
  }
  switch(tokens[tp1].type)
  {
    case K_STRUCT:
    case K_INT:
    case K_CHAR:
    case K_VOID: 
      return parse_declaration();
    case '{':
      return parse_block();
    case K_IF:
      return parse_if_stmt();
    case K_WHILE:
      return parse_while();
    case K_BREAK:
    {
      Node* n = new_node(NODE_BREAK);
      n->unary.expr = NULL;
      consume(K_BREAK);
      consume(';');
      return n;
    }
    case K_CONTINUE:
    {
      Node* n = new_node(NODE_CONTINUE);
      n->unary.expr = NULL;
      consume(K_CONTINUE);
      consume(';');
      return n;
    }
    case K_FOR:
      return parse_for();
    case K_RETURN:
      return parse_return();
      break;
    default:
      printf("going into parse_expression\n");
      Node* left = parse_expression(0);
      if (peek()->type == '=')
      {
        consume('=');
        Node* n = new_node(NODE_ASSIGN);
        Node* base_var = left;
        while (base_var && base_var->type == NODE_POINTER) {
            base_var = base_var->unary.expr;
        }
        n->token_id = base_var->token_id; // Index of var_tok

        char* var_name = gettokenname(&tokens[n->token_id]);
        int sym_idx = find_symbol(var_name);
        if (sym_idx != -1) {
            n->var.offset = symtab[sym_idx].offset; // Bind the real stack offset!
        } else {
            n->var.offset = 0;
        }
        
        n->bin.left = left;
        n->bin.op = '=';
        n->bin.right = parse_expression(0);
        consume(';');
        return n;
      }
      
      if (peek()->type == 59) {
          printf("Successfully consumed semicolon at index %d\n", tp1);
          advance(); // Move past the ';' so the next loop sees the '}'
      } else {
        printf("Parser Error: Missing semicolon\n");
        exit(1);
      }
      return left;
      
  }
}
void init_parser() {
    tp1 = 0;  
    sp = 0;  
    gt_count = 0; 
    bp = 0;  
}


//


