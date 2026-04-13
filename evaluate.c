#include <stdio.h>
#include <stdlib.h>
#include "evaluate.h"

void print_symbols_table(SymbolsManager* manager, SymbolsTable* table, char* st_name, int depth) {
    if (!manager || !table || !table->hash) return;

    char**    identifiers = dynadict_list_indexes(table->hash);
    AnyType** storage     = (AnyType**)hash_to_list(table->hash);

    if (!identifiers || !storage) return;

    // --- Build indent ---
    char indent[128] = "";
    for (int i = 0; i < depth; i++)
        strncat(indent, "    ", sizeof(indent) - strlen(indent) - 1);

    // --- Build lines into a temp buffer first to measure width ---
    #define MAX_LINES     256
    #define MAX_LINE_LEN  256
    char lines[MAX_LINES][MAX_LINE_LEN];
    int  line_count = 0;
    int  max_width  = 0;

    // Header line
    snprintf(lines[line_count], MAX_LINE_LEN, " SYMBOLS TABLE: %s ", st_name);
    max_width = strlen(lines[line_count]);
    line_count++;

    for (int i = 0; i < dynarray_length(identifiers); i++) {
        AnyType* row = storage[i];

        // Find the variable attribute (slot 0)
        AnyType entry = row[0];

        if (entry.tag == UNDEFINED_TYPE) {
            snprintf(lines[line_count], MAX_LINE_LEN, " %s: UNDEFINED ", identifiers[i]);
            if ((int)strlen(lines[line_count]) > max_width) max_width = strlen(lines[line_count]);
            line_count++;
            continue;
        }

        if (entry.tag != VART_TYPE) continue;

        Variable var = entry.as_variable;
        char type_buf[64];
        char val_buf[64];

        // Format type
        if (var.vtype.kind == KIND_PRIMITIVE) {
            switch (var.vtype.primitive_tag) {
                case VAR_TYPE_INT:    snprintf(type_buf, sizeof(type_buf), "int");    break;
                case VAR_TYPE_FLOAT:  snprintf(type_buf, sizeof(type_buf), "float");  break;
                case VAR_TYPE_BOOL:   snprintf(type_buf, sizeof(type_buf), "bool");   break;
                case VAR_TYPE_STRING: snprintf(type_buf, sizeof(type_buf), "string"); break;
                default:              snprintf(type_buf, sizeof(type_buf), "???");     break;
            }
        } else if (var.vtype.kind == KIND_ARRAY) {
            // Build something like "int[2][3][4]"
            int offset = 0;
            switch (var.vtype.ArrType.elem_type) {
                case VAR_TYPE_INT:    offset = snprintf(type_buf, sizeof(type_buf), "int");    break;
                case VAR_TYPE_FLOAT:  offset = snprintf(type_buf, sizeof(type_buf), "float");  break;
                case VAR_TYPE_BOOL:   offset = snprintf(type_buf, sizeof(type_buf), "bool");   break;
                case VAR_TYPE_STRING: offset = snprintf(type_buf, sizeof(type_buf), "string"); break;
                default:              offset = snprintf(type_buf, sizeof(type_buf), "???");     break;
            }
            for (int d = 0; d < var.vtype.ArrType.ndims; d++) {
                offset += snprintf(type_buf + offset, sizeof(type_buf) - offset,
                                   "[%d]", var.vtype.ArrType.sizes[d]);
            }
        }

        // Format value
        if (var.vtype.kind == KIND_PRIMITIVE) {
            switch (var.vtype.primitive_tag) {
                case VAR_TYPE_INT:    snprintf(val_buf, sizeof(val_buf), "%d", var.storage.value_int);       break;
                case VAR_TYPE_FLOAT:  snprintf(val_buf, sizeof(val_buf), "%.2f", var.storage.value_float);   break;
                case VAR_TYPE_BOOL:   snprintf(val_buf, sizeof(val_buf), "%s", var.storage.value_bool ? "true" : "false"); break;
                case VAR_TYPE_STRING: snprintf(val_buf, sizeof(val_buf), "\"%s\"", var.storage.value_string ? var.storage.value_string : "null"); break;
                default:              snprintf(val_buf, sizeof(val_buf), "???"); break;
            }
        } else if (var.vtype.kind == KIND_ARRAY) {
            snprintf(val_buf, sizeof(val_buf), "ptr(%p)", var.storage.value_ptr);
        }

        snprintf(lines[line_count], MAX_LINE_LEN, " %s : %s = %s ", identifiers[i], type_buf, val_buf);
        if ((int)strlen(lines[line_count]) > max_width) max_width = strlen(lines[line_count]);
        line_count++;
    }

    // --- Draw box ---
    char bar[MAX_LINE_LEN];
    memset(bar, '-', max_width + 2);
    bar[max_width + 2] = '\0';

    printf("\n");
    printf("%s+%s+\n", indent, bar);
    for (int i = 0; i < line_count; i++) {
        if (i == 1) printf("%s+%s+\n", indent, bar);
        printf("%s| %-*s |\n", indent, max_width, lines[i]);
    }
    printf("%s+%s+\n", indent, bar);
    printf("\n");
}

void print_symbols_tree_recursive(SymbolsManager* manager, SymbolsTable* table, char* name, int depth) {
    if (!manager || !table) return;

    print_symbols_table(manager, table, name, depth);

    if (table->child)   print_symbols_tree_recursive(manager, table->child,   "Child",   depth + 1);
    if (table->sibling) print_symbols_tree_recursive(manager, table->sibling, "Sibling", depth);
}

void print_symbols_tree(SymbolsManager* manager, SymbolsTable* root, char* root_name) {
    printf("═══════════════ SCOPE TREE ═══════════════\n");
    print_symbols_tree_recursive(manager, root, root_name, 0);
    printf("══════════════════════════════════════════\n");
}

bool type_equals(Type a, Type b) {
    if (a.kind != b.kind) return false;
    if (a.size != b.size) return false;

    switch (a.kind) {
        case KIND_PRIMITIVE:
            return a.primitive_tag == b.primitive_tag;

        case KIND_ARRAY:
            if (a.ArrType.elem_type != b.ArrType.elem_type) return false;
            if (a.ArrType.ndims != b.ArrType.ndims) return false;
            for (int i = 0; i < a.ArrType.ndims; i++) {
                if (a.ArrType.sizes[i] != b.ArrType.sizes[i]) return false;
            }
            return true;

        default:
            return false;
    }
}

AnyType evaluate(SymbolsManager* manager, ASTNode* node){
    AnyType action;
    action.tag = UNDEFINED_TYPE;

    evaluate(manager, node->storage.node->left_child);

    switch(node->tag) {
        case NODE:
            switch(node->storage.node->type) {

                case STATLIST:
                    // evaluate each child in order
                    ASTNode* stmt = node->storage.node->left_child;
                    while (stmt) {
                        evaluate(manager, stmt);
                        stmt = stmt->sibling;
                    }
                    break;

                case VARDECL:
                    // initializeIdentifier
                    // insertAttribute type
                    // insertAttribute value if exists
                    ASTNode* var_type_node = node->storage.node->left_child;
                    ASTNode* identifier_node = var_type_node->sibling;
                    
                    AnyType var_type = evaluate(manager, var_type_node);
                    // OUTPUT should only be a pointer to variable type
                    AnyType identifier = evaluate(manager, identifier_node);
                    // OUTPUT should only be a string type

                    assert(identifier.tag == VALUE_TYPE);
                    assert(identifier.as_value.vv_tag == VV_STRING);
                    assert(var_type.tag == VART_TYPE);

                    char * id_char = identifier.as_value.value_string;

                    initializeIdentifier(manager, id_char);
                    insertAttribute(manager, id_char, "type", var_type);
                    if(get_children_count(*node) == 3){
                        ASTNode* expr_node = identifier_node->sibling;
                        AnyType expr = evaluate(manager, expr_node);

                        assert(type_equals(var_type.as_vart, expr.as_variable.vtype));
                        // OUTPUT should be a variable with type

                        AnyType expr_val = {.tag = VALUE_TYPE, .as_value = expr.as_variable.storage};
                        
                        insertAttribute(manager, id_char, "value", expr);
                    }
                    break;

                case VARASSIGN:

                    // evaluate rhs
                    // insertAttribute value
                    ASTNode* access_node = node->storage.node->left_child;
                    ASTNode* expr_node = var_type_node->sibling;

                    AnyType access = evaluate(manager, var_type_node);
                    // OUTPUT should only be a variable with type and pointer to location
                    AnyType expr = evaluate(manager, identifier_node);
                    // OUTPUT should be a variable

                    void* location = access.as_variable.storage.value_ptr;
                    Type access_type = access.as_variable.vtype;

                    VValue expr_value = expr.as_variable.storage;
                    Type expr_type = expr.as_variable.vtype;

                    assert(type_equals(access_type, expr_type));

                    void* source;
                    switch(expr_type.kind){
                        case KIND_PRIMITIVE:
                        case KIND_ARRAY: 
                        default:
                            printf("Kind is wrong\n");
                            assert(false);
                    }

                case FUNCDECL:
                    // insertAttribute params
                    // insertAttribute returns
                    // store body node for later call
                    break;

                case IFNODE:
                    // evaluate condition child
                    // if true  → initializeScope, evaluate body, finalizeScope
                    // if false → initializeScope, evaluate else, finalizeScope
                    break;

                case WHILENODE:
                    // initializeScope
                    // while evaluate(condition)
                    //     evaluate body
                    //     handle BREAK/CONTINUE
                    // finalizeScope
                    break;

                case FORNODE:
                    // evaluate init
                    // initializeScope
                    // while evaluate(condition)
                    //     evaluate body
                    //     evaluate step
                    //     handle BREAK/CONTINUE
                    // finalizeScope
                    break;

                case SUM:
                    // evaluate left + evaluate right
                    break;

                case SUBTRACT:
                    // evaluate left - evaluate right
                    break;

                case MULT:
                    // evaluate left * evaluate right
                    break;

                case DIVISION:
                    // evaluate left / evaluate right
                    // check div by zero
                    break;

                case EXPRESSION:
                    // evaluate operands
                    // apply LoP operator (==, >=, <=, >, <)
                    break;

                case FUNCCALL:
                    // look up function in symbols table
                    // initializeScope
                    // bind arguments to params
                    // evaluate body
                    // finalizeScope
                    // return value
                    break;

                case ARRACCESS:
                    // evaluate index
                    // bounds check against length
                    // return element
                    break;

                case ARRTYPE:
                    break;

                case PROPERTYACCESS:
                    break;

                case ARGUMENTS:
                    break;

                case PARAMETERS:
                    break;

                case UNIPARAM:
                    break;

                case BREAK:
                    break;

                case CONTINUE:
                    break;

                case RETURN:
                    // evaluate expr if exists
                    // signal return up the call stack
                    break;

                case GOTO:
                    break;

                default:
                    printf("Unknown node type\n");
                    assert(false);
                    break;
            }
            break;

        case BOX:
            switch(node->storage.box->wrapper) {
                case INT_WRAPPER:
                    break;
                case FLOAT_WRAPPER:
                    break;
                case STRING_WRAPPER:
                    break;
                case BOOL_WRAPPER:
                    break;
                case ID_WRAPPER:
                    break;
                case TYPE_WRAPPER:
                    break;
                default:
                    printf("Unknown box type\n");
                    assert(false);
                    break;
            }
            break;

        case LABEL:
            printf("There should be no labels\n");
            assert(false);
            break;

        default:
            printf("Something is wrong\n");
            assert(false);
            break;
    }

    return action;
}

//SymbolsManager* create_symbols_manager(){
    //SymbolsManager* manager = initializeSymbols(STORAGE_NAME_CAPACITY);
    //createAttribute(manager, "type",    0);
    //createAttribute(manager, "value",   1);
    //createAttribute(manager, "length",  2);
    //createAttribute(manager, "address", 3);
    //createAttribute(manager, "params",  4); 
    //createAttribute(manager, "returns", 5); 

    //return manager;
//}

//void calculate_tree(TreeManager tree_manager){
    //SymbolsManager* symbols_manager = create_symbols_manager();
    //evaluate(symbols_manager, &tree_manager.root);
//}

int main(){
    printf("Evaluate...\n");

    test_symbols_table_tree();
    return 0;
}