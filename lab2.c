#include <stdio.h>
#include <stdlib.h>

/* Header structure (4 bytes):
 * | X X X | Z |
 *   \___/   \--- STAT (FREE = 1, OCCUPIED = 0)
 *     \--------- SIZE (size of allocated mem)
 */

/* Page descriptor structure (4 bytes):
 * | B B | A | F |
 *   \_/   \   \--- FREE STATUS (FREE = 1, OCCUPIED = 0)
 *    |     \------ ALLOC STATUS (BLOCKS = 1, MULTIPAGE = 0)
 *    \------------ BLOCK SIZE (if A = 1)
 */

// statuses
#define FREE 1
#define OCCUPIED 0
#define BLOCKS 1
#define MULTIPAGE 0

// page masks
#define PFS_MASK 0x000000FF
#define PAS_MASK 0x0000FF00
#define PBSZ_MASK 0xFFFF0000

// block masks
#define BSIZE_MASK 0xFFFFFF00
#define BSTAT_MASK 0x000000FF

// sizes
#define HEADER_SIZE 8    // 8b
#define MIN_AREA_SIZE 8  // 4b
#define PHEADER_SIZE 4
#define PAGE_SIZE 4000   // 4Kb
#define APAGE_SIZE 3996  // (PAGE_SIZE - PHEADER_SIZE)
#define MEM_SIZE 4000000 // 4Mb = 1000 * 4Kb

// to 4 byte unsigned
#define CAST_UNSIG(ptr) ((unsigned*)(ptr))
#define DEREF(ptr) *(CAST_UNSIG(ptr))

void* mem_beg;
void* mem_end;

// page free status
void p_set_fs(void* pptr, unsigned free) {
    DEREF(pptr) = (DEREF(pptr) & ~PFS_MASK) | (free & PFS_MASK);
}

unsigned p_get_fs(void* pptr) {
    return DEREF(pptr) & PFS_MASK;
}

// page alloc status

void p_set_as(void* pptr, unsigned alloc) {
    DEREF(pptr) = (DEREF(pptr) & ~PAS_MASK) | ((alloc << 8) & PAS_MASK);
}

unsigned p_get_as(void* pptr) {
    return (DEREF(pptr) & PAS_MASK) >> 8;
}

// page block size

void p_set_bsz(void* pptr, unsigned blk_sz) {
    DEREF(pptr) = (DEREF(pptr) & ~PBSZ_MASK) | ((blk_sz << 16) & PBSZ_MASK);
}

unsigned p_get_bsz(void* pptr) {
    return (DEREF(pptr) & PBSZ_MASK) >> 16;
}

// block block size

/* void b_set_bsz(void* bptr, unsigned blk_sz) { */
/*     DEREF(bptr) = (DEREF(bptr) & ~BSIZE_MASK) | ((blk_sz << 32) & BSIZE_MASK); */
/* } */

/* unsigned b_get_bsz(void* bptr, unsigned blk_sz) { */
/*     return (DEREF(bptr) & BSIZE_MASK) >> 32; */
/* } */

// block prev size

void b_set_bsz(void* bptr, unsigned prv_sz) {
    DEREF(bptr) = (DEREF(bptr) & ~BSIZE_MASK) | ((prv_sz << 8) & BSIZE_MASK);
}

unsigned b_get_bsz(void* bptr) {
    return (DEREF(bptr) & BSIZE_MASK) >> 8;
}

// block free status

void b_set_fs(void* bptr, unsigned stat) {
    DEREF(pptr) = (DEREF(pptr) & ~BSTAT_MASK) | (free & BSTAT_MASK);
}

unsigned b_get_fs(void *bptr) {
    return DEREF(bptr) & BSTAT_MASK;
}

// dump

void dump_pg_head(void* pptr) {
    printf("| %06u | %9s | %8s |\n", p_get_bsz(pptr), (p_get_as(pptr)) ? "BLOCKS" : "MULTIPAGE", (p_get_fs(pptr)) ? "FREE" : "OCCUPIED");
}

void dump() {
    printf("-------------------- DUMP --------------------\n");

    unsigned free_pgs = 0;
    for (void* pptr = mem_beg; pptr != mem_end; pptr += PAGE_SIZE) {
        if (p_get_fs(pptr)) {
            ++free_pgs;
        } else {
            dump_pg_head(pptr);
        }
    }

    // printf("----------------------------------------------\n");
    printf("free pages: %u\n", free_pgs);
    printf("==============================================\n");
}

//

void occupy_page_with_blocks(void* pptr, unsigned blk_sz) {
    p_set_fs(pptr, OCCUPIED);
    p_set_as(pptr, BLOCKS);
    p_set_bsz(pptr, blk_sz);
}

unsigned not_out_of_page(void* pptr, void* ptr) {
    return (abs(ptr - pptr) < PHEADER_SIZE) ? 1 : 0;
}

unsigned alloc_block(void* pptr) {
    unsigned blk_sz = p_get_bsz(pptr);
    void* free_blk == NULL;
    unsigned prv_sz = 0;

    for (void* bptr = pptr + PHEADER_SIZE; not_out_of_page(pptr, bptr); bptr += HEADER_SIZE + blk_sz) {
        if (b_get_fs(bptr)) { // free
            free_blk = bptr;
            break;
        } else {
            
        }
    }

    if (free_blk == NULL) {
        return 0;
    } else {
        b_set_fs(OCCUPIED);
        b_set_prv
    }
}

void mem_init(unsigned size) {
    mem_beg = malloc(size);
    mem_end = mem_beg + MEM_SIZE;

    for (void* pptr = mem_beg; pptr != mem_end; pptr += PAGE_SIZE) {
        p_set_fs(pptr, FREE);
    }
}

void* alloc_lt_page_size(unsigned blk_sz) {
    void* free_page = NULL;
    void* fits_size_page = NULL;
    for (void* pptr = mem_beg; pptr != mem_end; pptr += PAGE_SIZE) {
        // printf("%x -- %u\n", DEREF(pptr), p_get_fs(pptr));
        if (p_get_fs(pptr)) {
            free_page = pptr;
        } else if ((p_get_as(pptr) == BLOCKS) && (p_get_bsz(pptr) == blk_sz)) {

            fits_size_page = pptr;
            break;
        }
    }

    if (fits_size_page == NULL) {
        if (free_page != NULL) {
            occupy_page_with_blocks(free_page, blk_sz);
        } else {
            printf("lab2: no free space\n");
        }
    } else {
        
    }

}

void* mem_alloc(unsigned size) {
    unsigned size_plus_header = size + HEADER_SIZE;
    // printf("%u - %u\n", size, size_plus_header);
    if (size + HEADER_SIZE <= APAGE_SIZE / 2) {
        alloc_lt_page_size(size);
    } else {

    }
}

int main(int argc, char** argv) {
    mem_init(MEM_SIZE);

    mem_alloc(3);
    dump();

    return 0;
}
