#include "vector.h"

allocator_type *allocator = malloc;
deallocator_type *deallocator = free;
memcopy_type *memcopy = memcpy;

void set_funcs(allocator_type al, deallocator_type del, memcopy_type mc) {
    allocator = al;
    deallocator = del;
    memcopy = mc;
}

vector new_vec(size_t elem_size, size_t prealloc) {
    vector_metainfo* meta = (vector_metainfo*) allocator(sizeof(vector_metainfo) + elem_size*prealloc);
    meta->length = 0; meta->capacity = prealloc; meta->size = elem_size;
    vector vec = (void*) (meta + 1);
    return vec;
}

void check_not_null(vector vec) {
    if (vec == NULL) {
        puts("Vector is NULL. Make new vectors with new_vec()");
        exit(1);
    }
}

void delete_vec(vector vec) {
    check_not_null(vec);
    vector_metainfo* meta = (vector_metainfo*)vec - 1;
    deallocator(meta);
}

vector vec_add(vector vec, void* elem) {
    check_not_null(vec);   
    vector_metainfo *meta = (vector_metainfo*)vec - 1;
    if (meta->capacity == 0) {
        vector nv = new_vec(meta->size, STANDART_PREALLOC);
        delete_vec(vec);
        vec = nv;
        meta = (vector_metainfo*)vec - 1;
    } else if (meta->capacity == meta->length) {
        vector nv = new_vec(meta->size, (size_t)(meta->length*2));
        memcopy(nv, vec, meta->length*meta->size);
        vector_metainfo *newmeta = (vector_metainfo*)nv - 1;
        newmeta->length = meta->length;
        delete_vec(vec);
        vec = nv;
        meta = newmeta;
    }
    memcopy(vec + meta->length*meta->size, elem, meta->size);
    meta->length += 1;
    return vec;
}

void vec_remove(vector vec, size_t index) {
    check_not_null(vec);   
    vector_metainfo *meta = (vector_metainfo*)vec - 1;
    if (index >= meta->length) {
        puts("Index out of range");
        exit(1);
    }
    memcopy(vec + meta->size*(index), vec + meta->size*(index+1), (meta->length-index-1)*meta->size);
    meta->length -= 1;
}

vector_metainfo vec_meta(vector vec) {
    check_not_null(vec);
    return *((vector_metainfo*)vec - 1);
}

vector_metainfo *vec_metaptr(vector vec) {
    check_not_null(vec);
    return (vector_metainfo*)vec - 1;
}