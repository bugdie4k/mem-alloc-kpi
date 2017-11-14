#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>

/* Page descriptor structure (8 bytes):
 * | 0 0 | N N | B B | A | F |
 *         \_/   \_/   \   \--- FREE STATUS (FREE = 1, OCCUPIED = 0)
 *          |     |     \------ ALLOC STATUS (BLOCKS = 1, MULTIPAGE = 0)
 *          |     \------------ BLOCK SIZE (if A = 1)                     | NUMBER OF PAGES IN MULTIBLK
 *          \------------------ NUM OF BLOCKS IN PAGE                     | NUMBER OF THIS PAGE IN MULTIBLK
 */

// statuses
const int _stat_free = 1;
const int _stat_occupied = 0;
const int _stat_blocks = 1;
const int _stat_multipage = 0;

// sizes
const int _size_mem = 4000000;
const int _size_page_header = 8;
const int _size_page = 4000;
const int _size_headless_page = 3992;

// page masks
// NOTE: bitwise operations work only on 4 bytes
// - 4 junior bytes
const int _p_mask_free =  0x000000FF;
const int _p_mask_alloc =  0x0000FF00;
const int _p_mask_size = 0xFFFF0000;
// - 4 senior bytes
const int _p_mask_num = 0x0000FFFF;

int align_to_4(int x) {
  return ((x) & (-1 - 3)) + ((((x) & 3) != 0) ? 4 : 0);
}

typedef struct {
     void* pptr;
     unsigned shift;
} blk_t;

// mem pointers
void* mem_beg;
void* mem_end;

// page header accessors
// - free status
void p_set_free(void* pptr, int free) {
  *(int*)(pptr + 4) = (*(int*)(pptr + 4) & ~_p_mask_free) | (free & _p_mask_free);
}
int  p_get_free(void* pptr) {
    return *(int*)(pptr + 4) & _p_mask_free;
}

// - alloc status
void p_set_alloc(void* pptr, int alloc) {
    *(int*)(pptr + 4) = (*(int*)(pptr + 4) & ~_p_mask_alloc) | ((alloc << 8) & _p_mask_alloc);
}
int  p_get_alloc(void* pptr) {
    return (*(int*)(pptr + 4) & _p_mask_alloc) >> 8;
}

// - block size OR number of pages in multiblock
void p_set_size(void* pptr, int size) {
    *(int*)(pptr + 4) = (*(int*)(pptr + 4) & ~_p_mask_size) | ((size << 16) & _p_mask_size);
}
int  p_get_size(void* pptr) {
    return (*(int*)(pptr + 4) & _p_mask_size) >> 16;
}

// - blocks num OR number of this page in multiblock
void p_set_num(void* pptr, int num) {
    *(int*)(pptr) = (*(int*)(pptr) & ~_p_mask_num) | (num & _p_mask_num);
}
int  p_get_num(void* pptr) {
    return (*(int*)(pptr) & _p_mask_num);
}

// dump

void dump_pg_head(void* pptr) {
    unsigned blocks_p = p_get_alloc(pptr);
    unsigned free_p = p_get_free(pptr);

    if (blocks_p) {
        printf("paddr: %#14lx | %9s | %9s | %6s | %8s |\n", pptr, "FREE?", "ALLOC", "BLKS", "BLK SIZE");
    } else {
        printf("paddr: %#14lx | %9s | %9s | %6s | %8s |\n", pptr, "FREE?", "ALLOC", "PG#", "PGS");
    }

    printf("       %14s | %9s | %9s | %06u |   %06u |\n",
           "",
           (p_get_free(pptr)) ? "FREE" : "OCCUPIED",
           (p_get_alloc(pptr)) ? "BLOCKS" : "MULTIPAGE",
           p_get_num(pptr),
           p_get_size(pptr));
    printf("-------------------------------------------------------------------\n");
}

void dump() {
    printf("============================== DUMP ===============================\n");

    unsigned free_pgs = 0;
    for (void* pptr = mem_beg; pptr != mem_end; pptr += _size_page) {
        if (p_get_free(pptr)) {
            ++free_pgs;
        } else {
            dump_pg_head(pptr);
        }
    }

    printf("free pages: %u\n", free_pgs);
    printf("===================================================================\n\n");
}

//
void occupy_page_with_blocks(void* pptr, unsigned blk_sz) {
    p_set_free(pptr, _stat_occupied);
    p_set_alloc(pptr, _stat_blocks);
    p_set_size(pptr, blk_sz);
    p_set_num(pptr, 0);
}

unsigned not_out_of_page(void* pptr, void* ptr) {
    return (abs(ptr - pptr) < _size_page_header) ? 1 : 0;
}

blk_t alloc_block(void* pptr) {
    unsigned new_num = p_get_num(pptr) + 1;
    p_set_num(pptr, new_num);
    blk_t retval;
    retval.pptr = pptr;
    retval.shift = new_num;
    return retval;
}

void mem_init_page(void* pptr) {
    p_set_free(pptr, _stat_free);
    p_set_alloc(pptr, 0);
    p_set_size(pptr, 0);
    p_set_num(pptr, 0);
}

void mem_init(unsigned size) {
    mem_beg = malloc(size);
    mem_end = mem_beg + _size_mem;

    for (void* pptr = mem_beg; pptr != mem_end; pptr += _size_page) {
        mem_init_page(pptr);
    }
}

blk_t alloc_lt_page_size(unsigned blk_sz) {
    void* free_page = NULL;
    void* fits_size_page = NULL;

    for (void* pptr = mem_beg; pptr != mem_end; pptr += _size_page) {
        if (p_get_free(pptr)) {
            if (free_page == NULL) free_page = pptr;
        } else if ((p_get_alloc(pptr) == _stat_blocks) && (p_get_size(pptr) == blk_sz)) {
            fits_size_page = pptr;
            break;
        }
    }

    if (fits_size_page == NULL) {
        if (free_page != NULL) {
            occupy_page_with_blocks(free_page, blk_sz);
            return alloc_block(free_page);
        } else {
            printf("mem_alloc: no free space\n");
        }
    } else {
        return alloc_block(fits_size_page);
    }
}

void occupy_pages_with_multiblk(void* pptr_arg, unsigned pages_n) {
    void* pptr = pptr_arg;

    p_set_num(pptr, 0);
    p_set_size(pptr, pages_n);
    p_set_alloc(pptr, _stat_multipage);
    p_set_free(pptr, _stat_occupied);

    for (unsigned pg = 1; pg < pages_n; ++pg) {
        pptr = pptr + _size_page * pg;

        p_set_num(pptr, pg);
        p_set_size(pptr, pages_n);
        p_set_alloc(pptr, _stat_multipage);
        p_set_free(pptr, _stat_occupied);
    }
}

// TODO: refine
blk_t alloc_gt_page_size(unsigned pages_n) {
    unsigned prv_free = 0;
    unsigned free_found = 0;
    void *free_page1 = NULL;

    for (void* pptr = mem_beg; pptr != mem_end; pptr += _size_page) {
        if (p_get_free(pptr)) {
            if (prv_free) {
                ++free_found;
            } else {
                free_found = 1;
                free_page1 = pptr;
            }
            prv_free = 1;
        } else {
            prv_free = 0;
        }
    }

    if (free_found >= pages_n) {
        occupy_pages_with_multiblk(free_page1, pages_n);
        blk_t retval;
        retval.pptr = free_page1;
        return retval;
    } else {
        printf("mem_alloc: not enough mem");
    }
}

void* get_blk_ptr(blk_t blk) {
    return blk.pptr + _size_page_header + blk.shift * p_get_size(blk.pptr);
}

blk_t mem_alloc(unsigned size) {
    unsigned aligned_size = align_to_4(size);
    if (aligned_size <= _size_headless_page / 2) {
        return alloc_lt_page_size(aligned_size);
    } else {
        return alloc_gt_page_size((aligned_size / _size_headless_page) + 1);
    }
}

void mem_free(blk_t blk) {
     if (p_get_alloc(blk.pptr) == _stat_multipage) {
        unsigned num = p_get_num(blk.pptr);
        unsigned tot = p_get_size(blk.pptr);
        for (void* pptr = blk.pptr; pptr != blk.pptr + tot * _size_page; pptr += _size_page) {
            p_set_free(pptr, _stat_free);
        }
    } else {
         if (p_get_num(blk.pptr) - 1) {
             p_set_num(blk.pptr, p_get_num(blk.pptr) - 1);
         } else {
             p_set_free(blk.pptr, _stat_free);
         }
    }
}

void test() {
    mem_init(_size_mem);

    printf("mem_beg: %#14lx\n", mem_beg);
    printf("mem_end: %#14lx\n", mem_end);

    blk_t a1 = mem_alloc(1);
    blk_t a2 = mem_alloc(2);
    blk_t a3 = mem_alloc(3);
    blk_t a4 = mem_alloc(4);

    dump();

    mem_free(a1);
    mem_free(a2);

    dump();

    blk_t b1 = mem_alloc(10);
    blk_t b2 = mem_alloc(20);
    blk_t b3 = mem_alloc(30);
    blk_t b4 = mem_alloc(30);

    dump();

    mem_free(b1);
    mem_free(b2);

    dump();

    blk_t c1 = mem_alloc(5000);
    mem_alloc(13000);

    dump();

    mem_free(c1);

    dump();
}

unsigned main(unsigned argc, char** argv) {
    test();
    return 0;
}
