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

// page masks
//                 |0 0 N N B B A F |
#define PFS_MASK  0x00000000000000FF
//                 |0 0 N N B B A F |
#define PAS_MASK  0x000000000000FF00
//                 |0 0 N N B B A F |
#define PBSZ_MASK 0x00000000FFFF0000
//                 |0 0 N N B B A F |
#define PNUM_MASK 0x0000FFFF00000000

// sizes
#define HEADER_SIZE 8    // 8b
#define MIN_AREA_SIZE 8  // 4b
#define PHEADER_SIZE 8
#define PAGE_SIZE 4000   // 4Kb
#define APAGE_SIZE 3992  // (PAGE_SIZE - PHEADER_SIZE)
#define MEM_SIZE 4000000 // 4Mb = 1000 * 4Kb

#define ALIGN4(x) (((x) & (-1 - 3)) + ((((x) & 3) != 0) ? 4 : 0))

// to 4 byte size_t
#define CAST_SIZE_T(ptr) ((size_t*)(ptr))
#define DEREF(ptr) *(CAST_SIZE_T(ptr))

void* mem_beg;
void* mem_end;

typedef struct {
     void* pptr;
     size_t shift;
} blk_t;

// page free status
void p_set_fs(void* pptr, size_t free) {
    DEREF(pptr) = (DEREF(pptr) & ~PFS_MASK) | (free & PFS_MASK);
}

size_t p_get_fs(void* pptr) {
    return DEREF(pptr) & PFS_MASK;
}

// page alloc status

void p_set_as(void* pptr, size_t alloc) {
    DEREF(pptr) = (DEREF(pptr) & ~PAS_MASK) | ((alloc << 8) & PAS_MASK);
}

size_t p_get_as(void* pptr) {
    return (DEREF(pptr) & PAS_MASK) >> 8;
}

// page block size

void p_set_bsz(void* pptr, size_t blk_sz) {
    *(int*)(pptr + 4) = (*(int*)(pptr + 4) & ~PBSZ_MASK) | ((blk_sz << 16) & PBSZ_MASK);
}

size_t p_get_bsz(void* pptr) {
    return (*(int)(pptr + 4) & PBSZ_MASK);
}

// page blocks num


void p_set_num(void* pptr, size_t num) {
    DEREF(pptr) = (DEREF(pptr) & ~PNUM_MASK) | ((num << 32) & PNUM_MASK);
}

size_t p_get_num(void* pptr) {
    return (DEREF(pptr) & PNUM_MASK) >> 32;
}

// dump

void dump_pg_head(void* pptr) {
    printf("paddr: %#14lx | %6s | %6s | %9s | %8s |\n", pptr, "NUM", "SIZE", "ALLOC", "FREE?");
    printf("       %14s | %06lu | %06lu | %9s | %8s |\n",
           "",
           p_get_num(pptr),
           p_get_bsz(pptr),
           (p_get_as(pptr)) ? "BLOCKS" : "MULTIPAGE",
           (p_get_fs(pptr)) ? "FREE" : "OCCUPIED");
}

void dump() {
    printf("----------------------------- DUMP -----------------------------\n");

    size_t free_pgs = 0;
    for (void* pptr = mem_beg; pptr != mem_end; pptr += PAGE_SIZE) {
        if (p_get_fs(pptr)) {
            ++free_pgs;
        } else {
            dump_pg_head(pptr);
        }
    }

    printf("free pages: %lu\n", free_pgs);
    // printf("================================================================\n");
}

//

void occupy_page_with_blocks(void* pptr, size_t blk_sz) {
    p_set_num(pptr, 0);
    p_set_fs(pptr, OCCUPIED);
    p_set_as(pptr, BLOCKS);
    p_set_bsz(pptr, blk_sz);
}

size_t not_out_of_page(void* pptr, void* ptr) {
    return (abs(ptr - pptr) < PHEADER_SIZE) ? 1 : 0;
}

blk_t alloc_block(void* pptr) {
    size_t new_num = p_get_num(pptr) + 1;
    p_set_num(pptr, new_num);
    blk_t retval;
    retval.pptr = pptr;
    retval.shift = new_num;
    return retval;
}

void mem_init(size_t size) {
    mem_beg = malloc(size);
    mem_end = mem_beg + MEM_SIZE;

    for (void* pptr = mem_beg; pptr != mem_end; pptr += PAGE_SIZE) {
        p_set_fs(pptr, FREE);
    }
}

blk_t alloc_lt_page_size(size_t blk_sz) {
    void* free_page = NULL;
    void* fits_size_page = NULL;

    for (void* pptr = mem_beg; pptr != mem_end; pptr += PAGE_SIZE) {
        // printf("%x -- %u\n", DEREF(pptr), p_get_fs(pptr));
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

void occupy_pages_with_multiblk(void* pptr_arg, size_t pages_n) {
    void* pptr = pptr_arg;

    p_set_num(pptr, 0);
    p_set_bsz(pptr, pages_n);
    p_set_as(pptr, MULTIPAGE);
    p_set_fs(pptr, OCCUPIED);

    for (size_t pg = 1; pg < pages_n; ++pg) {
        pptr = pptr + PAGE_SIZE * pg;
        p_set_num(pptr, pg);
        p_set_bsz(pptr, pages_n);
        p_set_as(pptr, MULTIPAGE);
        p_set_fs(pptr, OCCUPIED);
    }
}

// ыыыыы
blk_t alloc_gt_page_size(size_t pages_n) {
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

blk_t mem_alloc(size_t size) {
    size_t aligned_size = ALIGN4(size);
    size_t size_plus_header = aligned_size + HEADER_SIZE;
    if (aligned_size + HEADER_SIZE <= APAGE_SIZE / 2) {
        return alloc_lt_page_size(aligned_size);
    } else {
        return alloc_gt_page_size((aligned_size / APAGE_SIZE) + 1);
    }
}

int main(int argc, char** argv) {
    mem_init(MEM_SIZE);

    printf("mem_beg: %#14lx; ", mem_beg);
    printf("mem_end: %#14lx\n", mem_end);

    blk_t a1 = mem_alloc(1);
    blk_t a2 = mem_alloc(2);
    blk_t a3 = mem_alloc(3);
    blk_t a4 = mem_alloc(4);

    dump();

    mem_alloc(10);
    mem_alloc(20);
    mem_alloc(30);

    dump();

    mem_alloc(5000);

    dump();

    return 0;
}
