#ifndef MAPPING_H__
#define MAPPING_H__

#include <stdint.h>
#include <unistd.h>

struct mm_mapping {
	char *fname;
        int fd;
        size_t size;
        off_t offset;
        void *mapping;
};

int mm_map(struct mm_mapping *mr, const char *fname, size_t size, off_t offset);
int mm_unmap(struct mm_mapping *mr);

void *mm_phys_to_virt(const struct mm_mapping *m, uint64_t paddr);

uint64_t mm_readq(const struct mm_mapping *m, uint64_t paddr);
uint32_t mm_readl(const struct mm_mapping *m, uint64_t paddr);
uint16_t mm_readw(const struct mm_mapping *m, uint64_t paddr);
uint8_t mm_readb(const struct mm_mapping *m, uint64_t paddr);

void mm_writeq(const struct mm_mapping *m, uint64_t val, uint64_t paddr);
void mm_writel(const struct mm_mapping *m, uint32_t val, uint64_t paddr);
void mm_writew(const struct mm_mapping *m, uint16_t val, uint64_t paddr);
void mm_writeb(const struct mm_mapping *m, uint8_t val, uint64_t paddr);

#endif //MAPPING_H__
