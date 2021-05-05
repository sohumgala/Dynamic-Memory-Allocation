/*
 * My implementations of the Dynamic Memory Allocation functions malloc, calloc, realloc, and free
 * Author: Sohum Gala
 */

#include <stdint.h>
#include <string.h>
#include <stdio.h>
#include "my_malloc.h"

/* Head of the free list, a singly linked list sorted by address of available memory */
metadata_t *address_list;

/* Set on every invocation of my_malloc()/my_free()/my_realloc()/my_calloc() to indicate success or the type of failure */
enum my_malloc_err my_malloc_errno;


// Helper Functions

/* merge helper function:
 * given pointers to adjacent free memory in the freelist, merge them
 */
static void merge(metadata_t *left, metadata_t *right) {
    left->size = left->size + right->size + TOTAL_METADATA_SIZE;
    left->next = right->next;
}

/* mergeAll helper function:
 * iterates through the free list and merges all adjacent blocks of memory
*/
static void mergeAll() {
    if (address_list == NULL) {
        return;
    }
    metadata_t *curr = address_list->next;
    metadata_t *prev = address_list;
    int i = 0;
    while (curr != NULL) {
        if ((uintptr_t)((char *)prev + TOTAL_METADATA_SIZE + prev->size) == (uintptr_t)curr) {
            merge(prev, curr);
            i = 1;
        }
        prev = curr;
        curr = curr->next;
    }
    if (i) {
        mergeAll();
    }
}


/* split_block helper function:
 * given a large block and a size, split a block of the specified size off the back of the larger block and return the pointer to it
 * the sizes of both blocks are updated accordingly
 */
static metadata_t *split_block(metadata_t *block, size_t size) {
    size_t realSize = size + TOTAL_METADATA_SIZE;
    metadata_t *newBlock = (metadata_t *)((char *)block + TOTAL_METADATA_SIZE + block->size - realSize);
    block->size -= realSize;
    newBlock->size = size;
    return newBlock;
}

/* add_to_addr_list helper function:
 * This function adds a block to the appropriate spot on the free list
 */
static void add_to_addr_list(metadata_t *block) {
    if (address_list == NULL) {
        address_list = block;
        return;
    }
    metadata_t *curr = address_list;
    metadata_t *prev = NULL;
    while (curr != NULL && (uintptr_t)curr < (uintptr_t)block) {
        prev = curr;
        curr = curr->next;
    }
    if (prev == NULL) {
        address_list = block;
        block->next = curr;
    } else {
        prev->next = block;
        block->next = curr;
    }
    return;
}

/* remove_from_addr_list helper function:
 * This function removes a block from the freelist
 */
static void remove_from_addr_list(metadata_t *block) {
    if (address_list == NULL) {
        return;
    }
    metadata_t *curr = address_list;
    metadata_t *prev = NULL;
    while (curr != NULL && (uintptr_t)curr != (uintptr_t)block) {
        prev = curr;
        curr = curr->next;
    }
    if (prev == NULL) {
        address_list = curr->next;
    } else if (curr == NULL) {
        return;
    } else {
        prev->next = curr->next;
    }
    return;
}

/* find_best_fit helper function: 
 * This function finds and returns a pointer to the best fit block of memory.
 * If there are no blocks of exctly the specified size, return a pointer to the smallest.
 * If there are no usable blocks of memory in the free list, return NULL.
 */
static metadata_t *find_best_fit(size_t size) {
    if (address_list == NULL) {
        return NULL;
    }
    metadata_t *curr = address_list;
    unsigned long bestSize = 0xffffffff;
    while (curr != NULL) {
        if (curr->size == size) {
            return curr;
        } else if (curr->size > size && curr->size < bestSize) {
            bestSize = curr->size;
        }
        curr = curr->next;
    }
    curr = address_list;
    while (curr != NULL) {
        if (curr->size == bestSize) {
            return curr;
        } else {
            curr = curr->next;
        }
    }
    return NULL;
}


// memory allocation functions

/* MALLOC
 * allocate and return a pointer to a block of memory in the heap, returning NULL in case of failure
 */
void *my_malloc(size_t size) {
    my_malloc_errno = NO_ERROR;
    if (size > SBRK_SIZE - TOTAL_METADATA_SIZE) {
        my_malloc_errno = SINGLE_REQUEST_TOO_LARGE;
        return NULL;
    } else if (size == 0) {
        my_malloc_errno = NO_ERROR;
        return NULL;
    }
    metadata_t *temp = find_best_fit(size);
    if (temp == NULL) {
        metadata_t *newBlock = my_sbrk(SBRK_SIZE);
        if (newBlock == (metadata_t *)-1) {
            my_malloc_errno = OUT_OF_MEMORY;
            return NULL;
        }
        newBlock->size = SBRK_SIZE - TOTAL_METADATA_SIZE;
        newBlock->next = NULL;
        add_to_addr_list(newBlock);
        mergeAll();

        temp = find_best_fit(size);
        if (temp == NULL || temp->size < size) {
            my_malloc_errno = OUT_OF_MEMORY;
            return NULL;
        }
    }
    if (temp->size == size) {
        remove_from_addr_list(temp);
        my_malloc_errno = NO_ERROR;
        return (char *)temp + TOTAL_METADATA_SIZE;
    } else if (temp->size > size) {
        if (temp->size >= size + TOTAL_METADATA_SIZE + 1) {
            metadata_t *ret = split_block(temp, size);
            ret->size = size;
            my_malloc_errno = NO_ERROR;
            return (char *)ret + TOTAL_METADATA_SIZE;
        } else {
            my_malloc_errno = NO_ERROR;
            remove_from_addr_list(temp);
            return (char *)temp + TOTAL_METADATA_SIZE;
        }
    }
    return (NULL);
}

/* REALLOC
 * reallocates memory previously allocated to a new size specified by the caller, returning NULL in case of failure
 */
void *my_realloc(void *ptr, size_t size) {
    my_malloc_errno = NO_ERROR;

    if (ptr == NULL) {
        return my_malloc(size);
    }
    if (size == 0) {
        my_free(ptr);
        return NULL;
    }
    void *temp = my_malloc(size);
    if (temp == NULL) {
        return NULL;
    }
    metadata_t *curr = (metadata_t *) ((char *)ptr - TOTAL_METADATA_SIZE);
    int min = (curr->size > size) ? size : curr->size;

    memcpy(temp, ptr, min);
    my_free(ptr);
    return temp;
}

/* CALLOC
 * allocates and returns a pointer to nmemb * size bytes of memory on the heap, clearing all values to 0, returning NULL in case of failure
 */
void *my_calloc(size_t nmemb, size_t size) {
    my_malloc_errno = NO_ERROR;
    void *temp = my_malloc(nmemb * size);
    if (temp == NULL) {
        return NULL;
    }
    memset(temp, 0, nmemb * size);

    return temp;
}

/* FREE
 * frees a block of memory previously allocated by a malloc function, adding it back to the free list
 */
void my_free(void *ptr) {
    my_malloc_errno = NO_ERROR;
    if (ptr == NULL) {
        return;
    }
    metadata_t *addr = (metadata_t *) ((char *)ptr - TOTAL_METADATA_SIZE);
    add_to_addr_list(addr);
    mergeAll();
}
