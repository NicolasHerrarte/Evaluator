#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include "evaluate.h"

#define INT_MACRO_TYPE (Type){.kind = KIND_PRIMITIVE, .primitive_tag = VAR_TYPE_INT}
#define FLOAT_MACRO_TYPE (Type){.kind = KIND_PRIMITIVE, .primitive_tag = VAR_TYPE_FLOAT}
#define BOOL_MACRO_TYPE (Type){.kind = KIND_PRIMITIVE, .primitive_tag = VAR_TYPE_BOOL}
#define STRING_MACRO_TYPE (Type){.kind = KIND_PRIMITIVE, .primitive_tag = VAR_TYPE_STRING}

#define EPSILON_FLOAT_COMP 0.0001f

typedef enum {
    LOP_EQ,
    LOP_GTE,
    LOP_LTE,
    LOP_GT,
    LOP_LT,
} LopTag;

void print_vvalue(VValue* v) {
    if (!v) { printf("(null)"); return; }
    switch (v->vv_tag) {
        case VV_INT:       printf("%d",    v->value_int);                              break;
        case VV_FLOAT:     printf("%.2f",  v->value_float);                            break;
        case VV_BOOL:      printf("%s",    v->value_bool ? "true" : "false");          break;
        case VV_STRING:    printf("\"%s\"", v->value_string ? v->value_string : "null"); break;
        case VV_PTR:       printf("ptr(%p)", v->value_ptr);                            break;
        case VV_UNDEFINED: printf("<undefined>");                                       break;
        default:           printf("???");                                               break;
    }
}

void print_symbols_table(SymbolsManager* manager, SymbolsTable* table, char* st_name, int depth) {
    if (!manager || !table || !table->hash) return;

    char**   identifiers = dynadict_list_indexes(table->hash);
    VValue** storage     = (VValue**)hash_to_list(table->hash);

    if (!identifiers || !storage) return;

    char indent[128] = "";
    for (int i = 0; i < depth; i++)
        strncat(indent, "    ", sizeof(indent) - strlen(indent) - 1);

    #define MAX_LINES    256
    #define MAX_LINE_LEN 256
    char lines[MAX_LINES][MAX_LINE_LEN];
    int  line_count = 0;
    int  max_width  = 0;

    snprintf(lines[line_count], MAX_LINE_LEN, " SYMBOLS TABLE: %s ", st_name);
    max_width = strlen(lines[line_count]);
    line_count++;

    int n     = table->hash->count;
    int nattr = manager->index_hash->count;

    char** attr_keys = dynadict_list_indexes(manager->index_hash);

    for (int i = 0; i < n; i++) {
        VValue* slots = storage[i];

        /* identifier header line */
        snprintf(lines[line_count], MAX_LINE_LEN, " %s :", identifiers[i]);
        if ((int)strlen(lines[line_count]) > max_width) max_width = strlen(lines[line_count]);
        line_count++;

        for (int a = 0; a < nattr; a++) {
            if (slots[a].vv_tag == VV_UNDEFINED) continue;

            /* reverse lookup attribute name */
            char* attr_name = NULL;
            for (int k = 0; k < nattr; k++) {
                int* idx = dynadict_get(manager->index_hash, attr_keys[k]);
                if (idx && *idx == a) { attr_name = attr_keys[k]; break; }
            }

            char val_buf[64];
            switch (slots[a].vv_tag) {
                case VV_INT:    snprintf(val_buf, sizeof(val_buf), "%d",      slots[a].value_int);                              break;
                case VV_FLOAT:  snprintf(val_buf, sizeof(val_buf), "%.2f",    slots[a].value_float);                            break;
                case VV_BOOL:   snprintf(val_buf, sizeof(val_buf), "%s",      slots[a].value_bool ? "true" : "false");          break;
                case VV_STRING: snprintf(val_buf, sizeof(val_buf), "\"%s\"",  slots[a].value_string ? slots[a].value_string : "null"); break;
                case VV_PTR:    snprintf(val_buf, sizeof(val_buf), "ptr(%p)", slots[a].value_ptr);                              break;
                default:        snprintf(val_buf, sizeof(val_buf), "???");                                                       break;
            }

            if (attr_name)
                snprintf(lines[line_count], MAX_LINE_LEN, "     %s = %s", attr_name, val_buf);
            else
                snprintf(lines[line_count], MAX_LINE_LEN, "     [%d] = %s", a, val_buf);

            if ((int)strlen(lines[line_count]) > max_width) max_width = strlen(lines[line_count]);
            line_count++;
        }
    }

    /* draw box */
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

EvalPass eval_comparison(EvalPass left, EvalPass right, int op) {
    assert(left.tag  == VARIABLE_TYPE);
    assert(right.tag == VARIABLE_TYPE);
    assert(left.as_variable.vtype.kind  == KIND_PRIMITIVE);
    assert(right.as_variable.vtype.kind == KIND_PRIMITIVE);

    TypeTag lt = left.as_variable.vtype.primitive_tag;
    TypeTag rt = right.as_variable.vtype.primitive_tag;

    EvalPass action;
    action.tag = VARIABLE_TYPE;
    action.as_variable.vtype = BOOL_MACRO_TYPE;

    // disallow everything with string
    assert(lt != VAR_TYPE_STRING && rt != VAR_TYPE_STRING);
    // disallow bool vs numeric
    assert(!((lt == VAR_TYPE_BOOL && rt != VAR_TYPE_BOOL) ||
            (rt == VAR_TYPE_BOOL && lt != VAR_TYPE_BOOL)));

    
    float lf, rf;
    bool result;

    if (lt == VAR_TYPE_STRING) {
        assert(op == LOP_EQ);
        result = strcmp(left.as_variable.storage.value_string,
                        right.as_variable.storage.value_string) == 0;
    } else {
        // int, float, bool all convert cleanly to float
        switch (lt) {
            case VAR_TYPE_FLOAT: lf = left.as_variable.storage.value_float;         break;
            case VAR_TYPE_BOOL:  lf = (float)left.as_variable.storage.value_bool;   break;
            case VAR_TYPE_INT:   lf = (float)left.as_variable.storage.value_int;    break;
            default: assert(false);
        }

        switch (rt) {
            case VAR_TYPE_FLOAT: rf = right.as_variable.storage.value_float;        break;
            case VAR_TYPE_BOOL:  rf = (float)right.as_variable.storage.value_bool;  break;
            case VAR_TYPE_INT:   rf = (float)right.as_variable.storage.value_int;   break;
            default: assert(false);
        }

        switch (op) {
            case LOP_EQ:  result = fabsf(lf - rf) < EPSILON_FLOAT_COMP; break;
            case LOP_GTE: result = lf >= rf;                  break;
            case LOP_LTE: result = lf <= rf;                  break;
            case LOP_GT:  result = lf >  rf;                  break;
            case LOP_LT:  result = lf <  rf;                  break;
            default: assert(false);
        }
    }

    action.as_variable.storage.vv_tag = VV_BOOL;
    action.as_variable.storage.value_bool = result;

    return action;
}

EvalPass eval_arithmetic(EvalPass left, EvalPass right, char op) {
    assert(left.tag  == VARIABLE_TYPE);
    assert(right.tag == VARIABLE_TYPE);
    assert(left.as_variable.vtype.kind  == KIND_PRIMITIVE);
    assert(right.as_variable.vtype.kind == KIND_PRIMITIVE);

    TypeTag lt = left.as_variable.vtype.primitive_tag;
    TypeTag rt = right.as_variable.vtype.primitive_tag;

    assert(lt == VAR_TYPE_INT || lt == VAR_TYPE_FLOAT);
    assert(rt == VAR_TYPE_INT || rt == VAR_TYPE_FLOAT);

    EvalPass action;
    action.tag = VARIABLE_TYPE;

    float lf = (lt == VAR_TYPE_FLOAT) ? left.as_variable.storage.value_float  : (float)left.as_variable.storage.value_int;
    float rf = (rt == VAR_TYPE_FLOAT) ? right.as_variable.storage.value_float : (float)right.as_variable.storage.value_int;

    if (lt == VAR_TYPE_INT && rt == VAR_TYPE_INT) {
        assert(op != '/');  // no int division, use float() cast first
        int li = left.as_variable.storage.value_int;
        int ri = right.as_variable.storage.value_int;
        action.as_variable.vtype = INT_MACRO_TYPE;

        switch (op) {
            case '+': action.as_variable.storage = (VValue){ .vv_tag = VV_INT, .value_int = li + ri }; break;
            case '-': action.as_variable.storage = (VValue){ .vv_tag = VV_INT, .value_int = li - ri }; break;
            case '*': action.as_variable.storage = (VValue){ .vv_tag = VV_INT, .value_int = li * ri }; break;
            default:  assert(false);
        }
    } else {
        action.as_variable.vtype = FLOAT_MACRO_TYPE;

        switch (op) {
            case '+': action.as_variable.storage = (VValue){ .vv_tag = VV_FLOAT, .value_float = lf + rf }; break;
            case '-': action.as_variable.storage = (VValue){ .vv_tag = VV_FLOAT, .value_float = lf - rf }; break;
            case '*': action.as_variable.storage = (VValue){ .vv_tag = VV_FLOAT, .value_float = lf * rf }; break;
            case '/': 
                assert(rf != 0.0f);
                action.as_variable.storage = (VValue){ .vv_tag = VV_FLOAT, .value_float = lf / rf }; 
                break;
            default:  assert(false);
        }
    }

    return action;
}

EvalPass eval_deep_copy(EvalPass source, Arena* dest_arena) {
    EvalPass copy = source;

    if (copy.tag != VARIABLE_TYPE) return copy;

    VValue val = copy.as_variable.storage;
    Type type = copy.as_variable.vtype;

    switch (val.vv_tag) {
        case VV_STRING: {
            if (val.value_string) {
                size_t len = strlen(val.value_string) + 1;
                char* str_copy = (char*) arena_get(dest_arena, len);
                memcpy(str_copy, val.value_string, len);
                val.value_string = str_copy;
            }
            break;
        }
        case VV_PTR: {
            if (!val.value_ptr) break;

            if (type.kind == KIND_ARRAY) {
                size_t total = 1;
                for (int d = 0; d < type.ArrType.ndims; d++) {
                    total *= type.ArrType.sizes[d];
                }

                VValue* old_data = (VValue*) val.value_ptr;
                VValue* new_data = (VValue*) arena_get(dest_arena, total * sizeof(VValue));
                memcpy(new_data, old_data, total * sizeof(VValue));

                for (size_t i = 0; i < total; i++) {
                    if (new_data[i].vv_tag == VV_STRING && new_data[i].value_string) {
                        size_t len = strlen(new_data[i].value_string) + 1;
                        char* s = (char*) arena_get(dest_arena, len);
                        memcpy(s, new_data[i].value_string, len);
                        new_data[i].value_string = s;
                    }
                }

                val.value_ptr = new_data;
            }
            break;
        }
        default:
            break;
    }

    copy.as_variable.storage = val;
    return copy;
}

void print_eval_pass(EvalPass ep) {
    switch (ep.tag) {
        case UNDEFINED_TYPE:
            printf("UNDEFINED\n");
            break;
        case VALUE_TYPE:
            printf("VALUE: ");
            print_vvalue(&ep.as_value);
            printf("\n");
            break;
        case VART_TYPE:
            printf("TYPE: kind=%d", ep.as_vart.kind);
            if (ep.as_vart.kind == KIND_PRIMITIVE) {
                printf(" primitive=%d", ep.as_vart.primitive_tag);
            } else if (ep.as_vart.kind == KIND_ARRAY) {
                printf(" elem=%d ndims=%d sizes=[", ep.as_vart.ArrType.elem_type, ep.as_vart.ArrType.ndims);
                for (int i = 0; i < ep.as_vart.ArrType.ndims; i++) {
                    printf("%d%s", ep.as_vart.ArrType.sizes[i], i < ep.as_vart.ArrType.ndims - 1 ? "," : "");
                }
                printf("]");
            }
            printf("\n");
            break;
        case VARIABLE_TYPE:
            printf("VARIABLE: type=");
            if (ep.as_variable.vtype.kind == KIND_PRIMITIVE) {
                printf("prim(%d)", ep.as_variable.vtype.primitive_tag);
            } else {
                printf("arr(elem=%d,ndims=%d)", ep.as_variable.vtype.ArrType.elem_type, ep.as_variable.vtype.ArrType.ndims);
            }
            printf(" value=");
            print_vvalue(&ep.as_variable.storage);
            printf("\n");
            break;
        case SPAR_TYPE:
            printf("PARAM: mode=%d name=%s\n", ep.as_spar.mode, ep.as_spar.name);
            break;
        case ARGS_TYPE:
            printf("ARGS: mode=%d amount=%d\n", ep.as_args.mode, ep.as_args.amount);
            break;
        case SIGNAL_TYPE:
            printf("SIGNAL: %d", ep.as_signal.signal);
            if (ep.as_signal.signal == SIGNAL_RETURN) {
                printf(" return_value=");
                print_vvalue(&ep.as_signal.variable.storage);
            }
            printf("\n");
            break;
        default:
            printf("UNKNOWN TAG %d\n", ep.tag);
            break;
    }
}

EvalPass evaluate(SymbolsManager* manager, ASTNode* node, Arena* current_arena){
    EvalPass action;
    action.tag = UNDEFINED_TYPE;
    if(node->tag == NODE){
        action.nclass = node->storage.node->type;
    }
    else{
        action.nclass = -1;
    }

    switch(node->tag) {
        case NODE:
            switch(node->storage.node->type) {

                case STATLIST: {
                    // evaluate each child in order
                    ASTNode* stmt = node->storage.node->left_child;
                    int counter = 1;
                    while (stmt != NULL) {
                        EvalPass result = evaluate(manager, stmt, current_arena);
                        //printf("Statement -> %d\n", counter);
                        //print_eval_pass(result);
                        if (result.tag == SIGNAL_TYPE) {
                            return result;
                        }
                        stmt = stmt->sibling;

                        counter ++;
                    }
                    break;
                }
                case VARDECL: {
                    // initializeIdentifier
                    // insertAttribute type
                    // insertAttribute value if exists
                    ASTNode* var_type_node = node->storage.node->left_child;
                    ASTNode* identifier_node = var_type_node->sibling;
                    
                    EvalPass var_type = evaluate(manager, var_type_node, current_arena);
                    // OUTPUT should only be a pointer to variable type
                    EvalPass identifier = evaluate(manager, identifier_node, current_arena);
                    // OUTPUT should only be a string type

                    assert(identifier.tag == VALUE_TYPE);
                    assert(identifier.as_value.vv_tag == VV_STRING);
                    assert(var_type.tag == VART_TYPE);

                    assert(var_type.as_vart.primitive_tag != VAR_TYPE_VOID);

                    char * id_char = identifier.as_value.value_string;

                    initializeIdentifier(manager, id_char);

                    Type* type_storage = (Type*) arena_get(current_arena, sizeof(Type));
                    *type_storage = var_type.as_vart;

                    setAttributeIdentifier(manager, id_char, "type", (VValue){ .vv_tag = VV_PTR, .value_ptr = type_storage});
                    if(get_children_count(*node) == 3){
                        ASTNode* expr_node = identifier_node->sibling;
                        EvalPass expr = evaluate(manager, expr_node, current_arena);
                        assert(expr.tag == VARIABLE_TYPE);
                        assert(type_equals(var_type.as_vart, expr.as_variable.vtype));
                        // OUTPUT should be a variable with type
                        
                        setAttributeIdentifier(manager, id_char, "value", expr.as_variable.storage);
                    }
                    break;
                }
                case VARASSIGN: {

                    // evaluate rhs
                    // insertAttribute value
                    ASTNode* access_node = node->storage.node->left_child;
                    ASTNode* expr_node = access_node->sibling;

                    EvalPass access = evaluate(manager, access_node, current_arena);
                    // OUTPUT should be a VValue pointer towards a *VValue
                    assert(access.tag == VALUE_TYPE);
                    assert(access.as_value.vv_tag == VV_PTR);
                    
                    VValue access_type_raw = getAttributeStorage(manager, access.as_value.value_ptr, "type");
                    assert(access_type_raw.vv_tag == VV_PTR);
                    Type access_type = *((Type*) access_type_raw.value_ptr);

                    EvalPass expr = evaluate(manager, expr_node, current_arena);
                    // OUTPUT should be a variable
                    assert(expr.tag == VARIABLE_TYPE);
                    assert(type_equals(access_type, expr.as_variable.vtype));

                    setAttributeStorage(manager, access.as_value.value_ptr, "value", expr.as_variable.storage);

                    break;
                }
                case ASSIGNSTORAGE: {
                    ASTNode* specification_node = node->storage.node->left_child;
                    EvalPass specification = evaluate(manager, specification_node, current_arena);

                    assert(specification.tag == VART_TYPE);
                    assert(specification.as_vart.kind != KIND_PRIMITIVE);

                    Type type_spec = specification.as_vart;

                    action.tag = VARIABLE_TYPE;
                    
                    if(type_spec.kind == KIND_ARRAY){
                        size_t store_size = 1;
                        for (int d = 0; d < type_spec.ArrType.ndims; d++){
                            store_size *= type_spec.ArrType.sizes[d];
                        }

                        VValue* storage = (VValue*) arena_get(current_arena, store_size*sizeof(VValue));
                        action.as_variable.vtype = type_spec;
                        action.as_variable.storage.vv_tag = VV_PTR;
                        action.as_variable.storage.value_ptr = storage;
                    }
                    else{
                        assert(false);
                    }
                    break;
                }
                case ARRINST: {
                    ASTNode* instantiation_node = node->storage.node->left_child;
                    ASTNode* size_node = instantiation_node->sibling;

                    EvalPass instantiation = evaluate(manager, instantiation_node, current_arena);
                    EvalPass size = evaluate(manager, size_node, current_arena);

                    assert(size.tag == VARIABLE_TYPE);
                    assert(size.as_variable.vtype.primitive_tag == VAR_TYPE_INT);
                    int dim_size = size.as_variable.storage.value_int;
                    assert(dim_size > 0);

                    Type arr_type;
                    arr_type.kind = KIND_ARRAY;

                    if (instantiation.nclass == ARRINST) {
                        // chained — extend existing array type
                        assert(instantiation.tag == VART_TYPE);
                        arr_type.ArrType.elem_type = instantiation.as_vart.ArrType.elem_type;
                        arr_type.ArrType.ndims     = instantiation.as_vart.ArrType.ndims + 1;
                        arr_type.ArrType.sizes[0]  = dim_size;
                        for (int d = 0; d < instantiation.as_vart.ArrType.ndims; d++)
                            arr_type.ArrType.sizes[d + 1] = instantiation.as_vart.ArrType.sizes[d];
                    } else if (instantiation.tag == VALUE_TYPE) {
                        // innermost — primitive type from @vl
                        assert(instantiation.as_value.vv_tag == VV_INT);
                        switch (instantiation.as_value.value_int) {
                            case 0: arr_type.ArrType.elem_type = VAR_TYPE_INT;    break;
                            case 1: arr_type.ArrType.elem_type = VAR_TYPE_BOOL;   break;
                            case 2: arr_type.ArrType.elem_type = VAR_TYPE_FLOAT;  break;
                            case 3: arr_type.ArrType.elem_type = VAR_TYPE_STRING; break;
                            default: assert(false);
                        }
                        arr_type.ArrType.ndims    = 1;
                        arr_type.ArrType.sizes[0] = dim_size;
                    } else {
                        assert(false);
                    }

                    action.tag = VART_TYPE;
                    action.as_vart = arr_type;
                    break;
                }
                case FUNCDECL: {
                    // insertAttribute params
                    // insertAttribute returns
                    // store body node for later call

                    ASTNode* identifier_node = node->storage.node->left_child;
                    ASTNode* params_list_node = identifier_node->sibling;
                    ASTNode* return_type_node = params_list_node->sibling;
                    ASTNode* block_node = return_type_node->sibling;

                    EvalPass identifier = evaluate(manager, identifier_node, current_arena);
                    EvalPass params_list = evaluate(manager, params_list_node, current_arena);
                    EvalPass return_type = evaluate(manager, return_type_node, current_arena);

                    assert(identifier.tag == VALUE_TYPE);
                    assert(identifier.as_value.vv_tag == VV_STRING);
                    assert(params_list.tag == ARGS_TYPE);
                    assert(params_list.as_args.mode == PARAMETER_MODE);
                    assert(return_type.tag == VART_TYPE);

                    char* id_char = identifier.as_value.value_string;

                    initializeIdentifier(manager, id_char);

                    Type* type_storage = (Type*) arena_get(current_arena, sizeof(Type));
                    *type_storage = return_type.as_vart;
                    setAttributeIdentifier(manager, id_char, "type", (VValue){ .vv_tag = VV_PTR, .value_ptr = type_storage});

                    Args* args_storage = (Args*) arena_get(current_arena, sizeof(Args));
                    *args_storage = params_list.as_args;
                    setAttributeIdentifier(manager, id_char, "params", (VValue){ .vv_tag = VV_PTR, .value_ptr = args_storage });

                    setAttributeIdentifier(manager, id_char, "value", (VValue){ .vv_tag = VV_PTR, .value_ptr = block_node });

                    break;
                }
                case IFNODE: {
                    // evaluate condition child
                    // if true  → initializeScope, evaluate body, finalizeScope
                    // if false → initializeScope, evaluate else, finalizeScope

                    ASTNode* cond_node = node->storage.node->left_child;
                    ASTNode* body_node = cond_node->sibling;

                    EvalPass cond = evaluate(manager, cond_node, current_arena);
                    assert(cond.tag == VARIABLE_TYPE);
                    assert(cond.as_variable.vtype.primitive_tag == VAR_TYPE_BOOL);

                    if (cond.as_variable.storage.value_bool) {
                        initializeScope(manager, DYNADICT_DEFAULT_CAPACITY);
                        EvalPass body = evaluate(manager, body_node, current_arena);
                        if (body.tag == SIGNAL_TYPE){
                            action = body;
                        }
                        finalizeScope(manager);
                    } else if (get_children_count(*node) == 3) {
                        ASTNode* else_node = body_node->sibling;
                        initializeScope(manager, DYNADICT_DEFAULT_CAPACITY);
                        EvalPass myelse = evaluate(manager, else_node, current_arena);
                        if (myelse.tag == SIGNAL_TYPE){
                            action = myelse;
                        }
                        finalizeScope(manager);
                    }

                    break;
                }
                case WHILENODE: {
                    ASTNode* cond_node = node->storage.node->left_child;
                    ASTNode* body_node = cond_node->sibling;

                    initializeScope(manager, DYNADICT_DEFAULT_CAPACITY);
                    while (true) {
                        EvalPass cond = evaluate(manager, cond_node, current_arena);
                        assert(cond.tag == VARIABLE_TYPE);
                        assert(cond.as_variable.vtype.primitive_tag == VAR_TYPE_BOOL);

                        if (!cond.as_variable.storage.value_bool) break;

                        EvalPass body = evaluate(manager, body_node, current_arena);
                        if (body.tag == SIGNAL_TYPE) {
                            if (body.as_signal.signal == SIGNAL_BREAK)    break;
                            if (body.as_signal.signal == SIGNAL_CONTINUE) continue;
                            if (body.as_signal.signal == SIGNAL_RETURN){
                                action = body;
                                break;
                            }
                        }
                    }
                    finalizeScope(manager);

                    break;
                }
                case FORNODE: {
                    ASTNode* init_node = node->storage.node->left_child;
                    ASTNode* cond_node = init_node->sibling;
                    ASTNode* step_node = cond_node->sibling;
                    ASTNode* body_node = step_node->sibling;

                    evaluate(manager, init_node, current_arena);
                    initializeScope(manager, DYNADICT_DEFAULT_CAPACITY);
                    while (true) {
                        EvalPass cond = evaluate(manager, cond_node, current_arena);
                        assert(cond.tag == VARIABLE_TYPE);
                        assert(cond.as_variable.vtype.primitive_tag == VAR_TYPE_BOOL);

                        if (!cond.as_variable.storage.value_bool) break;

                        EvalPass body = evaluate(manager, body_node, current_arena);
                        if (body.tag == SIGNAL_TYPE) {
                            if (body.as_signal.signal == SIGNAL_BREAK) break;
                            if (body.as_signal.signal == SIGNAL_CONTINUE){
                                evaluate(manager, step_node, current_arena);
                                continue;
                            }
                            if (body.as_signal.signal == SIGNAL_RETURN){
                                action = body;
                                break;
                            }
                        }

                        evaluate(manager, step_node, current_arena);
                    }
                    finalizeScope(manager);

                    break;
                }
                case SUM: {
                    // evaluate left + evaluate right
                    ASTNode* left_expr_node = node->storage.node->left_child;
                    ASTNode* right_expr_node = left_expr_node->sibling;

                    EvalPass left_expr = evaluate(manager, left_expr_node, current_arena);
                    EvalPass right_expr = evaluate(manager, right_expr_node, current_arena);

                    action = eval_arithmetic(left_expr, right_expr, '+');
                    
                    break;
                }
                case SUBTRACT: {
                    // evaluate left - evaluate right
                    ASTNode* left_expr_node = node->storage.node->left_child;
                    ASTNode* right_expr_node = left_expr_node->sibling;

                    EvalPass left_expr = evaluate(manager, left_expr_node, current_arena);
                    EvalPass right_expr = evaluate(manager, right_expr_node, current_arena);

                    action = eval_arithmetic(left_expr, right_expr, '-');
                    break;
                }
                case MULT: {
                    // evaluate left * evaluate right
                    ASTNode* left_expr_node = node->storage.node->left_child;
                    ASTNode* right_expr_node = left_expr_node->sibling;

                    EvalPass left_expr = evaluate(manager, left_expr_node, current_arena);
                    EvalPass right_expr = evaluate(manager, right_expr_node, current_arena);

                    action = eval_arithmetic(left_expr, right_expr, '*');
                    break;
                }
                case DIVISION: {
                    // evaluate left / evaluate right
                    // check div by zero
                    ASTNode* left_expr_node = node->storage.node->left_child;
                    ASTNode* right_expr_node = left_expr_node->sibling;

                    EvalPass left_expr = evaluate(manager, left_expr_node, current_arena);
                    EvalPass right_expr = evaluate(manager, right_expr_node, current_arena);

                    action = eval_arithmetic(left_expr, right_expr, '/');
                    break;
                }
                case LOGICAND : {
                    ASTNode* left_expr_node = node->storage.node->left_child;
                    ASTNode* right_expr_node = left_expr_node->sibling;
                    
                    EvalPass left_expr = evaluate(manager, left_expr_node, current_arena);
                    EvalPass right_expr = evaluate(manager, right_expr_node, current_arena);

                    assert(left_expr.tag == VARIABLE_TYPE);
                    assert(right_expr.tag == VARIABLE_TYPE);
                    assert(left_expr.as_variable.vtype.primitive_tag == VAR_TYPE_BOOL);
                    assert(right_expr.as_variable.vtype.primitive_tag == VAR_TYPE_BOOL);

                    bool and_op = left_expr.as_variable.storage.value_bool && right_expr.as_variable.storage.value_bool;

                    action.tag = VARIABLE_TYPE;
                    action.as_variable.vtype = BOOL_MACRO_TYPE;
                    action.as_variable.storage = (VValue){ .vv_tag = VV_BOOL, .value_bool = and_op};
                    break;
                }
                case LOGICOR : {
                    ASTNode* left_expr_node = node->storage.node->left_child;
                    ASTNode* right_expr_node = left_expr_node->sibling;
                    
                    EvalPass left_expr = evaluate(manager, left_expr_node, current_arena);
                    EvalPass right_expr = evaluate(manager, right_expr_node, current_arena);

                    assert(left_expr.tag == VARIABLE_TYPE);
                    assert(right_expr.tag == VARIABLE_TYPE);
                    assert(left_expr.as_variable.vtype.primitive_tag == VAR_TYPE_BOOL);
                    assert(right_expr.as_variable.vtype.primitive_tag == VAR_TYPE_BOOL);

                    bool and_op = left_expr.as_variable.storage.value_bool || right_expr.as_variable.storage.value_bool;

                    action.tag = VARIABLE_TYPE;
                    action.as_variable.vtype = BOOL_MACRO_TYPE;
                    action.as_variable.storage = (VValue){ .vv_tag = VV_BOOL, .value_bool = and_op};
                    break;
                }
                case COMPNODE: {
                    // evaluate operands
                    // apply RelOP operator (==, >=, <=, >, <)
                    ASTNode* left_expr_node = node->storage.node->left_child;
                    ASTNode* logical_operator_node = left_expr_node->sibling;
                    ASTNode* right_expr_node = logical_operator_node->sibling;

                    EvalPass left_expr = evaluate(manager, left_expr_node, current_arena);
                    EvalPass logical_operator = evaluate(manager, logical_operator_node, current_arena);
                    EvalPass right_expr = evaluate(manager, right_expr_node, current_arena);

                    assert(logical_operator.as_value.vv_tag == VV_INT);
                    action = eval_comparison(left_expr, right_expr, logical_operator.as_value.value_int);
                    break;
                }
                case FUNCCALL: {
                    // look up function in symbols table
                    // initializeScope
                    // bind arguments to params
                    // evaluate body
                    // finalizeScope
                    // return value

                    ASTNode* access_node = node->storage.node->left_child;
                    ASTNode* args_node = access_node->sibling;

                    EvalPass access = evaluate(manager, access_node, current_arena);
                    EvalPass args = evaluate(manager, args_node, current_arena);

                    // access gives us a VV_PTR to the symbol storage
                    assert(access.tag == VALUE_TYPE);
                    assert(access.as_value.vv_tag == VV_PTR);
                    assert(args.tag == ARGS_TYPE);
                    assert(args.as_args.mode == ARGUMENT_MODE);

                    // retrieve function info from symbol table
                    VValue* func_storage = (VValue*) access.as_value.value_ptr;

                    VValue params_raw = getAttributeStorage(manager, func_storage, "params");
                    assert(params_raw.vv_tag == VV_PTR);
                    Args* params = (Args*) params_raw.value_ptr;

                    VValue body_raw = getAttributeStorage(manager, func_storage, "value");
                    assert(body_raw.vv_tag == VV_PTR);
                    ASTNode* body_node = (ASTNode*) body_raw.value_ptr;

                    VValue return_type_raw = getAttributeStorage(manager, func_storage, "type");
                    assert(return_type_raw.vv_tag == VV_PTR);
                    Type return_type = *((Type*) return_type_raw.value_ptr);

                    // check arg count matches param count
                    assert(args.as_args.amount == params->amount);

                    // new scope + bind args to param names
                    initializeScope(manager, DYNADICT_DEFAULT_CAPACITY);

                    // sub arena for function
                    Arena* local_inner_arena = arena_create(LOCAL_ARENA_SIZE);

                    for (int i = 0; i < params->amount; i++) {
                        // type check each argument against parameter
                        assert(type_equals(args.as_args.list[i].argtype, params->list[i].argtype));

                        char* param_name = params->list[i].name;
                        initializeIdentifier(manager, param_name);

                        Type* type_store = (Type*) arena_get(local_inner_arena, sizeof(Type));
                        *type_store = params->list[i].argtype;

                        setAttributeIdentifier(manager, param_name, "type", (VValue){ .vv_tag = VV_PTR, .value_ptr = type_store });
                        setAttributeIdentifier(manager, param_name, "value", args.as_args.list[i].value);
                    }

                    // evaluate body
                    EvalPass result = evaluate(manager, body_node, local_inner_arena);

                    if (return_type.kind == KIND_PRIMITIVE && return_type.primitive_tag == VAR_TYPE_VOID) {
                        action.tag = UNDEFINED_TYPE;
                    } else {
                        assert(result.tag == SIGNAL_TYPE);
                        assert(result.as_signal.signal == SIGNAL_RETURN);
                        assert(result.as_signal.variable.storage.vv_tag != VV_UNDEFINED);

                        action.tag = VARIABLE_TYPE;
                        action.as_variable = result.as_signal.variable;
                        action = eval_deep_copy(action, current_arena);

                        assert(type_equals(action.as_variable.vtype, return_type));
                    }

                    arena_destroy(local_inner_arena);

                    finalizeScope(manager);
                    
                    break;
                }
                case ARRACCESS: {
                    ASTNode* access_node = node->storage.node->left_child;
                    ASTNode* expr_node = access_node->sibling;

                    EvalPass access = evaluate(manager, access_node, current_arena);
                    EvalPass expr = evaluate(manager, expr_node, current_arena);

                    assert(expr.tag == VARIABLE_TYPE);
                    assert(expr.as_variable.vtype.primitive_tag == VAR_TYPE_INT);

                    int idx = expr.as_variable.storage.value_int;

                    Type curr_type;
                    VValue* base_ptr;

                    if (access.nclass == ARRACCESS) {
                        assert(access.tag == VARIABLE_TYPE);
                        assert(access.as_variable.vtype.kind == KIND_ARRAY);
                        curr_type = access.as_variable.vtype;
                        base_ptr = (VValue*) access.as_variable.storage.value_ptr;
                    } else if (access.tag == VALUE_TYPE && access.as_value.vv_tag == VV_PTR) {
                        VValue* storage_ptr = (VValue*) access.as_value.value_ptr;

                        VValue type_slot = getAttributeStorage(manager, storage_ptr, "type");
                        assert(type_slot.vv_tag == VV_PTR);
                        curr_type = *((Type*) type_slot.value_ptr);
                        assert(curr_type.kind == KIND_ARRAY);

                        VValue value_slot = getAttributeStorage(manager, storage_ptr, "value");
                        assert(value_slot.vv_tag == VV_PTR);
                        base_ptr = (VValue*) value_slot.value_ptr;
                    } else {
                        assert(false);
                    }

                    // bounds check
                    assert(idx >= 0 && idx < curr_type.ArrType.sizes[0]);

                    // compute stride
                    int stride = 1;
                    for (int d = 1; d < curr_type.ArrType.ndims; d++)
                        stride *= curr_type.ArrType.sizes[d];

                    VValue* elem_ptr = &base_ptr[idx * stride];

                    if (curr_type.ArrType.ndims == 1) {
                        // last dimension → unpack actual value into variable
                        Type prim_type = (Type){ .kind = KIND_PRIMITIVE, .primitive_tag = curr_type.ArrType.elem_type };

                        VValue val;
                        switch (curr_type.ArrType.elem_type) {
                            case VAR_TYPE_INT:    val = (VValue){ .vv_tag = VV_INT,    .value_int    = elem_ptr->value_int    }; break;
                            case VAR_TYPE_FLOAT:  val = (VValue){ .vv_tag = VV_FLOAT,  .value_float  = elem_ptr->value_float  }; break;
                            case VAR_TYPE_BOOL:   val = (VValue){ .vv_tag = VV_BOOL,   .value_bool   = elem_ptr->value_bool   }; break;
                            case VAR_TYPE_STRING: val = (VValue){ .vv_tag = VV_STRING, .value_string = elem_ptr->value_string }; break;
                            default: assert(false);
                        }

                        action.tag = VARIABLE_TYPE;
                        action.as_variable.vtype = prim_type;
                        action.as_variable.storage = val;
                    } else if (curr_type.ArrType.ndims > 1) {
                        Type reduced;
                        reduced.kind              = KIND_ARRAY;
                        reduced.ArrType.elem_type = curr_type.ArrType.elem_type;
                        reduced.ArrType.ndims     = curr_type.ArrType.ndims - 1;
                        for (int d = 0; d < reduced.ArrType.ndims; d++)
                            reduced.ArrType.sizes[d] = curr_type.ArrType.sizes[d + 1];

                        action.tag               = VARIABLE_TYPE;
                        action.as_variable.vtype   = reduced;
                        action.as_variable.storage = (VValue){ .vv_tag = VV_PTR, .value_ptr = elem_ptr };
                    } else {
                        assert(false);
                    }

                    break;
                }
                case PRIMITIVETYPE: {
                    ASTNode* prim_spec_node = node->storage.node->left_child;

                    EvalPass prim_spec = evaluate(manager, prim_spec_node, current_arena);

                    assert(prim_spec.as_value.vv_tag == VV_INT);
                    
                    Type prim_type;
                    prim_type.kind = KIND_PRIMITIVE;
                    
                    switch (prim_spec.as_value.value_int) {
                            case 0: prim_type.primitive_tag = VAR_TYPE_INT;    break;
                            case 1: prim_type.primitive_tag = VAR_TYPE_BOOL;   break;
                            case 2: prim_type.primitive_tag = VAR_TYPE_FLOAT;  break;
                            case 3: prim_type.primitive_tag = VAR_TYPE_STRING; break;
                            case 4: prim_type.primitive_tag = VAR_TYPE_VOID;  break;
                            default: assert(false);
                    }

                    action.tag = VART_TYPE;
                    action.as_vart = prim_type;
                    break;
                }
                case ARRTYPE: {
                    ASTNode* inner_node = node->storage.node->left_child;
                    EvalPass inner = evaluate(manager, inner_node, current_arena);

                    assert(inner.tag == VART_TYPE);

                    Type arr_type;
                    arr_type.kind = KIND_ARRAY;

                    if(inner.as_vart.kind == KIND_PRIMITIVE){
                        assert(inner.as_vart.primitive_tag != VAR_TYPE_VOID);

                        arr_type.ArrType.elem_type = inner.as_vart.primitive_tag;
                        arr_type.ArrType.ndims = 1;
                        arr_type.ArrType.sizes[0] = 0;
                    }
                    else if(inner.as_vart.kind == KIND_ARRAY){
                        arr_type.ArrType.elem_type = inner.as_vart.ArrType.elem_type;
                        arr_type.ArrType.ndims = inner.as_vart.ArrType.ndims + 1;
                        arr_type.ArrType.sizes[arr_type.ArrType.ndims] = 0;
                    }
                    else {
                        assert(false);
                    }

                    action.tag = VART_TYPE;
                    action.as_vart = arr_type;
                    break;
                }
                case PROPERTYACCESS:
                    assert(false);
                    break;

                case ARGUMENTS: {
                    ASTNode* curr_expr = node->storage.node->left_child;
                    Arena* g_arena = manager->global_arena;

                    int children_count = get_children_count(*curr_expr);

                    Args fargs;
                    fargs.mode = ARGUMENT_MODE;
                    fargs.amount = children_count;
                    fargs.list = arena_get(g_arena, children_count*sizeof(Argument));

                    for(int i = 0;i<children_count;i++){
                        EvalPass expr = evaluate(manager, curr_expr, current_arena);
                        assert(expr.tag == VARIABLE_TYPE);

                        Argument sarg;
                        sarg.mode = ARGUMENT_MODE;
                        sarg.argtype = expr.as_variable.vtype;
                        sarg.value = expr.as_variable.storage;

                        fargs.list[i] = sarg;

                        curr_expr = curr_expr->sibling;
                    }
                    action.tag = ARGS_TYPE;
                    action.as_args = fargs;
                    break;
                }
                case PARAMETERS: {
                    ASTNode* param = node->storage.node->left_child;
                    Arena* g_arena = manager->global_arena;

                    Args fpars;
                    int children_count = get_children_count(*param);

                    fpars.amount = children_count;
                    fpars.list = arena_get(g_arena, children_count*sizeof(Argument));
                    fpars.mode = PARAMETER_MODE;

                    for(int i = 0;i<children_count;i++){
                        EvalPass spar = evaluate(manager, param, current_arena);
                        assert(spar.tag == SPAR_TYPE);
                        assert(spar.as_spar.mode == PARAMETER_MODE);

                        fpars.list[i] = spar.as_spar;

                        param = param->sibling;
                    }

                    action.tag = ARGS_TYPE;
                    action.as_args = fpars;
                    break;
                }
                case UNIPARAM: {
                    ASTNode* identifier_node = node->storage.node->left_child;
                    ASTNode* param_type_node = identifier_node->sibling;

                    EvalPass identifier = evaluate(manager, identifier_node, current_arena);
                    EvalPass param = evaluate(manager, param_type_node, current_arena);

                    assert(identifier.tag == IDENTIFIER_TYPE);
                    assert(param.tag == VART_TYPE);

                    Argument new_argument;

                    new_argument.mode = PARAMETER_MODE;
                    new_argument.name = identifier.as_identifier.name;
                    new_argument.argtype = param.as_vart;

                    action.tag = SPAR_TYPE;
                    action.as_spar = new_argument;
                    break;
                }
                case BREAK: {
                    action.tag = SIGNAL_TYPE;
                    action.as_signal.signal = SIGNAL_BREAK;
                    break;
                }
                case CONTINUE: {
                    action.tag = SIGNAL_TYPE;
                    action.as_signal.signal = SIGNAL_CONTINUE;
                    break;
                }
                case RETURN:
                    action.tag = SIGNAL_TYPE;
                    action.as_signal.signal = SIGNAL_RETURN;

                    if (get_children_count(*node) > 0) {
                        ASTNode* expr_node = node->storage.node->left_child;
                        EvalPass expr = evaluate(manager, expr_node, current_arena);
                        assert(expr.tag == VARIABLE_TYPE);
                        action.as_signal.variable = expr.as_variable;
                    } else {
                        action.as_signal.variable.vtype = (Type){ .kind = KIND_PRIMITIVE, .primitive_tag = VAR_TYPE_VOID };
                        action.as_signal.variable.storage = (VValue){ .vv_tag = VV_UNDEFINED };
                    }
                    break;

                default:
                    printf("Unknown node type\n");
                    assert(false);
                    break;
            }
            break;

        case BOX:
            switch(node->storage.box->wrapper) {
                case INT_WRAPPER: {
                    action.tag = VARIABLE_TYPE;
                    action.as_variable.vtype   = INT_MACRO_TYPE;
                    action.as_variable.storage = (VValue){ .vv_tag = VV_INT, .value_int = node->storage.box->value.bint };
                    break;
                }
                case FLOAT_WRAPPER: {
                    action.tag = VARIABLE_TYPE;
                    action.as_variable.vtype   = FLOAT_MACRO_TYPE;
                    action.as_variable.storage = (VValue){ .vv_tag = VV_FLOAT, .value_float = node->storage.box->value.bfloat };
                    break;
                }
                case STRING_WRAPPER: {
                    char* src = node->storage.box->value.bstring;
                    size_t len = strlen(src) + 1;
                    char* str_copy = (char*) arena_get(current_arena, len);
                    strncpy(str_copy, src, len);
                    str_copy[len - 1] = '\0';

                    action.tag = VARIABLE_TYPE;
                    action.as_variable.vtype = STRING_MACRO_TYPE;
                    action.as_variable.storage = (VValue){ .vv_tag = VV_STRING, .value_string = str_copy };
                    break;
                }
                case BOOL_WRAPPER:{
                    action.tag = VARIABLE_TYPE;
                    action.as_variable.vtype   = BOOL_MACRO_TYPE;
                    action.as_variable.storage = (VValue){ .vv_tag = VV_BOOL, .value_bool = node->storage.box->value.bint };
                    break;
                }
                case ID_WRAPPER:{
                    char* id_char = node->storage.box->value.bstring;

                    size_t len = strlen(id_char);
                    char* id_copy = (char*) arena_get(current_arena, len + 1);
                    memcpy(id_copy, id_char, len);
                    id_copy[len] = '\0';

                    VValue* found = findIdentifierStorage(manager, id_copy);

                    Identifier id;
                    id.name = id_copy;

                    if (found != NULL) {
                        id.storage = (VValue){ .vv_tag = VV_PTR, .value_ptr = found };
                    } else {
                        id.storage = (VValue){ .vv_tag = VV_UNDEFINED};
                    }

                    action.tag = VALUE_TYPE;
                    action.as_value = (VValue){ .vv_tag = VV_PTR, .value_ptr = getIdentifierStorage(manager, id_copy)};
                    break;
                }
                case TYPE_WRAPPER: {
                    action.tag = VALUE_TYPE;
                    action.as_value = (VValue){ .vv_tag = VV_INT, .value_int = node->storage.box->value.bint };
                    break;
                }
                default: {
                    printf("Unknown box type\n");
                    assert(false);
                    break;
                }
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

    if(node->tag == NODE){
        printf("Node type -> %d:\n", node->storage.node->type);
    }
    else{
        printf("Box Leaf:\n");
    }
    print_eval_pass(action);

    return action;
}

SymbolsManager* create_symbols_manager(){
    SymbolsManager* manager = initializeSymbols(3);
    createAttribute(manager, "type",    0);
    createAttribute(manager, "value",   1);
    createAttribute(manager, "params",  2); 

    return manager;
}

void calculate_tree(TreeManager tree_manager){
    SymbolsManager* symbols_manager = create_symbols_manager();

    Arena* global_arena = arena_create(GLOBAL_ARENA_SIZE);
    evaluate(symbols_manager, &tree_manager.root, global_arena);
}

void test_symbols_table_tree(void) {
    /* 2 attributes: "value" at index 0, "extra" at index 1 */
    SymbolsManager* manager = initializeSymbols(2);
    createAttribute(manager, "value", 0);
    createAttribute(manager, "extra", 1);

    /* ── Global scope (already created by initializeSymbols) ── */
    SymbolsTable* global = manager->root;

    initializeIdentifier(manager, "score");
    setAttributeIdentifier(manager, "score", "value", (VValue){ .vv_tag = VV_INT, .value_int = 42 });

    initializeIdentifier(manager, "ratio");
    setAttributeIdentifier(manager, "ratio", "value", (VValue){ .vv_tag = VV_FLOAT, .value_float = 3.14 });

    initializeIdentifier(manager, "name");
    setAttributeIdentifier(manager, "name", "value", (VValue){ .vv_tag = VV_STRING, .value_string = "alice" });

    /* ── Child scope ── */
    initializeScope(manager, DYNADICT_DEFAULT_CAPACITY);

    initializeIdentifier(manager, "active");
    setAttributeIdentifier(manager, "active", "value", (VValue){ .vv_tag = VV_BOOL, .value_bool = true });

    char** identifiers = dynadict_list_indexes(manager->root->child->hash);

    initializeIdentifier(manager, "count");
    setAttributeIdentifier(manager, "count", "value", (VValue){ .vv_tag = VV_INT,    .value_int    = 7      });
    setAttributeIdentifier(manager, "count", "extra", (VValue){ .vv_tag = VV_STRING, .value_string = "meta" });

    //print_symbols_table(manager, manager->root->child, "test", 0);

    finalizeScope(manager);

    /* ── Print ── */
    print_symbols_tree(manager, global, "global");
}

void test_evaluator(void) {
    TreeManager tm = initializeAST();

    // init int x <- 5;
    ASTNode type_val = create_value_box(tm.arena, 0);
    ASTNode type_children[] = {type_val};
    ASTNode prim_type = create_node(tm.arena, PRIMITIVETYPE, type_children, 1);

    ASTNode x_label = create_label(tm.arena, "x", 1);
    ASTNode x_id = create_box(tm.arena, ID_M, x_label);

    ASTNode five_label = create_label(tm.arena, "5", 1);
    ASTNode five = create_box(tm.arena, INT_M, five_label);

    ASTNode decl_children[] = {prim_type, x_id, five};
    ASTNode vardecl = create_node(tm.arena, VARDECL, decl_children, 3);

    ASTNode stat_children[] = {vardecl};
    ASTNode statlist = create_node(tm.arena, STATLIST, stat_children, 1);

    // print AST
    print_ast(statlist, "", true, NULL);

    // evaluate
    SymbolsManager* manager = create_symbols_manager();
    Arena* eval_arena = arena_create(LOCAL_ARENA_SIZE);

    EvalPass result = evaluate(manager, &statlist, eval_arena);
    print_eval_pass(result);

    print_symbols_tree(manager, manager->root, "global");

    destroyAST(tm);
    arena_destroy(eval_arena);
}

int main(){
    printf("Evaluate...\n");

    test_evaluator();
    return 0;
}