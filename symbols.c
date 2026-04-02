#include <stdio.h>
#include <stdlib.h>
#include "hash.h"
#include "allocator.h"
#include "dynarray.h"
#include "symbols.h"

void print_symbols_table(SymbolsManager* manager, SymbolsTable* table, char* st_name, int depth) {
    if (!manager || !table || !table->hash) return;

    char**    identifiers = dynadict_list_indexes(table->hash);
    AnyType** storage     = (AnyType**)hash_to_list(table->hash);
    char**    attr_names  = dynadict_list_indexes(manager->index_hash);
    int*      attr_slots  = hash_to_list(manager->index_hash);

    if (!identifiers || !storage || !attr_names || !attr_slots) return;

    // --- Build indent ---
    char indent[128] = "";
    for (int i = 0; i < depth; i++)
        strncat(indent, "    ", sizeof(indent) - strlen(indent) - 1);

    // --- Build lines into a temp buffer first to measure width ---
    #define MAX_LINES     256
    #define MAX_LINE_LEN  256
    char lines[MAX_LINES][MAX_LINE_LEN];
    int  line_count = 0;
    int  max_width  = 0;

    // Header line
    snprintf(lines[line_count], MAX_LINE_LEN, " SYMBOLS TABLE: %s ", st_name);
    max_width = strlen(lines[line_count]);
    line_count++;

    for (int i = 0; i < dynarray_length(identifiers); i++) {
        AnyType* row = storage[i];

        snprintf(lines[line_count], MAX_LINE_LEN, " %s ", identifiers[i]);
        if ((int)strlen(lines[line_count]) > max_width) max_width = strlen(lines[line_count]);
        line_count++;

        for (int j = 0; attr_names[j] != NULL; j++) {
            int slot = attr_slots[j];
            if (slot < 0 || slot >= STORAGE_NAME_CAPACITY) continue;

            AnyType val = row[slot];
            if (val.tag == UNDEFINED_TYPE) continue;

            char val_buf[64];
            switch (val.tag) {
                case INT_TYPE:   snprintf(val_buf, sizeof(val_buf), "INT(%d)",     *((int*) val.aptr));   break;
                case FLOAT_TYPE: snprintf(val_buf, sizeof(val_buf), "FLOAT(%.2f)", *((float*) val.aptr)); break;
                case PTR_TYPE:   snprintf(val_buf, sizeof(val_buf), "PTR(%p)",     val.aptr);   break;
                default:         snprintf(val_buf, sizeof(val_buf), "UNKNOWN");                               break;
            }

            snprintf(lines[line_count], MAX_LINE_LEN, "   - %s: %s ", attr_names[j], val_buf);
            if ((int)strlen(lines[line_count]) > max_width) max_width = strlen(lines[line_count]);
            line_count++;
        }
    }

    // --- Draw box ---
    char bar[MAX_LINE_LEN];
    memset(bar, '-', max_width + 2);
    bar[max_width + 2] = '\0';

    printf("\n");
    printf("%s+%s+\n", indent, bar);
    for (int i = 0; i < line_count; i++) {
        // Separator after header
        if (i == 1) printf("%s+%s+\n", indent, bar);
        printf("%s| %-*s |\n", indent, max_width, lines[i]);
    }
    printf("%s+%s+\n", indent, bar);
    printf("\n");
}

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

void test_symbols_table_tree(){
    printf("\n");
    printf("╔══════════════════════════════════════╗\n");
    printf("║     SYMBOLS TABLE TREE TEST          ║\n");
    printf("╚══════════════════════════════════════╝\n\n");

    // ================================================================
    // --- Setup: 4 attributes ---
    // ================================================================
    SymbolsManager* manager = initializeSymbols(STORAGE_NAME_CAPACITY);

    char* attr_type    = "type";
    char* attr_value   = "value";
    char* attr_length  = "length";
    char* attr_address = "address";
    //int idx_type    = 0;
    //int idx_value   = 1;
    //int idx_length  = 2;
    //int idx_address = 3;
    //dynadict_add(manager->index_hash, attr_type,    idx_type);
    //dynadict_add(manager->index_hash, attr_value,   idx_value);
    //dynadict_add(manager->index_hash, attr_length,  idx_length);
    //dynadict_add(manager->index_hash, attr_address, idx_address);

    createAttribute(manager, attr_type, 0);
    createAttribute(manager, attr_value, 1);
    createAttribute(manager, attr_length, 2);
    createAttribute(manager, attr_address, 3);

    printf("  Attributes registered: type[0], value[1], length[2], address[3]\n\n");

    // ================================================================
    // --- DEPTH 0: ROOT ---
    // x: int = 10
    // y: float = 2.71
    // z: int = 99
    // ================================================================

    int int_type = 0;
    int float_type = 1;

    printf("  [ROOT] Declaring x, y, z...\n");

    int   x_storage = 10;
    float y_storage = 2.71f;
    int   z_storage = 99;

    initializeIdentifier(manager, "x");
    insertAttribute(manager, "x", attr_type,    (AnyType){.tag=INT_TYPE,   .aptr=&int_type});
    insertAttribute(manager, "x", attr_value,   (AnyType){.tag=INT_TYPE,   .aptr=&x_storage});
    insertAttribute(manager, "x", attr_address, (AnyType){.tag=PTR_TYPE,   .aptr=&x_storage});

    initializeIdentifier(manager, "y");
    insertAttribute(manager, "y", attr_type,    (AnyType){.tag=INT_TYPE,   .aptr=&float_type});
    insertAttribute(manager, "y", attr_value,   (AnyType){.tag=FLOAT_TYPE, .aptr=&y_storage});
    insertAttribute(manager, "y", attr_address, (AnyType){.tag=PTR_TYPE,   .aptr=&y_storage});

    initializeIdentifier(manager, "z");
    insertAttribute(manager, "z", attr_type,    (AnyType){.tag=INT_TYPE,   .aptr=&int_type});
    insertAttribute(manager, "z", attr_value,   (AnyType){.tag=INT_TYPE,   .aptr=&z_storage});
    insertAttribute(manager, "z", attr_address, (AnyType){.tag=PTR_TYPE,   .aptr=&z_storage});

    // --- Verify ROOT ---
    AnyType x_val = *findStorage(manager, "x", attr_value);
    assert(x_val.tag == INT_TYPE && *((int*) x_val.aptr) == 10);
    printf("  x.value = %d  ✓\n", *((int*) x_val.aptr));

    AnyType x_addr = *findStorage(manager, "x", attr_address);
    assert(x_addr.tag == PTR_TYPE && x_addr.aptr == &x_storage);
    printf("  x.address = %p  ✓\n", x_addr.aptr);

    AnyType y_val = *findStorage(manager, "y", attr_value);
    assert(y_val.tag == FLOAT_TYPE && *((float*) y_val.aptr) == 2.71f);
    printf("  y.value = %.2f  ✓\n", *((float*) y_val.aptr));

    AnyType z_val = *findStorage(manager, "z", attr_value);
    assert(z_val.tag == INT_TYPE && *((int*) z_val.aptr) == 99);
    printf("  z.value = %d  ✓\n\n", *((int*) z_val.aptr));

    // ================================================================
    // --- DEPTH 1: Child A ---
    // a: int = 42
    // b: float = 3.14
    // ================================================================
    SymbolsTable* child_a = initializeScope(manager, DYNADICT_DEFAULT_CAPACITY);
    printf("  [Child A] Declaring a, b...\n");

    int   a_storage = 42;
    float b_storage = 3.14f;

    initializeIdentifier(manager, "a");
    insertAttribute(manager, "a", attr_type,    (AnyType){.tag=INT_TYPE,   .aptr=&int_type});
    insertAttribute(manager, "a", attr_value,   (AnyType){.tag=INT_TYPE,   .aptr=&a_storage});
    insertAttribute(manager, "a", attr_address, (AnyType){.tag=PTR_TYPE,   .aptr=&a_storage});

    initializeIdentifier(manager, "b");
    insertAttribute(manager, "b", attr_type,    (AnyType){.tag=INT_TYPE,   .aptr=&float_type});
    insertAttribute(manager, "b", attr_value,   (AnyType){.tag=FLOAT_TYPE, .aptr=&b_storage});
    insertAttribute(manager, "b", attr_address, (AnyType){.tag=PTR_TYPE,   .aptr=&b_storage});

    // --- Verify Child A ---
    AnyType a_val = *findStorage(manager, "a", attr_value);
    assert(a_val.tag == INT_TYPE && *((int*) a_val.aptr) == 42);
    printf("  a.value = %d  ✓\n", *((int*) a_val.aptr));

    AnyType a_addr = *findStorage(manager, "a", attr_address);
    assert(a_addr.tag == PTR_TYPE && a_addr.aptr == &a_storage);
    printf("  a.address = %p  ✓\n", a_addr.aptr);

    AnyType a_type = *findStorage(manager, "a", attr_type);
    assert(a_type.tag == INT_TYPE &&  *((int*) a_type.aptr) == 0);
    printf("  a.type = %d  ✓\n", *((int*) a_type.aptr));

    AnyType b_val = *findStorage(manager, "b", attr_value);
    assert(b_val.tag == FLOAT_TYPE && *((float*) b_val.aptr) == 3.14f);
    printf("  b.value = %.2f  ✓\n\n", *((float*) b_val.aptr));

    // ================================================================
    // --- DEPTH 2: Child A1 ---
    // p: int = 7
    // q: float = 0.5
    // r: int (value undefined)
    // ================================================================
    SymbolsTable* child_a1 = initializeScope(manager, DYNADICT_DEFAULT_CAPACITY);

    printf("Address -> %p\n", child_a1);
    printf("Address -> %p\n", manager->stack[2]);

    printf("  [Child A1] Declaring p, q, r...\n");

    int   p_storage = 7;
    float q_storage = 0.5f;

    initializeIdentifier(manager, "p");
    insertAttribute(manager, "p", attr_type,    (AnyType){.tag=INT_TYPE,   .aptr=&int_type});
    insertAttribute(manager, "p", attr_value,   (AnyType){.tag=INT_TYPE,   .aptr=&p_storage});
    insertAttribute(manager, "p", attr_address, (AnyType){.tag=PTR_TYPE,   .aptr=&p_storage});

    initializeIdentifier(manager, "q");
    insertAttribute(manager, "q", attr_type,    (AnyType){.tag=INT_TYPE,   .aptr=&float_type});
    insertAttribute(manager, "q", attr_value,   (AnyType){.tag=FLOAT_TYPE, .aptr=&q_storage});
    insertAttribute(manager, "q", attr_address, (AnyType){.tag=PTR_TYPE,   .aptr=&q_storage});

    initializeIdentifier(manager, "r");
    insertAttribute(manager, "r", attr_type,    (AnyType){.tag=INT_TYPE,   .aptr=&int_type});
    // r value and address intentionally undefined

    // --- Verify Child A1 ---
    AnyType p_val = *findStorage(manager, "p", attr_value);
    assert(p_val.tag == INT_TYPE && *((int*) p_val.aptr) == 7);
    printf("  p.value = %d  ✓\n", *((int*) p_val.aptr));

    AnyType q_val = *findStorage(manager, "q", attr_value);
    assert(q_val.tag == FLOAT_TYPE && *((float*) q_val.aptr) == 0.5f);
    printf("  q.value = %.2f  ✓\n", *((float*) q_val.aptr));

    AnyType r_val = *findStorage(manager, "r", attr_value);
    assert(r_val.tag == UNDEFINED_TYPE);
    printf("  r.value = UNDEFINED  ✓\n\n");

    //print_symbols_table(manager, child_a, "Test", 0);

    printf("FUCK HELL -> %p\n", (manager->stack)[dynarray_length(manager->stack)-1]);
    printf("FUCK HELL -> %p\n", child_a1);

    finalizeScope(manager); // back to Child A
    
    //printf("Address -> %p\n", manager->stack[1]);

    // ================================================================
    // --- DEPTH 2: Child A2 (sibling of A1) ---
    // m: int = 100
    // n: float = 9.81
    // ================================================================
    SymbolsTable* child_a2 = initializeScope(manager, DYNADICT_DEFAULT_CAPACITY);
    printf("  [Child A2] Declaring m, n...\n");

    int   m_storage = 100;
    float n_storage = 9.81f;

    initializeIdentifier(manager, "m");
    insertAttribute(manager, "m", attr_type,    (AnyType){.tag=INT_TYPE,   .aptr=&int_type});
    insertAttribute(manager, "m", attr_value,   (AnyType){.tag=INT_TYPE,   .aptr=&m_storage});
    insertAttribute(manager, "m", attr_address, (AnyType){.tag=PTR_TYPE,   .aptr=&m_storage});

    initializeIdentifier(manager, "n");
    insertAttribute(manager, "n", attr_type,    (AnyType){.tag=INT_TYPE,   .aptr=&float_type});
    insertAttribute(manager, "n", attr_value,   (AnyType){.tag=FLOAT_TYPE, .aptr=&n_storage});
    insertAttribute(manager, "n", attr_address, (AnyType){.tag=PTR_TYPE,   .aptr=&n_storage});

    // --- Verify Child A2 ---
    AnyType m_val = *findStorage(manager, "m", attr_value);
    assert(m_val.tag == INT_TYPE && *((int*) m_val.aptr) == 100);
    printf("  m.value = %d  ✓\n", *((int*) m_val.aptr));

    AnyType n_val = *findStorage(manager, "n", attr_value);
    assert(n_val.tag == FLOAT_TYPE && *((float*) n_val.aptr) == 9.81f);
    printf("  n.value = %.2f  ✓\n\n", *((float*) n_val.aptr));
    

    finalizeScope(manager); // back to Child A
    finalizeScope(manager); // back to ROOT

    // ================================================================
    // --- DEPTH 1: Child B (sibling of Child A) ---
    // arr: length=8
    // val: int = 55
    // ================================================================
    SymbolsTable* child_b = initializeScope(manager, DYNADICT_DEFAULT_CAPACITY);
    printf("  [Child B] Declaring arr, val...\n");

    int val_storage = 55;
    int arr_length = 8;

    initializeIdentifier(manager, "arr");
    insertAttribute(manager, "arr", attr_length,  (AnyType){.tag=INT_TYPE, .aptr=&arr_length});
    // arr address omitted — no single element address for an array

    initializeIdentifier(manager, "val");
    insertAttribute(manager, "val", attr_type,    (AnyType){.tag=INT_TYPE, .aptr=&int_type});
    insertAttribute(manager, "val", attr_value,   (AnyType){.tag=INT_TYPE, .aptr=&val_storage});
    insertAttribute(manager, "val", attr_address, (AnyType){.tag=PTR_TYPE, .aptr=&val_storage});
    

    // --- Verify Child B ---
    AnyType arr_len = *findStorage(manager, "arr", attr_length);
    assert(arr_len.tag == INT_TYPE && *((int*) arr_len.aptr) == 8);
    printf("  arr.length = %d  ✓\n", *((int*) arr_len.aptr));

    AnyType val_val = *findStorage(manager, "val", attr_value);
    assert(val_val.tag == INT_TYPE && *((int*) val_val.aptr) == 55);
    printf("  val.value = %d  ✓\n", *((int*) val_val.aptr));

    AnyType val_addr = *findStorage(manager, "val", attr_address);
    assert(val_addr.tag == PTR_TYPE && val_addr.aptr == &val_storage);
    printf("  val.address = %p  ✓\n\n", val_addr.aptr);


    finalizeScope(manager); // back to ROOT

    // ================================================================
    // --- Print full tree ---
    // ================================================================
    print_symbols_tree(manager, manager->root, "ROOT");

    printf("\n");
    printf("╔══════════════════════════════════════╗\n");
    printf("║          ALL CHECKS PASSED  ✔        ║\n");
    printf("╚══════════════════════════════════════╝\n\n");
}