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
#define FREE 1
#define OCCUPIED 0
#define BLOCKS 1
#define MULTIPAGE 0

// sizes
#define PHEADER_SIZE  8
#define PAGE_SIZE     4000    // 4Kb
#define APAGE_SIZE    3992    // (PAGE_SIZE - PHEADER_SIZE)
#define MEM_SIZE      4000000 // 4Mb = 1000 * 4Kb

#define ALIGN4(x) (((x) & (-1 - 3)) + ((((x) & 3) != 0) ? 4 : 0))

typedef struct {
     void* pptr;
     unsigned shift;
} blk_t;

// page masks
const int pfs_mask =  0x000000FF;
const int pas_mask =  0x0000FF00;
const int pbsz_mask = 0xFFFF0000;

const int pnum_mask = 0x0000FFFF;

// mem pointers
void* mem_beg;
void* mem_end;

// page free status
void p_set_fs(void* pptr, int free) {
    *(int*)(pptr + 4) = (*(int*)(pptr + 4) & ~pfs_mask) | (free & pfs_mask);
}
int  p_get_fs(void* pptr) {
    return *(int*)(pptr + 4) & pfs_mask;
}

// page alloc status
void p_set_as(void* pptr, int alloc) {
    *(int*)(pptr + 4) = (*(int*)(pptr + 4) & ~pas_mask)
                      | ((alloc << 8) & pas_mask);
}
int  p_get_as(void* pptr) {
    return (*(int*)(pptr + 4) & pas_mask) >> 8;
}

// page block size
void p_set_bsz(void* pptr, int blk_sz) {
    *(int*)(pptr + 4) = (*(int*)(pptr + 4) & ~pbsz_mask)
                      | ((blk_sz << 16) & pbsz_mask);
}
int  p_get_bsz(void* pptr) {
    return (*(int*)(pptr + 4) & pbsz_mask) >> 16;
}

// page blocks num
void p_set_num(void* pptr, int num) {
    *(int*)(pptr) = (*(int*)(pptr) & ~pnum_mask) | (num & pnum_mask);
}
int  p_get_num(void* pptr) {
    return (*(int*)(pptr) & pnum_mask);
}

// dump

void dump_pg_head(void* pptr) {
    unsigned blocks_p = p_get_as(pptr);
    unsigned free_p = p_get_fs(pptr);

    if (blocks_p) {
        printf("paddr: %#14lx | %9s | %9s | %6s | %8s |\n", pptr, "FREE?", "ALLOC", "BLKS", "BLK SIZE");
    } else {
        printf("paddr: %#14lx | %9s | %9s | %6s | %8s |\n", pptr, "FREE?", "ALLOC", "PG#", "PGS");
    }

    printf("       %14s | %9s | %9s | %06lu |   %06lu |\n",
           "",
           (p_get_fs(pptr)) ? "FREE" : "OCCUPIED",
           (p_get_as(pptr)) ? "BLOCKS" : "MULTIPAGE",
           p_get_num(pptr),
           p_get_bsz(pptr));
    printf("-------------------------------------------------------------------\n");
}

void dump() {
    printf("============================== DUMP ===============================\n");

    unsigned free_pgs = 0;
    for (void* pptr = mem_beg; pptr != mem_end; pptr += PAGE_SIZE) {
        if (p_get_fs(pptr)) {
            ++free_pgs;
        } else {
            dump_pg_head(pptr);
        }
    }

    printf("free pages: %lu\n", free_pgs);
    printf("===================================================================\n");
}

//
void occupy_page_with_blocks(void* pptr, unsigned blk_sz) {
    p_set_num(pptr, 0);
    p_set_fs(pptr, OCCUPIED);
    p_set_as(pptr, BLOCKS);
    p_set_bsz(pptr, blk_sz);
}

unsigned not_out_of_page(void* pptr, void* ptr) {
    return (abs(ptr - pptr) < PHEADER_SIZE) ? 1 : 0;
}

blk_t alloc_block(void* pptr) {
    unsigned new_num = p_get_num(pptr) + 1;
    p_set_num(pptr, new_num);
    blk_t retval;
    retval.pptr = pptr;
    retval.shift = new_num;
    return retval;
}

void mem_init(unsigned size) {
    mem_beg = malloc(size);
    mem_end = mem_beg + MEM_SIZE;

    for (void* pptr = mem_beg; pptr != mem_end; pptr += PAGE_SIZE) {
        p_set_fs(pptr, FREE);
    }
}

blk_t alloc_lt_page_size(unsigned blk_sz) {
    void* free_page = NULL;
    void* fits_size_page = NULL;

    for (void* pptr = mem_beg; pptr != mem_end; pptr += PAGE_SIZE) {
        if (p_get_fs(pptr)) {
            if (free_page == NULL) free_page = pptr;
        } else if ((p_get_as(pptr) == BLOCKS) && (p_get_bsz(pptr) == blk_sz)) {
            fits_size_page = pptr;
            break;
        }
    }

    if (fits_size_page == NULL) {
        if (free_page != NULL) {
            occupy_page_with_blocks(free_page, blk_sz);
            return alloc_block(free_page);
        } else {
            printf("lab2: no free space\n");
        }
    } else {
        return alloc_block(fits_size_page);
    }
}

void occupy_pages_with_multiblk(void* pptr_arg, unsigned pages_n) {
    void* pptr = pptr_arg;

    p_set_num(pptr, 0);
    p_set_bsz(pptr, pages_n);
    p_set_as(pptr, MULTIPAGE);
    p_set_fs(pptr, OCCUPIED);

    for (unsigned pg = 1; pg < pages_n; ++pg) {
        pptr = pptr + PAGE_SIZE * pg;

        p_set_num(pptr, pg);
        p_set_bsz(pptr, pages_n);
        p_set_as(pptr, MULTIPAGE);
        p_set_fs(pptr, OCCUPIED);
    }
}

// ыыыыы
blk_t alloc_gt_page_size(unsigned pages_n) {
    unsigned prv_free = 0;
    unsigned free_found = 0;
    void *free_page1 = NULL;

    for (void* pptr = mem_beg; pptr != mem_end; pptr += PAGE_SIZE) {
        if (p_get_fs(pptr)) {
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
        printf("lab2: not enough mem");
    }
}

void* get_blk_ptr(blk_t blk) {
    return blk.pptr + PHEADER_SIZE + blk.shift * p_get_bsz(blk.pptr);
}

blk_t mem_alloc(unsigned size) {
    unsigned aligned_size = ALIGN4(size);
    if (aligned_size <= APAGE_SIZE / 2) {
        return alloc_lt_page_size(aligned_size);
    } else {
        return alloc_gt_page_size((aligned_size / APAGE_SIZE) + 1);
    }
}

void mem_free(blk_t blk) {
     if (p_get_as(blk.pptr) == MULTIPAGE) {
        unsigned num = p_get_num(blk.pptr);
        unsigned tot = p_get_bsz(blk.pptr);
        /// printf("%d, %d\n", num, tot);
        for (void* pptr = blk.pptr; pptr != blk.pptr + tot * PAGE_SIZE; pptr += PAGE_SIZE) {
            // printf("%#14lx\n", pptr);
            p_set_fs(pptr, FREE);
        }
    } else {
         if (p_get_num(blk.pptr) - 1) {
             p_set_num(blk.pptr, p_get_num(blk.pptr) - 1);
         } else {
             p_set_fs(blk.pptr, FREE);
         }
    }
}

void test() {
    mem_init(MEM_SIZE);

    printf("mem_beg: %#14lx\n ", mem_beg);
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
