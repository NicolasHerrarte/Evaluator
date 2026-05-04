#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <stdbool.h>
#include "allocator.h"

#define ARENA_CHUNK_SIZE 1024
#define ROOT_TYPE 0

enum BoxWrapper{
    INT_WRAPPER,
    FLOAT_WRAPPER,
    STRING_WRAPPER,
    BOOL_WRAPPER,
    ID_WRAPPER,
    TYPE_WRAPPER
};

enum BoxMode{
    INT_M,
    FLOAT_M,
    STRING_M,
    BOOL_TRUE_M,
    BOOL_FALSE_M,
    ID_M
};

enum ChildTag{
    NODE,
    BOX,
    LABEL,
    EXTERNAL,
};

typedef struct InternalNode{
    struct ASTNode* left_child;
    int type;
} InternalNode;

typedef struct Box{
    enum BoxWrapper wrapper;
    union {
        int bint;
        float bfloat;
        char* bstring;
    } value;
} Box;

typedef struct ASTNode{
    union{
        struct InternalNode* node;
        struct Box* box;
        char* label;
        void (*external_func)(void*, ...);
    } storage;
    struct ASTNode* sibling;
    enum ChildTag tag;
} ASTNode;

typedef struct TreeManager{
    Arena* arena;
    ASTNode root;
} TreeManager;

ASTNode append_node(Arena* arena, ASTNode origin, ASTNode* children, int children_amount);
ASTNode create_node(Arena* arena, int nodetype, ASTNode* children, int children_amount);
ASTNode create_label(Arena* arena, char* token_char, int char_length);
ASTNode create_box(Arena* arena, enum BoxMode boxtype, ASTNode child);
ASTNode create_value_box(Arena* arena, int value);

ASTNode* get_child(ASTNode node, int child_number);
int get_children_count(ASTNode node);

TreeManager initializeAST();
void destroyAST(TreeManager tm);

void export_ast(FILE* stream_out, ASTNode node, char* prefix, bool is_last, char** ast_val_map);
void print_ast(ASTNode node, char* prefix, bool is_last, char** ast_val_map);