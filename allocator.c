#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <stdbool.h>
#include "allocator.h"

Arena* arena_create(size_t arena_size){
    Arena* header =(Arena *) malloc(sizeof(Arena)+arena_size);
    header->capacity = arena_size;
    header->occupied = 0;
    header->next = NULL;
    header->current = header;
    return header;
}

Arena* _get_header(void *arena)
{
    return ((Arena *)(arena) - 1);
}

void* _get_storage(Arena *header)
{
    return ((void *)(header + 1));
}

Arena* _arena_expand(Arena* header, size_t expansion_size){
    Arena* expansion = arena_create(expansion_size);
    header->current->next = expansion;
    header->current = expansion;
    return expansion;
}

Arena* _arena_expand_auto(Arena* header){
    return _arena_expand(header, header->capacity);
}

void* arena_get(Arena* header, size_t chunk_size){
    void* chunk;
    if(header->current->occupied + chunk_size <= header->capacity){
        //printf("SAVE STORAGE\n");
        chunk = _get_storage(header->current) + header->current->occupied;
        header->current->occupied += chunk_size;
        return chunk;
    }
    else if(chunk_size > header->capacity){
        printf("EXPAND FIT\n");
        Arena* new_header_fit = _arena_expand(header, chunk_size);
        chunk = _get_storage(new_header_fit);
        new_header_fit->occupied += chunk_size;
        return chunk;
    }
    else{
        printf("EXPAND NORMAL\n");
        Arena* new_header = _arena_expand_auto(header);
        chunk = _get_storage(new_header);
        new_header->occupied += chunk_size;
        
        return chunk;
    }
    
}

void arena_destroy(Arena* header){
    assert(header != NULL);
    if (header == NULL) return;

    if(header->next != NULL){
        arena_destroy(header->next);
    }

    free(header);
}


