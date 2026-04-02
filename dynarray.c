#include <stdio.h>
#include <stdlib.h>
#include "dynarray.h"
#include <assert.h>

/*

Dynamic Array

A dynarray has three hidden fields of type `size_t` stored in it's header:
    - capacity: size in `stride`-sized units of the allocated buffer.
    - length: the number of `stride`-sized units currently filled.
    - stride: the sizeof the datatype being stored in the dynarray.

To get the ith element in the array, you can use bracket notation (`arr[i]`),
or the `dynarray_get` method which does bounds checking.

To set the ith element of the array, use either bracket notation
(`arr[i] = x;`), or the `dynarray_set` method which does bounds checking.
*/

// Returns a pointer to the start of a new dynarray (after the header) which
// has `init_cap` units of `stride` bytes.
void *_dynarray_create(size_t init_cap, size_t stride)
{
    size_t header_size = DYNARRAY_FIELDS * sizeof(size_t);
    size_t arr_size = init_cap * stride;
    size_t *arr = (size_t *) malloc(header_size + arr_size);
    arr[CAPACITY] = init_cap;
    arr[LENGTH] = 0;
    arr[STRIDE] = stride;
    return (void *) (arr + DYNARRAY_FIELDS);
}

void *_dynarray_copy(void* arr)
{
    size_t header_size = DYNARRAY_FIELDS * sizeof(size_t);
    size_t arr_size = dynarray_stride(arr) * dynarray_length(arr);
    size_t *new_arr = (size_t *) malloc(header_size + arr_size);
    memcpy(new_arr, (size_t*) arr - DYNARRAY_FIELDS, header_size + arr_size);

    return (void *) (new_arr + DYNARRAY_FIELDS);
}

void _dynarray_destroy(void *arr)
{
    free(arr - DYNARRAY_FIELDS * sizeof(size_t));
}

// Returns the dynarray's field which is specified by passing
// one of CAPACITY, LENGTH, STRIDE.
size_t _dynarray_field_get(void *arr, size_t field)
{
    return ((size_t *)(arr) - DYNARRAY_FIELDS)[field];
}

void _dynarray_field_set(void *arr, size_t field, size_t value)
{
    ((size_t *)(arr) - DYNARRAY_FIELDS)[field] = value;
}

// Allocates a new dynarray with twice the size of the one passed in, and retaining
// the values that the original stored.
void *_dynarray_resize(void *arr)
{
    void *temp = _dynarray_create( // Allocate new dynarray w/ more space.
        DYNARRAY_RESIZE_FACTOR * dynarray_capacity(arr),
        dynarray_stride(arr)
    );
    memcpy(temp, arr, dynarray_length(arr) * dynarray_stride(arr)); // Copy erythin' over.
    _dynarray_field_set(temp, LENGTH, dynarray_length(arr)); // Set `length` field.
    _dynarray_destroy(arr); // Free previous array.
    return temp;
}

void *_dynarray_push(void *arr, void *xptr)
{
    if (dynarray_length(arr) >= dynarray_capacity(arr))
        arr = _dynarray_resize(arr);

    memcpy(arr + dynarray_length(arr) * dynarray_stride(arr), xptr, dynarray_stride(arr));
    _dynarray_field_set(arr, LENGTH, dynarray_length(arr) + 1);
    return arr;
}


void *_dynarray_pushleft(void *arr, void *xptr)
{
    if (dynarray_length(arr) >= dynarray_capacity(arr))
        arr = _dynarray_resize(arr);

    memmove(arr + dynarray_stride(arr), arr, dynarray_stride(arr)*dynarray_length(arr));
    memcpy(arr, xptr, dynarray_stride(arr));
    _dynarray_field_set(arr, LENGTH, dynarray_length(arr) + 1);
    return arr;
}

void _dynarray_replace(void *arr, void *xptr, int index)
{
    assert(index < dynarray_length(arr));
    assert(index >= 0);
    memcpy(arr + index * dynarray_stride(arr), xptr, dynarray_stride(arr));
}

// Removes the last element in the array, but copies it to `*dest` first.
void _dynarray_pop(void *arr, void *dest)
{
    //printf("FUCK HELL -> %p\n", arr + (dynarray_length(arr) - 1) * dynarray_stride(arr));
    memcpy(dest, arr + (dynarray_length(arr) - 1) * dynarray_stride(arr), dynarray_stride(arr));
    _dynarray_field_set(arr, LENGTH, dynarray_length(arr) - 1); // Decrement length.
}

void _dynarray_reset(void *arr)
{
    _dynarray_field_set(arr, LENGTH, 0); // Reset length.
}