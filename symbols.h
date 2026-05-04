#include "usetypes.h"

#define SYMBOLS_CHUNK_SIZE 128
#define DYNADICT_DEFAULT_CAPACITY 512

#define get_current_scope(manager) dynarray_get_last(manager->stack)

SymbolsTable* initializeScope(SymbolsManager* manager, size_t dynadict_capacity);
SymbolsTable* finalizeScope(SymbolsManager* manager);
void initializeIdentifier(SymbolsManager* manager, char* identifier);
void setAttributeIdentifier(SymbolsManager* manager, char* identifier, char* attribute, VValue record);
void setAttributeStorage(SymbolsManager* manager, VValue* storage_ptr, char* attribute, VValue record);
VValue getAttributeStorage(SymbolsManager* manager, VValue* storage_ptr, char* attribute);
VValue* getSoftStorage(SymbolsManager* manager, Arena* local_arena);
VValue* getIdentifierStorage(SymbolsManager* manager, char* identifier);
VValue* findStorage(SymbolsManager* manager, char* identifier, char* attribute);
SymbolsManager* initializeSymbols(int attr_cap, bool ini_production);
void print_symbols_tree_recursive(SymbolsManager* manager, SymbolsTable* table, char* name, int depth);
void print_symbols_tree(SymbolsManager* manager, SymbolsTable* root, char* root_name);
void print_symbols_table(SymbolsManager* manager, SymbolsTable* table, char* st_name, int depth);

void createAttribute(SymbolsManager* manager, char* attribute, int value);

void test_symbols_table_tree();