#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <stdint.h>
#include <stdbool.h>
#include "dynarray.h"
#include "hash.h"


Hash *_hash_create(int node_amount, size_t stride, size_t key_stride, void* ptr_func, bool uses_key_storage)
{
    Hash* hash = malloc(sizeof(Hash));
    hash->table = calloc(node_amount, sizeof(Node*));
    hash->obj_storage = _dynarray_create(DYNARRAY_DEFAULT_CAP, stride);
    if(uses_key_storage){
        hash->key_storage = _dynarray_create(DYNARRAY_DEFAULT_CAP, key_stride);
    }
    hash->stride = stride;
    hash->key_stride = key_stride;
    hash->f_ptr = ptr_func;
    hash->capacity = node_amount;
    hash->holes = dynarray_create(int);
    hash->count = 0;
    hash->uses_keys = uses_key_storage;

    return hash;
}


bool _hash_in(Hash *hash, void *xptr, void *str_ptr, bool (*f_equality_ptr)(void*, void*), int action){
    uint64_t hash_id = hash->f_ptr(xptr);

    int bucket_id = hash_id%(hash->capacity);
    
    Node* slot = hash->table[bucket_id];
    if(slot != NULL){
        Node* tmp = slot;
        Node* prev = NULL;

        while(tmp != NULL){
            uint64_t curr_hash_id = tmp->hash_index;
            bool bucket_pass;
            if(hash->uses_keys){
                bucket_pass = f_equality_ptr(xptr, (char*) hash->key_storage + tmp->storage_index * hash->key_stride);
            }
            else{
                bucket_pass = f_equality_ptr(xptr, (char*) hash->obj_storage + tmp->storage_index * hash->stride);
            }
            if(hash_id == curr_hash_id && bucket_pass){
                
                if(action == -1){
                    if(prev != NULL){
                        prev->next = tmp->next;
                    }
                    else{
                        hash->table[bucket_id] = tmp->next;
                    }
                    dynarray_push(hash->holes,tmp->storage_index);
                    hash->count --;
                    free(tmp);
                }

                return true;
                
            }
            prev = tmp;
            tmp = tmp->next;
        }
    }

    if(action == 1){
        Node* node = malloc(sizeof(Node));
        node->next = NULL;
        int placement_index = dynarray_length(hash->obj_storage);
        if(dynarray_length(hash->holes)>0){
            dynarray_pop(hash->holes, &placement_index);
            _dynarray_replace(hash->obj_storage, str_ptr, placement_index);
            if(hash->uses_keys){
                _dynarray_replace(hash->key_storage, xptr, placement_index);
            }
        }
        else{
            hash->obj_storage = _dynarray_push(hash->obj_storage, str_ptr);
            if(hash->uses_keys){
                hash->key_storage = _dynarray_push(hash->key_storage, xptr);
            }
        }
        
        node->storage_index = placement_index;
        node->hash_index = hash_id;

        if(slot != NULL){
            node->next = slot;
        }

        hash->table[bucket_id] = node;

        hash->count ++;
    }

    return false;
}

void *_hash_get(Hash *hash, void *xptr, bool (*f_equality_ptr)(void*, void*)){
    uint64_t hash_id = hash->f_ptr(xptr);

    int bucket_id = hash_id%(hash->capacity);
    
    Node* slot = hash->table[bucket_id];
    if(slot != NULL){
        Node* tmp = slot;
        Node* prev = NULL;

        while(tmp != NULL){
            uint64_t curr_hash_id = tmp->hash_index;

            bool bucket_pass;
            if(hash->uses_keys){
                bucket_pass = f_equality_ptr(xptr, (char*) hash->key_storage + tmp->storage_index * hash->key_stride);
            }
            else{
                bucket_pass = f_equality_ptr(xptr, (char*) hash->obj_storage + tmp->storage_index * hash->stride);
            }
            if(hash_id == curr_hash_id && bucket_pass){
                return (void*)((char*) hash->obj_storage + tmp->storage_index * hash->stride);            
            }
            prev = tmp;
            tmp = tmp->next;
        }
    }

    return NULL;
}

void _hash_destroy(Hash *hash){
    for(int i=0;i<hash->capacity;i++){
        Node* slot = hash->table[i];
        if(slot != NULL){
            Node* tmp = slot;
            Node* prev = NULL;

            while(tmp != NULL){
                prev = tmp;
                tmp = tmp->next;
                free(prev);
            }
        }
    }
    free(hash->table);
    dynarray_destroy(hash->obj_storage);
    if(hash->uses_keys){
        dynarray_destroy(hash->key_storage);
    }
    dynarray_destroy(hash->holes);
    free(hash);
}

void *_hash_to_list(Hash *hash){
    void* arr = _dynarray_create(hash->count, hash->stride);
    for(int i=0;i<hash->capacity;i++){
        if(hash->table[i] != NULL){

            Node* tmp = hash->table[i];

            while(tmp != NULL){
                arr = _dynarray_push(arr, (char*) hash->obj_storage + tmp->storage_index * hash->stride);
                tmp = tmp->next;
            }
        }
    }
    return arr;
}

void *_hash_list_indexes(Hash *hash){
    void* arr = _dynarray_create(hash->count, hash->key_stride);
    for(int i=0;i<hash->capacity;i++){
        if(hash->table[i] != NULL){

            Node* tmp = hash->table[i];

            while(tmp != NULL){
                arr = _dynarray_push(arr, (char*) hash->key_storage + tmp->storage_index * hash->key_stride);
                tmp = tmp->next;
            }
        }
    }
    return arr;
}

uint64_t _hash_int(void *xptr) {
    uint64_t x = (uint64_t) *((int*) xptr);
    x = (x ^ (x >> 30)) * UINT64_C(0xbf58476d1ce4e5b9);
    x = (x ^ (x >> 27)) * UINT64_C(0x94d049bb133111eb);
    x = x ^ (x >> 31);
    return x;
}

uint64_t hash_combine(uint64_t lhs, uint64_t rhs ) {
    if (sizeof(uint64_t) >= 8) {
        lhs ^= rhs + 0x517cc1b727220a95 + (lhs << 6) + (lhs >> 2);
    } 
    else {
        lhs ^= rhs + 0x9e3779b9 + (lhs << 6) + (lhs >> 2);
    }
    return lhs;
}

uint32_t _djb33_hash(void *xptr)
{
    const char* s = *((char**) xptr);
    uint32_t h = 5381;
    int c;

    while (c = *s++) {
        /* h = 33 * h ^ s[i]; */
        h = ((h << 5) + h) ^ c; 
    }
    return h;
}

bool string_equal(void* ptr1, void* ptr2){
    char* str1 = *((char**) ptr1);
    char* str2 = *((char**) ptr2);

    return strcmp(str1, str2) == 0;
}
