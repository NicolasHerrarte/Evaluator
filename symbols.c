#include <stdio.h>
#include <stdlib.h>
#include "hash.h"
#include "allocator.h"
#include "dynarray.h"
#include "symbols.h"

SymbolsTable* initializeScope(SymbolsManager* manager, int dynadict_capacity){
    SymbolsTable* parent = NULL;
    if(dynarray_length(manager->stack) > 0){
        parent = dynarray_get_last(manager->stack);
    }

    SymbolsTable* new_table = arena_get(manager->arena, sizeof(SymbolsTable));
    //Hash* hash_storage = arena_get(manager->arena, sizeof(Hash));
    //hash_storage = dynadict_create(dynadict_capacity, AnyType**);
    Hash* hash_storage = dynadict_create(dynadict_capacity, AnyType**);
    new_table->hash = hash_storage;
    new_table->local_arena = arena_create(LOCAL_ARENA_SIZE);
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
    SymbolsTable** removed_ptr;
    
    dynarray_pop(manager->stack, removed_ptr);

    //print_symbols_table(manager, removed_ptr, "Test", 0);

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

    AnyType* hard_storage = arena_get(manager->arena, manager->attribute_capacity*sizeof(AnyType));
    for(int i = 0;i<manager->attribute_capacity;i++){
        hard_storage[i].tag = UNDEFINED_TYPE;
    }
    bool already_defined = dynadict_add(current_scope->hash, identifier, hard_storage);
    assert(!already_defined);
}

void insertAttribute(SymbolsManager* manager, char* identifier, char* attribute, AnyType record){
    assert(dynarray_length(manager->stack) > 0);
    SymbolsTable* current_scope = dynarray_get_last(manager->stack);

    AnyType** storage_ptr = dynadict_get(current_scope->hash, identifier);

    assert(storage_ptr != NULL);

    int* storage_index = dynadict_get(manager->index_hash, attribute);

    assert(storage_index != NULL);
    assert(*storage_index >= 0);
    assert(*storage_index < manager->attribute_capacity);

    (*storage_ptr)[*storage_index] = record;
}

void createAttribute(SymbolsManager* manager, char* attribute, int value){
    dynadict_add(manager->index_hash, attribute, value);
}

AnyType* findStorage(SymbolsManager* manager, char* identifier, char* attribute){
    // This is kind of a little dangerous bc the hash is stored as an obj but, the function only calls the attributes
    // Changing the hash function might be a next step

    SymbolsTable* current_scope = dynarray_get_last(manager->stack);
    AnyType** storage = dynadict_get(current_scope->hash, identifier);

    if(storage != NULL){
        int* storage_index = dynadict_get(manager->index_hash, attribute);
        assert(storage_index != NULL);
        assert(*storage_index >= 0);
        assert(*storage_index < manager->attribute_capacity);

        return &(*storage)[*storage_index];
    }

    while(storage != NULL && current_scope->parent != NULL){
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

SymbolsManager* initializeSymbols(int attr_cap){
    SymbolsManager* manager = malloc(sizeof(SymbolsManager));
    manager->arena = arena_create(SYMBOLS_CHUNK_SIZE*(sizeof(SymbolsTable)+sizeof(Hash)));
    manager->stack = dynarray_create(SymbolsTable*);
    manager->root = initializeScope(manager, DYNADICT_DEFAULT_CAPACITY);
    //Hash* index_hash_storage = arena_get(manager->arena, sizeof(Hash));
    //index_hash_storage = dynadict_create(DYNADICT_DEFAULT_CAPACITY, int);
    Hash* index_hash_storage = dynadict_create(DYNADICT_DEFAULT_CAPACITY, int);
    manager->index_hash = index_hash_storage;
    //manager->index_hash = dynadict_create(DYNADICT_DEFAULT_CAPACITY, int);
    manager->attribute_capacity = attr_cap;
    return manager;
}