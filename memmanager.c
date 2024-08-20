#include "memmanager.h"

allocator_func_type *cool_allocator = malloc;
deallocator_func_type *cool_deallocator = free;

Page *start_page = NULL;

void init_allocator(allocator_func_type alloc, deallocator_func_type dealloc) {
    if (alloc && dealloc) {cool_allocator = alloc; cool_deallocator = dealloc;}
}

Chunk* new_chunk(void* start, void* end) {
    if (end-start <= sizeof(Chunk)) exit(1);
    Chunk *ptr = (Chunk*) start;
    //ptr->start = start;
    ptr->end = end;
    ptr->next = NULL;
    return ptr;
}

inline void* chunk_data(Chunk* ptr) {
    return (void*) ((Chunk*) ptr+1);
}

// ptr1 - ptr2
inline size_t chunk_diff(Chunk* ptr1, Chunk* ptr2){
    return (size_t) ((void*) ptr1 - (void*) ptr2);
}

void* try_allocate(Page* page, size_t size) {
    if (size <= sizeof(Chunk)) exit(1);
    size_t remain_cap = page->capacity;
    Chunk* ptr1 = page->chunk_chain;
    if (!ptr1) {
        page->chunk_chain = new_chunk(page->pointer, page->pointer+size);
        page->size -= size;
        return chunk_data(page->chunk_chain);
    }
    Chunk* ptr2 = ptr1->next;
    Chunk* temp; size_t diff;
    while (ptr2 != NULL) {
        diff = chunk_diff(ptr2, ptr1->end);
        if (diff >= size) {
            temp = new_chunk(ptr1->end, ptr1->end+size);
            ptr1->next = temp;
            //temp->prev = ptr1;
            //ptr2->prev = temp; 
            temp->next = ptr2;
            page->size -= size;
            return chunk_data(temp);
        }
        remain_cap -= diff;
        if (remain_cap < size) return NULL;
        ptr1 = ptr2;
        ptr2 = ptr2->next;
    }
    void* end = page->pointer + page->capacity;
    diff = end - ptr1->end;
    if (diff >= size) {
        temp = new_chunk(ptr1->end, ptr1->end+size);
        ptr1->next = temp; //temp->prev = ptr1;
        page->size -= size;
        return chunk_data(temp);
    }

    return NULL;
}

Page* new_page(size_t size) {
    Page *ptr;
    ptr = cool_allocator(sizeof(Page) + size);
    ptr->prev = NULL; ptr->next = NULL;
    ptr->chunk_chain = NULL;
    ptr->pointer = (void*) (ptr+1);
    ptr->size = size; ptr->capacity = size;
    return ptr;
}

void *memloc(size_t size) {
    if (size == 0) return NULL;
    size += sizeof(Chunk);
    size_t pagesize = (size > PAGE_SIZE) ? size : PAGE_SIZE;
    if (start_page==NULL) {
        start_page = new_page(pagesize);
    }
    Page* ptr = start_page;
    void* result;
    while (ptr->next!=NULL) {
        if (ptr->size >= size) {
            result = try_allocate(ptr, size);
            if (result) return result;
        }
        ptr = ptr->next;
    }
    if (ptr->size >= size) {
        result = try_allocate(ptr, size);
        if (result) return result;
    }
    ptr->next = new_page(pagesize);
    ptr->next->prev = ptr;
    result = try_allocate(ptr->next, size);
    return result;
}

void *memnew(size_t size) {
    if (size == 0) return NULL;
    size += sizeof(Chunk);
    Page* ptr;
    void* result;
    if (start_page==NULL) {
        start_page = new_page(size);
    }
    ptr = start_page;
    while (ptr->next != NULL) {
        ptr = ptr->next;
    }
    ptr->next = new_page(size);
    ptr->next->prev = ptr;
    result = try_allocate(ptr->next, size);
    return result;
}

void prealloc(size_t size) {
    if (size <= sizeof(Chunk)) size = PAGE_SIZE;

    if (start_page==NULL) {
        start_page = new_page(size);
    } else {
        start_page->prev = new_page(size);
        start_page->prev->next = start_page;
        start_page = start_page->prev;
    }
}

void prealloc_end(size_t size) {
    if (size <= sizeof(Chunk)) size = PAGE_SIZE;

    if (start_page==NULL) {
        start_page = new_page(size);
    } else {
        Page* ptr = start_page;
        while (ptr->next != NULL) {
            ptr = ptr->next;
        }
        ptr->next = new_page(size);
        ptr->next->prev = ptr;
    }
}

void chunkfree(Page* page, void* pointer) {
    Chunk* ptr = page->chunk_chain;
    Chunk* prv = NULL;
    while (ptr != NULL) {
        if (pointer == chunk_data(ptr)) {
            if (prv) prv->next = ptr->next;
            else page->chunk_chain = ptr->next;

            page->size += chunk_diff(ptr->end, ptr);
            return;
        }
        prv = ptr;
        ptr = ptr->next;
    }
    //no such adress or wrongly selected
}

void memfree(void *data) {
    Page* ptr = start_page;
    void *start, *end;
    while (ptr != NULL) {
        start = ptr->pointer;
        end = ptr->pointer + ptr->capacity;
        if (data >= start && data < end) {
            chunkfree(ptr, data);
            return;
        }
        ptr = ptr->next;
    }
    //no such adress in use
}

void destroy_pages() {
    Page* pageptr = start_page;
    if (!start_page) return;
    while (pageptr->next != NULL) {
        pageptr = pageptr->next;
        cool_deallocator(pageptr->prev);
    }
    cool_deallocator(pageptr);
    start_page = NULL;
}

void programm_end() {
    destroy_pages();
}


void page_info(short chunk_info) {
    Page* ptr;
    Chunk* ch;
    ptr = start_page;
    int i=0;
    size_t total = 0;
    printf("\n\n--------\n");
    while (ptr!=NULL){
        total = total + (size_t)(ptr->capacity - ptr->size);
        printf("%d: %ld/%ld (%ld used)\n", i, ptr->size, ptr->capacity, ptr->capacity - ptr->size);
        if (chunk_info) {
            ch = ptr->chunk_chain;
            while (ch != NULL) {
                if (chunk_diff(ch->end, ch) >= sizeof(int)) 
                    printf("\t%d\n", *((unsigned int*)chunk_data(ch)));
                else
                    printf("\t%d\n", (int)*((unsigned char*)chunk_data(ch)));
                ch = ch->next;
            }
        }
        ptr = ptr->next;
        i++;
    }
    printf("total: %ld used\n", total);
    printf("--------\n\n");
}