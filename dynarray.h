#ifndef DYNARRAY
#define DYNARRAY

#include <stdlib.h>     // malloc
#include <string.h>     // memcpy

/* Structure of a dynarray:
 * size_t capacity
 * size_t length
 * size_t stride
 * void *memory
 */

enum {
    CAPACITY,
    LENGTH,
    STRIDE,
    DYNARRAY_FIELDS
};

void *_dynarray_create(size_t length, size_t stride);
void *_dynarray_copy(void* arr);
void _dynarray_destroy(void *arr);

size_t _dynarray_field_get(void *arr, size_t field);
void _dynarray_field_set(void *arr, size_t field, size_t value);

void *_dynarray_resize(void *arr);

void *_dynarray_push(void *arr, void *xptr);
void *_dynarray_pushleft(void *arr, void *xptr);
void _dynarray_replace(void *arr, void *xptr, int index);
void _dynarray_pop(void *arr, void *dest);
void _dynarray_reset(void *arr);

#define DYNARRAY_DEFAULT_CAP 1
#define DYNARRAY_RESIZE_FACTOR 2

#define dynarray_create(type) _dynarray_create(DYNARRAY_DEFAULT_CAP, sizeof(type))
#define dynarray_create_prealloc(type, capacity) _dynarray_create(capacity, sizeof(type))
#define dynarray_copy(arr) _dynarray_copy(arr)
#define dynarray_destroy(arr) _dynarray_destroy(arr)

#define dynarray_push(arr, x) arr = _dynarray_push(arr, &x)
#define dynarray_pushleft(arr, x) arr = _dynarray_pushleft(arr, &x)
#define dynarray_replace(arr, x, ind) _dynarray_replace(arr, &x, int index)
#define dynarray_get_last(arr) arr[dynarray_length(arr)-1]

#define dynarray_push_rval(arr, x) \
    do { \
        __auto_type temp = x; \
        arr = _dynarray_push(arr, &temp); \
    } while (0)

#define dynarray_pop(arr, xptr) _dynarray_pop(arr, xptr)
#define dynarray_reset(arr) _dynarray_reset(arr)

#define dynarray_capacity(arr) _dynarray_field_get(arr, CAPACITY)
#define dynarray_length(arr) _dynarray_field_get(arr, LENGTH)
#define dynarray_stride(arr) _dynarray_field_get(arr, STRIDE)

#endif // DYNARRAY