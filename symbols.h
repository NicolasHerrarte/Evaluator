#include "usetypes.h"

#define SYMBOLS_CHUNK_SIZE 128
#define DYNADICT_DEFAULT_CAPACITY 512
#define LOCAL_ARENA_SIZE 1024

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