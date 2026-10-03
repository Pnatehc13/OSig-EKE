#include "sema.h"
#include "parser.h"
#include "lexer.h"
#include "compiler_compat.h"




void sema_error(char* message, int line) {
    printf("Semantic Error (Line %d): %s\n", line, message);
    exit(1);
}
Node* current_function = NULL;

TokenType get_node_type(Node* n)
{
    if(!n)return T_VOID;
    switch(n->type)
    {
        case NODE_INT: return T_INT;
        case NODE_STR : return T_STRING;
        case NODE_VAR:
        {
            char* name = gettokenname(&tokens[n->token_id]);
            int id = find_symbol(name);
            if (id == -1) sema_error("Undeclared variable context", tokens[n->token_id].line);
            return symtab[id].type + (symtab[id].ptrlvl * 1000);
        }
        case '[':
        {
            return get_node_type(n->bin.left);
        }
        case NODE_GVAR:
        {
            int id = find_symbol(n->gvar.name);
            return symtab[id].type+(symtab[id].ptrlvl * 1000);
        }
        case NODE_POINTER:
        {
            TokenType child_t = get_node_type(n->unary.expr);
            if (child_t < 1000) {
                sema_error("Cannot dereference a non-pointer type", tokens[n->token_id].line);
            }
            return child_t - 1000;
        }
        case NODE_ADDR:
        {
            TokenType child_t = get_node_type(n->unary.expr);
            return child_t + 1000;
        }
        case NODE_BIN:return T_INT;
        case NODE_MEM_ACCESS: return n->mem.dt;
        default : return T_VOID;
    }
}

int types_are_compatible(TokenType expected, TokenType actual) {
    if (expected == actual) return 1;
    
    // Allow string literals to be assigned to char pointers
    if (expected == (K_CHAR + 1000) && actual == T_STRING) return 1;
    
    int exp_base = expected % 1000;
    int act_base = actual % 1000;
    int exp_ptr  = expected / 1000;
    int act_ptr  = actual / 1000;
    
    // Pointer layers must match, and underlying primitives must align
    if (exp_ptr == act_ptr) {
        if ((exp_base == act_base) || 
            (exp_base == K_INT && act_base == T_INT) ||
            (exp_base == T_INT && act_base == K_INT) ||
            (exp_base == K_CHAR && act_base == T_INT)) {
            return 1;
        }
    }
    return 0;
}


TokenType sema_analyze(Node* n) 
{
    if(!n) return T_VOID;
    switch(n->type)
    {
        case NODE_INT:
            return T_INT;
        case NODE_STR:
            return T_STRING;
        case NODE_ASSIGN:
        {
            TokenType left_type = get_node_type(n->bin.left);
            TokenType right_type = get_node_type(n->bin.right);
            int is_string_ptr_assign = (left_type == (K_CHAR + 1000) && right_type == T_STRING);
            
            int types_match = is_string_ptr_assign || 
                              (left_type == right_type) || 
                              (left_type == K_INT && right_type == T_INT) ||
                              (left_type == T_INT && right_type == K_INT) ||
                              (left_type == K_CHAR && right_type == T_INT);
            if (!types_match) 
            {
                sema_error("Type mismatch in variable assignment", tokens[n->token_id].line);
            }
            return right_type;
        }
        case NODE_BLOCK:
        {
            sema_analyze(n->func.body);
            break;
        }
        case '=':
        {
            Node* lhs = n->bin.left;
            Node* rhs = n->bin.right;

            TokenType lhs_type = sema_analyze(lhs);
            TokenType rhs_type = sema_analyze(rhs);
            if (lhs->type != NODE_VAR && lhs->type != NODE_GVAR && (lhs->type != NODE_BIN || lhs->bin.op != '[')) {
                sema_error("Left-hand side of assignment must be a valid variable or array element", tokens[n->token_id].line);
            }

            if (lhs_type != rhs_type) {
                sema_error("Type mismatch across assignment operator", tokens[n->token_id].line);
            }

            return lhs_type;
        }
        case '[':
        {
            Node* base_var = n->bin.left;
            Node* index_expr = n->bin.right;
            TokenType index_type = sema_analyze(index_expr);
            if (index_type != T_INT) {
                sema_error("Array subscript index must resolve to an integer type", tokens[n->token_id].line);
            }
            TokenType base_type = get_node_type(base_var);
            if (base_var) {
                sema_analyze(base_var);
            }
            
            return base_type;
        }
        case NODE_FUNC:
            current_function = n;
            if(n->func.body)
                sema_analyze(n->func.body);
            current_function = NULL;
            break;

        case NODE_GVAR: 
        {
            return n->gvar.dt; 
        }
        case NODE_VAR:
        {
            char* vn = gettokenname(&tokens[n->token_id]);
            int sym_idx = find_symbol(vn);
            if(sym_idx == -1)
            {
                char err_msg[128];
                sprintf(err_msg, "Undeclared variable '%s'", vn);
                sema_error(err_msg, tokens[n->token_id].line);
            }
            TokenType my_type = symtab[sym_idx].type + (symtab[sym_idx].ptrlvl * 1000);
            if (n->var.value) 
            {
                if(n->var.value->next!=NULL)
                {
                    Node* curr = n->var.value;
                    int val = 0;

                    if(symtab[sym_idx].struct_id != -1)
                    {
                        int struct_id = symtab[sym_idx].struct_id;
                        while(curr != NULL)
                        {
                            if (val >= struct_table[struct_id].count)
                            {
                                sema_error("Excess elements in struct initializer list", tokens[n->token_id].line);
                            }
                            TokenType ct = sema_analyze(curr->unary.expr);
                            TokenType member_type = struct_table[struct_id].mem_type[val] + (struct_table[struct_id].isptr[val] * 1000);
                            if(!types_are_compatible(member_type, ct))
                            {
                                sema_error("Type mismatch inside struct initializer", tokens[n->token_id].line);
                            }
                            val++;
                            curr = curr->next;
                        }
                        
                    }
                    else 
                    {
                        while(curr!=NULL)
                        {
                            TokenType ct = sema_analyze(curr->unary.expr);
                            if(!types_are_compatible(my_type,ct))sema_error("Type mismatch inside array initializer", tokens[n->token_id].line);
                            val++;
                            curr = curr->next;
                        }
                        int limit = symtab[sym_idx].dim_size[0];
                        if(limit > 0 && val > limit)
                        {
                            sema_error("Excess elements in array initializer list", tokens[n->token_id].line);
                        }
                    }
                }
                else 
                {
                    TokenType t = sema_analyze(n->var.value);
                    TokenType my_type = symtab[sym_idx].type + (symtab[sym_idx].ptrlvl * 1000);

                    int is_string_ptr_assign = (my_type == (K_CHAR + 1000) && t == T_STRING);
                    if (!is_string_ptr_assign) 
                    {
                        int bm = my_type%1000;
                        int b = t%1000;
                        int pointer_levels_match = (my_type / 1000) == (t / 1000);
                        int base_types_match = (bm == b) || 
                                                (bm == K_INT && b == T_INT) ||
                                                (bm == T_INT && b == K_INT) ||
                                                (bm == K_CHAR && b == T_INT);
                        
                        if (!pointer_levels_match || !base_types_match) 
                        {    
                            sema_error("Type mismatch in variable initialization", tokens[n->token_id].line); 
                        }
                    }
                }
            }
            return symtab[sym_idx].type + (symtab[sym_idx].ptrlvl * 1000);
        }
        case NODE_ADDR:
        {
           TokenType t = sema_analyze(n->unary.expr);
           return t+1000;
        }

        case NODE_POINTER:
        {
            TokenType t = sema_analyze(n->unary.expr);
            if(t<1000) sema_error("Cannot dereference non-pointer type", tokens[n->token_id].line); 
            return t-1000;
        }
        
        case NODE_BIN:
        {
            TokenType left_t = sema_analyze(n->bin.left);
            TokenType right_t = sema_analyze(n->bin.right);
            if(left_t == T_STRING)
            {
                sema_error("Cannot perform such action on Strings", tokens[n->token_id].line); 
            }
            int types_match = (left_t == right_t) || 
                              (left_t == K_INT && right_t == T_INT) ||
                              (left_t == T_INT && right_t == K_INT);
                              
            if (!types_match) {
                sema_error("Type mismatch in binary operation", tokens[n->token_id].line);
            }
            return T_INT;
        }
        case NODE_MEM_ACCESS:
        {
            return sema_analyze(n->mem.par_expr);
        }
        case NODE_IF:
        {
            TokenType cond = sema_analyze(n->flow.cond);
            if (cond == T_STRING) {
                sema_error("Condition expression cannot be a String", tokens[n->token_id].line);
            }
            if (n->flow.then_stmt) {
                sema_analyze(n->flow.then_stmt);
            }
            if (n->flow.else_stmt) {
                sema_analyze(n->flow.else_stmt);
            }
            return T_VOID;
        }
        case NODE_WHILE:
        {
            TokenType cond_t = sema_analyze(n->flow.cond);
            if (cond_t == T_STRING) {
                sema_error("Loop condition expression cannot be a String", tokens[n->token_id].line);
            }
            if (n->flow.then_stmt) {
                sema_analyze(n->flow.then_stmt);
            }
            return T_VOID;
        }
        case NODE_CALL:
        {
            int target_idx = -1;
            for (int i = 0; i < gt_count; i++) {
                if (strcmp(global_table[i].name, n->func.name) == 0) {
                    target_idx = i;
                    break;
                }
            }
            if (strcmp(n->func.name, "printf") == 0) 
            {
                if (n->func.args) {
                    TokenType first_arg_t = sema_analyze(n->func.args);
                    if (first_arg_t != T_STRING) {
                        sema_error("First argument to printf must be a String literal", tokens[n->token_id].line);
                    }
                }
                return T_VOID;
            }

            if (target_idx == -1) {
                char err_msg[128];
                sprintf(err_msg, "Call to undeclared function '%s'", n->func.name);
                sema_error(err_msg, tokens[n->token_id].line);
            }

            Node* def_arg = global_table[target_idx].p->func.args; 
            Node* call_arg = n->func.args;

            while (def_arg && call_arg)
            {
                TokenType expected_t = def_arg->var.dt; // Definition nodes hold types
                TokenType passed_t = sema_analyze(call_arg);
                int match = (expected_t == passed_t) || 
                            (expected_t == K_INT && passed_t == T_INT) ||
                            (expected_t == T_INT && passed_t == K_INT);
                if (!match) 
                {
                    sema_error("Type mismatch in function argument", tokens[n->token_id].line);
                }
                def_arg = def_arg->next;
                call_arg = call_arg->next;
            }

            if (def_arg != NULL || call_arg != NULL) {
                sema_error("Argument count mismatch in function call", tokens[n->token_id].line);
            }
            return global_table[target_idx].p->func.returntype;
        }
        case NODE_RETURN:
            if (!current_function) 
            {
                sema_error("Return statement outside of any function", tokens[n->token_id].line);
            }
            TokenType expected_rt = current_function->func.returntype;
            if (n->ret.value) {
                TokenType actual_rt = sema_analyze(n->ret.value);
                int types_match = (expected_rt == actual_rt) || 
                                  (expected_rt == K_INT && actual_rt == T_INT) ||
                                  (expected_rt == T_INT && actual_rt == K_INT);
                if (!types_match) 
                {
                    char err_msg[256];
                    sprintf(err_msg, "Function '%s' expects return type %d but got %d", current_function->func.name, expected_rt, actual_rt);
                    sema_error(err_msg, tokens[n->token_id].line);
                }
            }
            else 
            {
                if (expected_rt != T_VOID) 
                {
                    char err_msg[256];
                    sprintf(err_msg, "Function '%s' must return a value", current_function->func.name);
                    sema_error(err_msg, tokens[n->token_id].line);
                }
            }
            return T_VOID;
        
        default:
            if (n->next) sema_analyze(n->next);
            break;
    }
    if (n->next) {
        sema_analyze(n->next);
    }
    return T_VOID;
    
}


void sema_run_global_analysis()
{
    printf("\n--- STARTING SEMANTIC ANALYSIS PASS ---\n");
    for (int i = 0; i < gt_count; i++) 
    {
        if (global_table[i].p != NULL) 
        {
            Node* entry = global_table[i].p;
            while(entry && entry->type == NODE_POINTER)
            {
                entry = entry->unary.expr;
            }
            sema_analyze(entry);
        }
    }
    printf("Sema Pass: Success! Abstract Syntax Tree is verified and decorated.\n");
}
