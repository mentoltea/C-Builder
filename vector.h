#ifndef s7k_vector_lib
#define s7k_vector_lib

#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#define STANDART_PREALLOC 10

typedef void*(allocator_type)(size_t);
typedef void(deallocator_type)(void*);
typedef void*(memcopy_type)(void* dest, const void* src, size_t numbytes);

extern allocator_type *allocator;
extern deallocator_type *deallocator;
extern memcopy_type *memcopy;

void set_funcs(allocator_type, deallocator_type, memcopy_type);

typedef struct {
    size_t length; // number of elements
    size_t capacity; // capacity in number of elemets
    size_t size; // size of one element
} vector_metainfo;

typedef void* vector;
vector vec_add(vector vec, void* elem);
// @note index MUST be in range
void vec_remove(vector vec, size_t index);

void check_not_null(vector vec);
vector_metainfo vec_meta(vector vec);
vector_metainfo *vec_metaptr(vector vec);

// @param elem_size size of one element
// @param prealloc number of elements to preallocate (capacity)
vector new_vec(size_t elem_size, size_t prealloc);
void delete_vec(vector vec);

#endif