#include <stdio.h>
#include <stdlib.h>
#include "ast.h"
#include "symbols.h"
#include "dynarray.h"

enum NodeClass{
    STATLIST,
    IFNODE,
    WHILENODE,
    FORNODE,
    VARDECL,
    FUNCDECL,
    VARASSIGN,
    ARRTYPE,
    BREAK,
    CONTINUE,
    RETURN,
    GOTO,
    ARGUMENTS,
    PARAMETERS,
    UNIPARAM,
    EXPRESSION,
    SUM,
    SUBTRACT,
    MULT,
    DIVISION,
    ARRACCESS,
    FUNCCALL,
    PROPERTYACCESS
};

AnyType* evaluate(SymbolsManager* manager, ASTNode* node){
    AnyType action;
    action.tag = UNDEFINED_TYPE;

    AnyType* children_output = NULL;
    children_output = evaluate(manager, node->storage.node->left_child);

    if(children_output != NULL){
        dynarray_destroy(children_output);
    }

    AnyType* sibling_chain = evaluate(manager, node->sibling);

    if(sibling_chain == NULL){sibling_chain = dynarray_create(AnyType); }

    dynarray_pushleft(sibling_chain, action);

    switch(node->tag) {
        case NODE:
            switch(node->storage.node->type) {

                case STATLIST:
                    // evaluate each child in order
                    break;

                case VARDECL:
                    // initializeIdentifier
                    // insertAttribute type
                    // insertAttribute value if exists
                    //ASTNode* variable_node = get_child(*node, 1);
                    //assert(variable_node->tag == BOX);
                    //Box* variable_box = variable_node->storage.box;

                    //assert(variable_box->wrapper == ID_WRAPPER);
                    //assert(children_out->tag == PTR_TYPE);
                    //char* variable_name = children_out[1].UnionStorage.aptr;

                    //assert(children_out != NULL);

                    //insertAttribute(manager, variable_name, "type", children_out[0]);
                    //if(get_children_count(*node) == 3){
                        //insertAttribute(manager, variable_name, "value", children_out[2]);
                    //}
                    //break;

                case VARASSIGN:
                    // evaluate rhs
                    // insertAttribute value
                    //ASTNode* variable_node = get_child(*node, 1);
                    //assert(variable_node->tag == BOX);
                    //Box* variable_box = variable_node->storage.box;

                    //assert(variable_box->wrapper == ID_WRAPPER);
                    //char* variable_name = variable_box->value.bstring;
                    //break;

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

    return NULL;
}

SymbolsManager* create_symbols_manager(){
    SymbolsManager* manager = initializeSymbols(STORAGE_NAME_CAPACITY);
    createAttribute(manager, "type",    0);
    createAttribute(manager, "value",   1);
    createAttribute(manager, "length",  2);
    //createAttribute(manager, "address", 3);
    createAttribute(manager, "params",  4); 
    createAttribute(manager, "returns", 5); 

    return manager;
}

void calculate_tree(TreeManager tree_manager){
    SymbolsManager* symbols_manager = create_symbols_manager();
    evaluate(symbols_manager, &tree_manager.root);
}

int main(){
    printf("Evaluate...\n");

    test_symbols_table_tree();
    return 0;
}