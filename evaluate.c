#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include "evaluate.h"

#define INT_MACRO_TYPE (Type){.kind = KIND_PRIMITIVE, .primitive_tag = VAR_TYPE_INT}
#define FLOAT_MACRO_TYPE (Type){.kind = KIND_PRIMITIVE, .primitive_tag = VAR_TYPE_FLOAT}
#define BOOL_MACRO_TYPE (Type){.kind = KIND_PRIMITIVE, .primitive_tag = VAR_TYPE_BOOL}
#define STRING_MACRO_TYPE (Type){.kind = KIND_PRIMITIVE, .primitive_tag = VAR_TYPE_STRING}
#define VOID_MACRO_TYPE (Type){.kind = KIND_PRIMITIVE, .primitive_tag = VAR_TYPE_VOID}
#define GENERIC_MACRO_TYPE (Type){.kind = KIND_PRIMITIVE, .primitive_tag = VAR_TYPE_GENERIC}

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

bool type_equals(Type a, Type b) {
    if (a.kind != b.kind) return false;

    switch (a.kind) {
        case KIND_PRIMITIVE:
            if (a.primitive_tag == VAR_TYPE_GENERIC || b.primitive_tag == VAR_TYPE_GENERIC)
                return true;

            return a.primitive_tag == b.primitive_tag;

        case KIND_ARRAY:
            if (a.array.elem_type != b.array.elem_type) return false;
            if (a.array.ndims != b.array.ndims) return false;
            for (int i = 0; i < a.array.ndims; i++) {
                if (a.array.sizes[i] != b.array.sizes[i]){
                    return false;
                }
            }
            return true;

        default:
            return false;
    }
}

EvalPass eval_comparison(EvalPass left, EvalPass right, int op) {
    assert(left.tag == VARIABLE_TYPE);
    assert(right.tag == VARIABLE_TYPE);
    assert(left.as_variable.vtype.kind == KIND_PRIMITIVE);
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
    assert(left.tag == VARIABLE_TYPE);
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

VValue var_deep_copy(VValue val, Type type, Arena* dest_arena) {
    VValue copy = val;
    if(type.kind == KIND_PRIMITIVE){
        if(type.primitive_tag == VAR_TYPE_STRING){
            size_t len = strlen(val.value_string) + 1;
            char* str_copy = (char*) arena_get(dest_arena, len);
            memcpy(str_copy, val.value_string, len);
            val.value_string = str_copy;
        }
        else{
            return copy;
        }
    }
    else if(type.kind == KIND_ARRAY){
        size_t total = 1;
        for (int d = 0; d < type.array.ndims; d++) {
            total *= type.array.sizes[d];
        }

        VValue* old_data = (VValue*) val.value_ptr;
        VValue* new_data = (VValue*) arena_get(dest_arena, total * sizeof(VValue));
        memcpy(new_data, old_data, total * sizeof(VValue));
        
        for (size_t i = 0; i < total; i++) {
            if (new_data[i].vv_tag == VV_STRING && new_data[i].value_string){
                new_data[i] = var_deep_copy(new_data[i], type, dest_arena);
            }
        }

        val.value_ptr = new_data;
    }

    return val;
}

Variable unpack_pointer(Variable variable){
    assert(variable.vtype.kind == KIND_POINTER);
    assert(variable.vtype.pointer.pointee_type);
    assert(variable.vtype.pointer.unpack);
    assert(variable.storage.vv_tag == VV_PTR);
    assert(variable.storage.value_ptr);

    VValue val;
    Type ptr_type = *variable.vtype.pointer.pointee_type;
    
    VValue* ptr_storage = (VValue*) variable.storage.value_ptr;
    switch (ptr_type.primitive_tag) {
        case VAR_TYPE_INT:    val = (VValue){ .vv_tag = VV_INT,    .value_int    = ptr_storage->value_int}; break;
        case VAR_TYPE_FLOAT:  val = (VValue){ .vv_tag = VV_FLOAT,  .value_float  = ptr_storage->value_float  }; break;
        case VAR_TYPE_BOOL:   val = (VValue){ .vv_tag = VV_BOOL,   .value_bool   = ptr_storage->value_bool   }; break;
        case VAR_TYPE_STRING: val = (VValue){ .vv_tag = VV_STRING, .value_string = ptr_storage->value_string }; break;
        default: assert(false);
    }

    return (Variable) {.vtype = ptr_type, .storage = val};
}

EvalPass unpack_access_to_var(SymbolsManager* manager, EvalPass packed_expr){
    if(packed_expr.tag == VARIABLE_TYPE){
        return packed_expr;
    }
    else if(packed_expr.tag == ACCESS_TYPE){
        VValue* storage_ptr = packed_expr.as_access.link;

        VValue type_slot = getAttributeStorage(manager, storage_ptr, "type");
        assert(type_slot.vv_tag == VV_PTR);
        Type access_type = *((Type*) type_slot.value_ptr);

        VValue value_slot = getAttributeStorage(manager, storage_ptr, "value");

        Variable new_var = (Variable) {.vtype = access_type, .storage = value_slot};

        if(new_var.vtype.kind == KIND_POINTER && new_var.vtype.pointer.unpack == true){
            new_var = unpack_pointer(new_var);
        }

        EvalPass expr_unpacked;
        expr_unpacked.tag = VARIABLE_TYPE;
        expr_unpacked.as_variable = new_var;

        return expr_unpacked;
    }
    else{
        assert(false);
    }
}

EvalPass pack_var_to_access(SymbolsManager* manager, Arena* current_arena, EvalPass unpacked_expr){
    if(unpacked_expr.tag == VARIABLE_TYPE){
        VValue* soft_storage = getSoftStorage(manager, current_arena);

        Type* type_storage = (Type*) arena_get(current_arena, sizeof(Type));
        *type_storage = unpacked_expr.as_variable.vtype;

        setAttributeStorage(manager, soft_storage, "type", (VValue){.vv_tag=VV_PTR, .value_ptr=type_storage});
        setAttributeStorage(manager, soft_storage, "value", unpacked_expr.as_variable.storage);

        CentralAccess packed_access = {.link=soft_storage, .view=true};
        return (EvalPass) {.tag=ACCESS_TYPE, .as_access=packed_access};
    }
    else if(unpacked_expr.tag == ACCESS_TYPE){
        return unpacked_expr;
    }
    else{
        assert(false);
    }
}

void type_modify(Type* type_ptr, Type type_from){
    if(type_ptr->kind != type_from.kind) return;
    if(type_ptr->kind == KIND_PRIMITIVE) return;

    if(type_ptr->kind == KIND_ARRAY){
        if(type_ptr->array.ndims != type_from.array.ndims) return;
        if(type_ptr->array.elem_type != type_from.array.elem_type) return;

        for(int i = 0; i < type_ptr->array.ndims; i++){
            if(type_ptr->array.sizes[i] != 0) return;
        }

        for(int i = 0; i < type_ptr->array.ndims; i++){
            type_ptr->array.sizes[i] = type_from.array.sizes[i];
        }
    }
}

size_t iterations_per_type(Type* type) {
    switch (type->kind) {
        case KIND_PRIMITIVE:
            return 1;
        case KIND_ARRAY: {
            size_t total = 1;
            for (int d = 0; d < type->array.ndims; d++) {
                total *= type->array.sizes[d];
            }
            return total;
        }
        case KIND_POINTER:
            return 1;
        default:
            assert(false);
            return 0;
    }
}

void print_type(Type tp){
    printf("TYPE: kind=%d", tp.kind);
    if (tp.kind == KIND_PRIMITIVE) {
        printf(" primitive=%d", tp.primitive_tag);
    } else if (tp.kind == KIND_ARRAY) {
        printf(" elem=%d ndims=%d sizes=[", tp.array.elem_type, tp.array.ndims);
        for (int i = 0; i < tp.array.ndims; i++) {
            printf("%d%s", tp.array.sizes[i], i < tp.array.ndims - 1 ? "," : "");
        }
        printf("]");
    }
    printf("\n");
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
            print_type(ep.as_vart);
            break;
        case VARIABLE_TYPE:
            printf("VARIABLE: type=");
            if (ep.as_variable.vtype.kind == KIND_PRIMITIVE) {
                printf("prim(%d)", ep.as_variable.vtype.primitive_tag);
            } else {
                printf("arr(elem=%d,ndims=%d)", ep.as_variable.vtype.array.elem_type, ep.as_variable.vtype.array.ndims);
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
        case ACCESS_TYPE:
            printf("ACCESS: %p\n", ep.as_access.link);
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
                    //printf("Tag -> %d\n", identifier.tag);
                    assert(identifier.tag == VALUE_TYPE);
                    assert(identifier.as_value.vv_tag == VV_STRING);
                    assert(var_type.tag == VART_TYPE);

                    assert(var_type.as_vart.primitive_tag != VAR_TYPE_VOID);
                    assert(var_type.as_vart.primitive_tag != VAR_TYPE_GENERIC);

                    char * id_char = identifier.as_value.value_string;

                    initializeIdentifier(manager, id_char);

                    Type* type_storage = (Type*) arena_get(current_arena, sizeof(Type));
                    *type_storage = var_type.as_vart;

                    setAttributeIdentifier(manager, id_char, "type", (VValue){ .vv_tag = VV_PTR, .value_ptr = type_storage});
                    if(get_children_count(*node) == 3){
                        ASTNode* expr_node = identifier_node->sibling;
                        EvalPass expr_p = evaluate(manager, expr_node, current_arena);

                        EvalPass expr_original = unpack_access_to_var(manager, expr_p);

                        type_modify(type_storage, expr_original.as_variable.vtype);
                        assert(type_equals(*type_storage, expr_original.as_variable.vtype));

                        VValue value_deep_copy = var_deep_copy(expr_original.as_variable.storage, expr_original.as_variable.vtype, current_arena);
                        
                        setAttributeIdentifier(manager, id_char, "value", value_deep_copy);
                    }
                    break;
                }
                case VARASSIGN: {

                    // evaluate rhs
                    // insertAttribute value
                    ASTNode* access_node = node->storage.node->left_child;
                    ASTNode* expr_node = access_node->sibling;

                    EvalPass access = evaluate(manager, access_node, current_arena);
                    EvalPass expr_p = evaluate(manager, expr_node, current_arena);
                    EvalPass expr_original = unpack_access_to_var(manager, expr_p);

                    assert(access.tag == ACCESS_TYPE);
                    
                    VValue access_type_raw = getAttributeStorage(manager, access.as_access.link, "type");
                    assert(access_type_raw.vv_tag == VV_PTR);
                    assert(access_type_raw.value_ptr);

                    Type* access_type = (Type*) access_type_raw.value_ptr;

                    if(!access.as_access.view){
                        type_modify(access_type, expr_original.as_variable.vtype);
                    }
                    //print_type(*access_type);
                    //print_type(expr_original.as_variable.vtype);
                    
                    int case_transfer = -1;
                    if(access_type->kind == KIND_PRIMITIVE){
                        case_transfer = 0;
                    }
                    else if(access_type->kind == KIND_POINTER){
                        if(access_type->pointer.unpack){
                            case_transfer = 1;
                        }
                        else{
                            case_transfer = 0;
                        }
                    }
                    else if(access_type->kind == KIND_ARRAY){
                        case_transfer = 2;
                    }

                    switch (case_transfer)
                    {
                        case 0: {
                            assert(type_equals(*access_type, expr_original.as_variable.vtype));
                            VValue value_deep_copy = var_deep_copy(expr_original.as_variable.storage, expr_original.as_variable.vtype, current_arena);
                            setAttributeStorage(manager, access.as_access.link, "value", value_deep_copy);
                            break;
                        }
                        case 1: {
                            assert(type_equals(*(access_type->pointer.pointee_type), expr_original.as_variable.vtype));
                            VValue value_deep_copy = var_deep_copy(expr_original.as_variable.storage, expr_original.as_variable.vtype, current_arena);
                            VValue val_slot = getAttributeStorage(manager, access.as_access.link, "value");
                            VValue* target = (VValue*) val_slot.value_ptr;
                            *target = value_deep_copy;
                            break;
                        }
                        case 2: {
                            VValue val_slot = getAttributeStorage(manager, access.as_access.link, "value");
                            VValue* dest = (VValue*) val_slot.value_ptr;
                            VValue* src = (VValue*) expr_original.as_variable.storage.value_ptr;
                            size_t total = iterations_per_type(access_type);
                            memcpy(dest, src, total * sizeof(VValue));
                            break;
                        }
                        default:
                            assert(false);
                            break;
                    }

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
                        for (int d = 0; d < type_spec.array.ndims; d++){
                            store_size *= type_spec.array.sizes[d];
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
                        arr_type.array.elem_type = instantiation.as_vart.array.elem_type;
                        arr_type.array.ndims     = instantiation.as_vart.array.ndims + 1;
                        arr_type.array.sizes[0]  = dim_size;
                        for (int d = 0; d < instantiation.as_vart.array.ndims; d++)
                            arr_type.array.sizes[d + 1] = instantiation.as_vart.array.sizes[d];
                    } else if (instantiation.tag == VALUE_TYPE) {
                        // innermost — primitive type from @vl
                        assert(instantiation.as_value.vv_tag == VV_INT);
                        switch (instantiation.as_value.value_int) {
                            case 0: arr_type.array.elem_type = VAR_TYPE_INT;    break;
                            case 1: arr_type.array.elem_type = VAR_TYPE_BOOL;   break;
                            case 2: arr_type.array.elem_type = VAR_TYPE_FLOAT;  break;
                            case 3: arr_type.array.elem_type = VAR_TYPE_STRING; break;
                            default: assert(false);
                        }
                        arr_type.array.ndims    = 1;
                        arr_type.array.sizes[0] = dim_size;
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

                    EvalPass cond_p = evaluate(manager, cond_node, current_arena);
                    EvalPass cond = unpack_access_to_var(manager, cond_p);
                    //assert(cond.tag == VARIABLE_TYPE);
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
                        EvalPass cond_p = evaluate(manager, cond_node, current_arena);
                        EvalPass cond = unpack_access_to_var(manager, cond_p);
                        //assert(cond.tag == VARIABLE_TYPE);
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
                        EvalPass cond_p = evaluate(manager, cond_node, current_arena);
                        EvalPass cond = unpack_access_to_var(manager, cond_p);
                        //assert(cond.tag == VARIABLE_TYPE);
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

                    EvalPass left_expr_p = evaluate(manager, left_expr_node, current_arena);
                    EvalPass right_expr_p = evaluate(manager, right_expr_node, current_arena);

                    EvalPass left_expr = unpack_access_to_var(manager, left_expr_p);
                    EvalPass right_expr = unpack_access_to_var(manager, right_expr_p);

                    printf("SUM:\n");
                    print_eval_pass(left_expr);

                    action = eval_arithmetic(left_expr, right_expr, '+');
                    
                    break;
                }
                case SUBTRACT: {
                    // evaluate left - evaluate right
                    ASTNode* left_expr_node = node->storage.node->left_child;
                    ASTNode* right_expr_node = left_expr_node->sibling;

                    EvalPass left_expr_p = evaluate(manager, left_expr_node, current_arena);
                    EvalPass right_expr_p = evaluate(manager, right_expr_node, current_arena);

                    EvalPass left_expr = unpack_access_to_var(manager, left_expr_p);
                    EvalPass right_expr = unpack_access_to_var(manager, right_expr_p);

                    action = eval_arithmetic(left_expr, right_expr, '-');
                    break;
                }
                case MULT: {
                    // evaluate left * evaluate right
                    ASTNode* left_expr_node = node->storage.node->left_child;
                    ASTNode* right_expr_node = left_expr_node->sibling;

                    EvalPass left_expr_p = evaluate(manager, left_expr_node, current_arena);
                    EvalPass right_expr_p = evaluate(manager, right_expr_node, current_arena);

                    EvalPass left_expr = unpack_access_to_var(manager, left_expr_p);
                    EvalPass right_expr = unpack_access_to_var(manager, right_expr_p);

                    action = eval_arithmetic(left_expr, right_expr, '*');
                    break;
                }
                case DIVISION: {
                    // evaluate left / evaluate right
                    // check div by zero
                    ASTNode* left_expr_node = node->storage.node->left_child;
                    ASTNode* right_expr_node = left_expr_node->sibling;

                    EvalPass left_expr_p = evaluate(manager, left_expr_node, current_arena);
                    EvalPass right_expr_p = evaluate(manager, right_expr_node, current_arena);

                    EvalPass left_expr = unpack_access_to_var(manager, left_expr_p);
                    EvalPass right_expr = unpack_access_to_var(manager, right_expr_p);

                    action = eval_arithmetic(left_expr, right_expr, '/');
                    break;
                }
                case LOGICAND : {
                    ASTNode* left_expr_node = node->storage.node->left_child;
                    ASTNode* right_expr_node = left_expr_node->sibling;
                    
                    EvalPass left_expr_p = evaluate(manager, left_expr_node, current_arena);
                    EvalPass right_expr_p = evaluate(manager, right_expr_node, current_arena);

                    EvalPass left_expr = unpack_access_to_var(manager, left_expr_p);
                    EvalPass right_expr = unpack_access_to_var(manager, right_expr_p);

                    //assert(right_expr.tag == VARIABLE_TYPE);
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
                    
                    EvalPass left_expr_p = evaluate(manager, left_expr_node, current_arena);
                    EvalPass right_expr_p = evaluate(manager, right_expr_node, current_arena);

                    EvalPass left_expr = unpack_access_to_var(manager, left_expr_p);
                    EvalPass right_expr = unpack_access_to_var(manager, right_expr_p);

                    //assert(left_expr.tag == VARIABLE_TYPE);
                    //assert(right_expr.tag == VARIABLE_TYPE);
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

                    EvalPass left_expr_p = evaluate(manager, left_expr_node, current_arena);
                    EvalPass logical_operator = evaluate(manager, logical_operator_node, current_arena);
                    EvalPass right_expr_p = evaluate(manager, right_expr_node, current_arena);

                    EvalPass left_expr = unpack_access_to_var(manager, left_expr_p);
                    EvalPass right_expr = unpack_access_to_var(manager, right_expr_p);

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
                    assert(access.tag == ACCESS_TYPE);
                    assert(args.tag == ARGS_TYPE);
                    assert(args.as_args.mode == ARGUMENT_MODE);

                    // retrieve function info from symbol table
                    VValue* func_storage = access.as_access.link;

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

                        Type body_ret_type = result.as_signal.variable.vtype;

                        type_modify(&return_type, body_ret_type);

                        print_type(body_ret_type);
                        print_type(return_type);

                        assert(type_equals(body_ret_type, return_type));

                        action.tag = VARIABLE_TYPE;
                        action.as_variable.vtype = body_ret_type;
                        action.as_variable.storage = var_deep_copy(result.as_signal.variable.storage, result.as_signal.variable.vtype, current_arena);
                    }

                    if(manager->production){
                        arena_destroy(local_inner_arena);
                    }

                    finalizeScope(manager);
                    
                    break;
                }
                case ARRACCESS: {
                    ASTNode* access_node = node->storage.node->left_child;
                    ASTNode* expr_node = access_node->sibling;

                    EvalPass access = evaluate(manager, access_node, current_arena);
                    EvalPass expr_p = evaluate(manager, expr_node, current_arena);
                    EvalPass expr = unpack_access_to_var(manager, expr_p);

                    //assert(expr.tag == VARIABLE_TYPE);
                    assert(expr.as_variable.vtype.primitive_tag == VAR_TYPE_INT);

                    int idx = expr.as_variable.storage.value_int;

                    Type curr_type;
                    VValue* base_ptr;

                    if (access.nclass == ARRACCESS) {
                        assert(access.tag == VARIABLE_TYPE);
                        assert(access.as_variable.vtype.kind == KIND_ARRAY);
                        curr_type = access.as_variable.vtype;
                        base_ptr = (VValue*) access.as_variable.storage.value_ptr;
                    } else if (access.tag == ACCESS_TYPE) {
                        // this can be replaced by access to var
                        VValue* storage_ptr = access.as_access.link;

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
                    assert(idx >= 0 && idx < curr_type.array.sizes[0]);

                    // compute stride
                    int stride = 1;
                    for (int d = 1; d < curr_type.array.ndims; d++)
                        stride *= curr_type.array.sizes[d];

                    VValue* elem_ptr = &base_ptr[idx * stride];

                    if (curr_type.array.ndims == 1) {
                        // last dimension → unpack actual value into pointer
                        Type* ptr_pointee_storage = (Type*) arena_get(current_arena, sizeof(Type));
                        ptr_pointee_storage->kind = KIND_PRIMITIVE;
                        ptr_pointee_storage->primitive_tag = curr_type.array.elem_type;

                        Type pointer_type;
                        pointer_type.kind = KIND_POINTER;
                        pointer_type.pointer.pointee_type = ptr_pointee_storage;
                        pointer_type.pointer.unpack = true;

                        EvalPass arr_access;
                        arr_access.tag = VARIABLE_TYPE;
                        arr_access.as_variable.vtype = pointer_type;
                        arr_access.as_variable.storage = (VValue){ .vv_tag = VV_PTR, .value_ptr = elem_ptr };

                        action = pack_var_to_access(manager, current_arena, arr_access);
                    } else if (curr_type.array.ndims > 1) {
                        Type reduced;
                        reduced.kind = KIND_ARRAY;
                        reduced.array.elem_type = curr_type.array.elem_type;
                        reduced.array.ndims = curr_type.array.ndims - 1;
                        for (int d = 0; d < reduced.array.ndims; d++)
                            reduced.array.sizes[d] = curr_type.array.sizes[d + 1];

                        EvalPass arr_access;
                        arr_access.tag = VARIABLE_TYPE;
                        arr_access.as_variable.vtype = reduced;
                        arr_access.as_variable.storage = (VValue){ .vv_tag = VV_PTR, .value_ptr = elem_ptr };

                        action = pack_var_to_access(manager, current_arena, arr_access);
                    } else {
                        assert(false);
                    }

                    break;
                }
                case ACCESS: {
                    ASTNode* identifier_node = node->storage.node->left_child;

                    EvalPass identifier = evaluate(manager, identifier_node, current_arena);

                    assert(identifier.tag == VALUE_TYPE);
                    assert(identifier.as_value.vv_tag == VV_STRING);

                    char* id_char = identifier.as_value.value_string;

                    VValue* found = getIdentifierStorage(manager, id_char);

                    assert(found);

                    action.tag = ACCESS_TYPE;
                    action.as_access.link = found;
                    action.as_access.view = false;
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

                        arr_type.array.elem_type = inner.as_vart.primitive_tag;
                        arr_type.array.ndims = 1;
                        arr_type.array.sizes[0] = 0;
                    }
                    else if(inner.as_vart.kind == KIND_ARRAY){
                        arr_type.array.elem_type = inner.as_vart.array.elem_type;
                        arr_type.array.ndims = inner.as_vart.array.ndims + 1;
                        arr_type.array.sizes[arr_type.array.ndims] = 0;
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
                    // Here there is problem
                    ASTNode* curr_expr = node->storage.node->left_child;
                    Arena* g_arena = manager->global_arena;

                    int children_count = 0;
                    if(curr_expr){
                        children_count = get_children_count(*node);
                    }

                    Args fargs;
                    fargs.mode = ARGUMENT_MODE;
                    fargs.amount = children_count;
                    fargs.list = arena_get(g_arena, children_count*sizeof(Argument));

                    for(int i = 0;i<children_count;i++){
                        EvalPass expr_p = evaluate(manager, curr_expr, current_arena);
                        EvalPass expr = unpack_access_to_var(manager, expr_p);
                        //assert(expr.tag == VARIABLE_TYPE);

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

                    int children_count = 0;
                    if(param){
                        children_count = get_children_count(*param);
                    }

                    Args fpars;
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

                    assert(identifier.tag == VALUE_TYPE);
                    assert(identifier.as_value.vv_tag == VV_STRING);
                    assert(param.tag == VART_TYPE);

                    Argument new_argument;

                    new_argument.mode = PARAMETER_MODE;
                    new_argument.name = identifier.as_value.value_string;
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
                        EvalPass expr_p = evaluate(manager, expr_node, current_arena);
                        EvalPass expr = unpack_access_to_var(manager, expr_p);

                        //assert(expr.tag == VARIABLE_TYPE);
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

                    action.tag = VALUE_TYPE;
                    action.as_value = (VValue){ .vv_tag = VV_STRING, .value_string = id_copy };
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
        case EXTERNAL:
            node->storage.external_func(manager);
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

void print_execute(void* manager_void, ...){
    SymbolsManager* manager = (SymbolsManager*) manager_void;

    VValue generic_print = *getAttributeIdentifier(manager, "generic_print", "value");

    switch (generic_print.vv_tag) {
        case VV_INT:
            printf("%d\n", generic_print.value_int);
            break;
        case VV_FLOAT:
            printf("%.2f\n", generic_print.value_float);
            break;
        case VV_BOOL:
            printf("%s\n", generic_print.value_bool ? "true" : "false");
            break;
        case VV_STRING:
            printf("%s\n", generic_print.value_string ? generic_print.value_string : "null");
            break;
        case VV_PTR:
            printf("ptr(%p)\n", generic_print.value_ptr);
            break;
        default:
            printf("Unsuported type for print function");
            assert(false);
            break;
    }
}

void print_define(SymbolsManager* manager){
    Type* void_type_storage = (Type*) arena_get(manager->global_arena, sizeof(Args));
    *void_type_storage = VOID_MACRO_TYPE;
    initializeIdentifier(manager, "print");
    setAttributeIdentifier(manager, "print", "type", (VValue){ .vv_tag = VV_PTR, .value_ptr = void_type_storage});

    Argument* argument = (Argument*) arena_get(manager->global_arena, sizeof(Argument));
    argument->mode = PARAMETER_MODE;
    argument->name = "generic_print";
    argument->argtype = GENERIC_MACRO_TYPE;
    Args* args_storage = (Args*) arena_get(manager->global_arena, sizeof(Args));
    args_storage->mode = PARAMETER_MODE;
    args_storage->list = argument;
    args_storage->amount = 1;
    setAttributeIdentifier(manager, "print", "params", (VValue){ .vv_tag = VV_PTR, .value_ptr = args_storage });

    ASTNode* external_node = (ASTNode*) arena_get(manager->global_arena, sizeof(ASTNode));
    external_node->tag = EXTERNAL;
    external_node->sibling = NULL;
    external_node->storage.external_func = print_execute;
    setAttributeIdentifier(manager, "print", "value", (VValue){ .vv_tag = VV_PTR, .value_ptr = external_node});
}

SymbolsManager* create_symbols_manager(bool production){
    SymbolsManager* manager = initializeSymbols(3, production);
    createAttribute(manager, "type",    0);
    createAttribute(manager, "value",   1);
    createAttribute(manager, "params",  2); 

    return manager;
}

void calculate_tree(TreeManager tree_manager, bool production){
    SymbolsManager* symbols_manager = create_symbols_manager(production);
    print_define(symbols_manager);

    evaluate(symbols_manager, &tree_manager.root, symbols_manager->global_arena);
}

void test_symbols_table_tree(void) {
    /* 2 attributes: "value" at index 0, "extra" at index 1 */
    SymbolsManager* manager = initializeSymbols(2, false);
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
    
    // init int a <- 5;
    ASTNode a_type_val = create_value_box(tm.arena, 0);
    ASTNode a_type_children[] = {a_type_val};
    ASTNode a_prim_type = create_node(tm.arena, PRIMITIVETYPE, a_type_children, 1);
    ASTNode a_label = create_label(tm.arena, "a", 1);
    ASTNode a_id = create_box(tm.arena, ID_M, a_label);
    ASTNode five_label = create_label(tm.arena, "5", 1);
    ASTNode five = create_box(tm.arena, INT_M, five_label);
    ASTNode a_decl_children[] = {a_prim_type, a_id, five};
    ASTNode a_decl = create_node(tm.arena, VARDECL, a_decl_children, 3);

    // print(a)
    ASTNode print_label = create_label(tm.arena, "print", 5);
    ASTNode print_id = create_box(tm.arena, ID_M, print_label);
    ASTNode print_access_children[] = {print_id};
    ASTNode print_access = create_node(tm.arena, ACCESS, print_access_children, 1);

    ASTNode a_ref_label = create_label(tm.arena, "a", 1);
    ASTNode a_ref = create_box(tm.arena, ID_M, a_ref_label);
    ASTNode a_ref_access_children[] = {a_ref};
    ASTNode a_ref_access = create_node(tm.arena, ACCESS, a_ref_access_children, 1);

    ASTNode args_children[] = {a_ref_access};
    ASTNode arguments = create_node(tm.arena, ARGUMENTS, args_children, 1);

    ASTNode funccall_children[] = {print_access, arguments};
    ASTNode funccall = create_node(tm.arena, FUNCCALL, funccall_children, 2);

    // StatList
    ASTNode stat_children[] = {a_decl, funccall};
    ASTNode statlist = create_node(tm.arena, STATLIST, stat_children, 2);
    // print AST
    print_ast(statlist, "", true, NULL);

    // evaluate
    SymbolsManager* manager = create_symbols_manager(false);

    print_define(manager);
    EvalPass result = evaluate(manager, &statlist, manager->global_arena);
    print_eval_pass(result);

    print_symbols_tree(manager, manager->root, "global");

    destroyAST(tm);
}

int main(){
    printf("Evaluate...\n");

    test_evaluator();
    return 0;
}