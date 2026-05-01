#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <stdbool.h>
#include <string.h>
#include "allocator.h"
#include "ast.h"
//#include "symbols.h"

ASTNode append_node(Arena* arena, ASTNode origin, ASTNode* children, int children_amount){
    assert(children_amount > 0);
    assert(children != NULL);
    ASTNode* children_storage = (ASTNode*) arena_get(arena, children_amount*sizeof(ASTNode));
    for(int i = 0;i<children_amount; i++){
        children_storage[i] = children[i];
    }

    for(int i = 0;i<children_amount; i++){
        if(i+1 < children_amount){
            children_storage[i].sibling = &children_storage[i+1];
        }
        else{
            children_storage[i].sibling = NULL;
        }
    }

    if(origin.storage.node->left_child == NULL){
        origin.storage.node->left_child = children_storage;
    }
    else{
        ASTNode* current = origin.storage.node->left_child; 
        int count = 1;
        while (current->sibling != NULL) {
            current = current->sibling;
            count ++;
        }

        current->sibling = children_storage;
    }

    return origin;
}

ASTNode create_node(Arena* arena, int nodetype, ASTNode* children, int children_amount){
    ASTNode new_child;
    new_child.tag = NODE;

    InternalNode* node_storage = (InternalNode*) arena_get(arena, sizeof(InternalNode));
    node_storage->left_child = NULL;
    node_storage->type = nodetype;

    new_child.storage.node = node_storage;
    new_child.sibling = NULL;
    
    if(children_amount == 0) {return new_child; }
    ASTNode appended_child = append_node(arena, new_child, children, children_amount);

    return appended_child;
}

ASTNode create_label(Arena* arena, char* token_char, int char_length){
    ASTNode new_child;
    new_child.tag = LABEL;

    char* new_label = (char*) arena_get(arena, sizeof(char) * (char_length+1));
    strncpy(new_label, token_char, char_length+1);

    //printf("TOKEN CHAR %s\n", token_char);
    //printf("NEW LABEL %s\n", new_label);
    new_child.storage.label = new_label;
    return new_child;
}

ASTNode create_box(Arena* arena, enum BoxMode boxtype, ASTNode child){
    assert(child.tag == LABEL);
    
    ASTNode new_child;
    new_child.tag = BOX;

    Box* box_storage = (Box*) arena_get(arena, sizeof(Box));
    switch (boxtype) {
        char *endptr;
        case INT_M:
            box_storage->wrapper = INT_WRAPPER;
            box_storage->value.bint = strtol(child.storage.label, &endptr, 10);
            break;
        case FLOAT_M:
            box_storage->wrapper = FLOAT_WRAPPER;
            box_storage->value.bfloat = strtof(child.storage.label, &endptr);
            break;
        case STRING_M:
            // removing the " "
            box_storage->wrapper = STRING_WRAPPER;

            int char_len =  strlen(child.storage.label);
            int new_len = char_len - 2;
            char* str_copy = (char*) arena_get(arena, sizeof(char) * (new_len+1));
            strncpy(str_copy, child.storage.label+1, new_len);
            str_copy[new_len] = '\0';
            box_storage->value.bstring = str_copy;
            break;
        case BOOL_TRUE_M:
            box_storage->wrapper = BOOL_WRAPPER;
            box_storage->value.bint = 1;
            break;
        case BOOL_FALSE_M:
            box_storage->wrapper = BOOL_WRAPPER;
            box_storage->value.bint = 0;
            break;
        case ID_M:
            box_storage->wrapper = ID_WRAPPER;
            box_storage->value.bstring = child.storage.label;
            break;
        default:
            assert(false);
    }

    new_child.storage.box = box_storage;
    return new_child;
}

ASTNode create_value_box(Arena* arena, int value){  
    ASTNode new_child;
    new_child.tag = BOX;

    Box* box_storage = (Box*) arena_get(arena, sizeof(Box));
    box_storage->wrapper = TYPE_WRAPPER;
    box_storage->value.bint = value;

    new_child.storage.box = box_storage;
    return new_child;
}

ASTNode* get_child(ASTNode node, int child_number){
    assert(node.tag == NODE);

    ASTNode* current = node.storage.node->left_child;
    for(int i = 0;i<child_number;i++){
        current = current->sibling;
    }
    assert(current != NULL);
    return current;
}

int get_children_count(ASTNode node){
    assert(node.tag == NODE);

    ASTNode* current = node.storage.node->left_child;
    int count = 0;
    while(current != NULL){
        count++;
        current = current->sibling;
    }
    return count;
}

TreeManager initializeAST(){
    Arena* arena = arena_create(sizeof(ASTNode) * ARENA_CHUNK_SIZE);
    ASTNode root = create_node(arena, ROOT_TYPE, NULL, 0);

    TreeManager manager = {arena, root};
    return manager;
}

void destroyAST(TreeManager tm){
    arena_destroy(tm.arena);
}

void export_ast(FILE* stream_out, ASTNode node, char* prefix, bool is_last, char** ast_val_map) {
    if (prefix[0] != '\0') {
        fprintf(stream_out, "%s%s", prefix, is_last ? "└── " : "├── ");
    }

    if (node.tag == NODE) {
        InternalNode* internal = node.storage.node;
        if(ast_val_map){
            fprintf(stream_out, "[NODE] Type: %s\n", ast_val_map[internal->type]);
        }
        else{
            fprintf(stream_out, "[NODE] Type: %d\n", internal->type);
        }
        

        char next_prefix[512];
        if (prefix[0] == '\0') {
            next_prefix[0] = '\0'; 
        } else {
            snprintf(next_prefix, sizeof(next_prefix), "%s%s", 
                     prefix, is_last ? "    " : "│   ");
        }

        ASTNode* current = internal->left_child; 

        while (current != NULL) {
            // A node is the last child if it has no more siblings
            bool last_child = (current->sibling == NULL);
            
            // Prepare the prefix logic
            char* actual_prefix = (prefix[0] == '\0') ? " " : next_prefix;

            // Recurse using the current node
            // Note: We pass *current because your print_ast likely expects the struct, 
            // or current if it expects a pointer.
            export_ast(stream_out, *current, actual_prefix, last_child, ast_val_map);

            // Move to the next sibling
            current = current->sibling;
        }
    } 
    else if (node.tag == BOX) {
        Box* b = node.storage.box;
        if (b->wrapper == ID_WRAPPER) fprintf(stream_out, "[ID] %s\n", b->value.bstring);
        else if (b->wrapper == INT_WRAPPER) fprintf(stream_out, "[INT] %d\n", b->value.bint);
        else if (b->wrapper == BOOL_WRAPPER) fprintf(stream_out, "[BOOL] %d\n", b->value.bint);
        else if (b->wrapper == FLOAT_WRAPPER) fprintf(stream_out, "[FLOAT] %.2f\n", b->value.bfloat);
        else if (b->wrapper == STRING_WRAPPER) fprintf(stream_out, "[STR] %s\n", b->value.bstring);
        else if (b->wrapper == TYPE_WRAPPER) fprintf(stream_out, "[TYPE] %d\n", b->value.bint);
    }

    else if (node.tag == LABEL) {
        char* label_name = node.storage.label;
        fprintf(stream_out, "LABEL[%s]\n", label_name);
    }
}

void print_ast(ASTNode node, char* prefix, bool is_last, char** ast_val_map) {
    export_ast(stdout, node, prefix, is_last, ast_val_map);
}

