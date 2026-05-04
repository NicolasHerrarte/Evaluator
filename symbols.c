#include <stdio.h>
#include <stdlib.h>
#include "hash.h"
#include "allocator.h"
#include "dynarray.h"
#include "symbols.h"

SymbolsTable* initializeScope(SymbolsManager* manager, size_t dynadict_capacity){
    SymbolsTable* parent = NULL;
    if(dynarray_length(manager->stack) > 0){
        parent = dynarray_get_last(manager->stack);
    }

    SymbolsTable* new_table = arena_get(manager->arena, sizeof(SymbolsTable));
    //Hash* hash_storage = arena_get(manager->arena, sizeof(Hash));
    //hash_storage = dynadict_create(dynadict_capacity, AnyType**);
    Hash* hash_storage = dynadict_create(dynadict_capacity, VValue**);
    new_table->hash = hash_storage;
    //new_table->hash = dynadict_create(dynadict_capacity, AnyType*);
    new_table->parent = parent;
    new_table->sibling = NULL;
    new_table->child = NULL;

    printf("Table Addr %p\n", new_table);

    if(parent != NULL){
        if(parent->child == NULL){
            parent->child = new_table;
        }
        else{
            SymbolsTable* current = parent->child; 

            while (current->sibling != NULL) {
                current = current->sibling;
            }

            current->sibling = new_table;
        }
    }

    dynarray_push(manager->stack, new_table);
    return new_table;
}

SymbolsTable* finalizeScope(SymbolsManager* manager) {
    SymbolsTable* removed = NULL;

    dynarray_pop(manager->stack, &removed);

    // this new
    // untested
    
    // this doesnt allow testing so I will remove for now but it shoul not be a comment
    if(manager->production){
        dynadict_destroy(removed->hash);
    }
    

    if(dynarray_length(manager->stack) > 0){
        return dynarray_get_last(manager->stack);
    }
    else{
        return NULL;
    }
}

void initializeIdentifier(SymbolsManager* manager, char* identifier){
    assert(dynarray_length(manager->stack) > 0);
    SymbolsTable* current_scope = dynarray_get_last(manager->stack);

    VValue* hard_storage = arena_get(manager->arena, manager->attribute_capacity*sizeof(VValue));
    for(int i = 0;i<manager->attribute_capacity;i++){
        hard_storage[i].vv_tag = VV_UNDEFINED;
    }
    bool already_defined = dynadict_add(current_scope->hash, identifier, hard_storage);
    assert(!already_defined);
}

VValue* getSoftStorage(SymbolsManager* manager, Arena* local_arena){
    VValue* soft_storage = arena_get(local_arena, manager->attribute_capacity*sizeof(VValue));
    for(int i = 0;i<manager->attribute_capacity;i++){
        soft_storage[i].vv_tag = VV_UNDEFINED;
    }

    return soft_storage;
}

VValue* getIdentifierStorage(SymbolsManager* manager, char* identifier){
    assert(dynarray_length(manager->stack) > 0);
    SymbolsTable* current_scope = dynarray_get_last(manager->stack);

    while (current_scope != NULL) {
        VValue** storage_ptr = dynadict_get(current_scope->hash, identifier);
        if (storage_ptr != NULL) return *storage_ptr;
        current_scope = current_scope->parent;
    }

    return NULL;
}

void setAttributeIdentifier(SymbolsManager* manager, char* identifier, char* attribute, VValue record){
    VValue* storage_ptr = getIdentifierStorage(manager, identifier);
    int* storage_index = dynadict_get(manager->index_hash, attribute);

    assert(storage_index != NULL);
    assert(*storage_index >= 0);
    assert(*storage_index < manager->attribute_capacity);

    (storage_ptr)[*storage_index] = record;
}

VValue* getAttributeIdentifier(SymbolsManager* manager, char* identifier, char* attribute){
    VValue* storage_ptr = getIdentifierStorage(manager, identifier);
    int* storage_index = dynadict_get(manager->index_hash, attribute);

    assert(storage_index != NULL);
    assert(*storage_index >= 0);
    assert(*storage_index < manager->attribute_capacity);

    return &(storage_ptr)[*storage_index];
}

VValue getAttributeStorage(SymbolsManager* manager, VValue* storage_ptr, char* attribute){
    int* storage_index = dynadict_get(manager->index_hash, attribute);

    assert(storage_index != NULL);
    assert(*storage_index >= 0);
    assert(*storage_index < manager->attribute_capacity);

    return (storage_ptr)[*storage_index];
}

void setAttributeStorage(SymbolsManager* manager, VValue* storage_ptr, char* attribute, VValue record){
    int* storage_index = dynadict_get(manager->index_hash, attribute);

    assert(storage_index != NULL);
    assert(*storage_index >= 0);
    assert(*storage_index < manager->attribute_capacity);

    (storage_ptr)[*storage_index] = record;
}

void createAttribute(SymbolsManager* manager, char* attribute, int value){
    dynadict_add(manager->index_hash, attribute, value);
}

VValue* findStorage(SymbolsManager* manager, char* identifier, char* attribute){
    SymbolsTable* current_scope = dynarray_get_last(manager->stack);
    VValue** storage = dynadict_get(current_scope->hash, identifier);

    if(storage != NULL){
        int* storage_index = dynadict_get(manager->index_hash, attribute);
        assert(storage_index != NULL);
        assert(*storage_index >= 0);
        assert(*storage_index < manager->attribute_capacity);

        return &(*storage)[*storage_index];
    }

    while(storage == NULL && current_scope->parent != NULL){
        current_scope = current_scope->parent;
        storage = dynadict_get(current_scope->hash, identifier);

        if(storage != NULL){
            int* storage_index = dynadict_get(manager->index_hash, attribute);
            assert(storage_index != NULL);
            assert(*storage_index >= 0);
            assert(*storage_index < manager->attribute_capacity);

            return &(*storage)[*storage_index];
        }
    }

    return NULL;
}

SymbolsManager* initializeSymbols(int attr_cap, bool ini_production){
    SymbolsManager* manager = malloc(sizeof(SymbolsManager));
    manager->arena = arena_create(SYMBOLS_CHUNK_SIZE*(sizeof(SymbolsTable)+sizeof(Hash)));
    manager->global_arena = arena_create(GLOBAL_ARENA_SIZE);
    manager->stack = dynarray_create(SymbolsTable*);
    manager->root = initializeScope(manager, DYNADICT_DEFAULT_CAPACITY);
    //Hash* index_hash_storage = arena_get(manager->arena, sizeof(Hash));
    //index_hash_storage = dynadict_create(DYNADICT_DEFAULT_CAPACITY, int);
    Hash* index_hash_storage = dynadict_create(DYNADICT_DEFAULT_CAPACITY, int);
    manager->index_hash = index_hash_storage;
    //manager->index_hash = dynadict_create(DYNADICT_DEFAULT_CAPACITY, int);
    manager->attribute_capacity = attr_cap;
    manager->production = ini_production;
    return manager;
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