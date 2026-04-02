#include "hash.h"
#include "allocator.h"

#define SYMBOLS_CHUNK_SIZE 128
#define DYNADICT_DEFAULT_CAPACITY 512
#define STORAGE_NAME_CAPACITY 10

enum AnyEnum{
    UNDEFINED_TYPE,
    INT_TYPE,
    FLOAT_TYPE,
    PTR_TYPE
};

typedef struct AnyType{
    enum AnyEnum tag;
    void* aptr;
} AnyType;

typedef struct SymbolsTable{
    Hash* hash;
    struct SymbolsTable* parent;
    struct SymbolsTable* sibling;
    struct SymbolsTable* child;
} SymbolsTable;

typedef struct SymbolsManager{
    Arena* arena;
    Hash* index_hash;
    SymbolsTable* root;
    SymbolsTable** stack;
    int attribute_capacity;
} SymbolsManager;

void print_symbols_table(SymbolsManager* manager, SymbolsTable* table, char* st_name, int depth);
SymbolsTable* initializeScope(SymbolsManager* manager, int dynadict_capacity);
SymbolsTable* finalizeScope(SymbolsManager* manager);
void initializeIdentifier(SymbolsManager* manager, char* identifier);
void insertAttribute(SymbolsManager* manager, char* identifier, char* attribute, AnyType record);
AnyType* findStorage(SymbolsManager* manager, char* identifier, char* attribute);
SymbolsManager* initializeSymbols(int attr_cap);
void print_symbols_tree_recursive(SymbolsManager* manager, SymbolsTable* table, char* name, int depth);
void print_symbols_tree(SymbolsManager* manager, SymbolsTable* root, char* root_name);
void createAttribute(SymbolsManager* manager, char* attribute, int value);

void test_symbols_table_tree();