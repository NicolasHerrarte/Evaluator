#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <stdbool.h>

typedef struct Arena {
    struct Arena *next;
    struct Arena *current;
    size_t capacity;
    size_t occupied;
} Arena;

Arena* arena_create(size_t arena_size);
Arena* _get_header(void *arena);
void* _get_storage(Arena *header);
Arena* _arena_expand(Arena* header, size_t expansion_size);
Arena* _arena_expand_auto(Arena* header);
void* arena_get(Arena* header, size_t chunk_size);
void arena_destroy(Arena* header);