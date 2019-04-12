#define _FILE_OFFSET_BITS 64

#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <inttypes.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <unistd.h>
#include <fcntl.h>
#include <signal.h>
#include <errno.h>

#include "mapping.h"

int mm_map(struct mm_mapping *m, const char *fname, size_t size, off_t offset)
{
	char *nm = NULL;
	int fd = -1;
	void *mapping = NULL;
	int result;

	nm = strdup(fname);
	if (nm == NULL) {
		result = -ENOMEM;
		goto err_out1;
	}

	fd = open(fname, O_RDWR | O_SYNC);
	if (fd == -1) {
		fprintf(stderr, "open(%s): %s\n", fname, strerror(errno));
		result = -EACCES;
		goto err_out1;
	}

	mapping = mmap(NULL, size, PROT_READ | PROT_WRITE,
		MAP_SHARED, fd, offset);
	if (mapping == MAP_FAILED) {
		perror("mmap");
		result = -EFAULT;
		goto err_out2;
	}

	m->fname = nm;
	m->fd = fd;
	m->size = size;
	m->offset = offset;
	m->mapping = mapping;

	return 0;

//err_out3:
	munmap(mapping, size);
	mapping = NULL;

err_out2:
	close(fd);
	fd = -1;

err_out1:
	free(nm);

	return result;
}

int mm_unmap(struct mm_mapping *m)
{
	munmap(m->mapping, m->size);
	m->size = 0;
	m->offset = 0;
	m->mapping = NULL;

	close(m->fd);
	m->fd = -1;

	free(m->fname);

	return 0;
}

/**
 * Return is physical address in mapping range or not
 *
 * @param m     mapping info
 * @param paddr physical address
 * @return 0: address is out of range, otherwise: address is inside of range
 */
int mm_is_valid(const struct mm_mapping *m, uint64_t paddr)
{
	return !(paddr < m->offset || (m->offset + m->size) < paddr);
}

/**
 * Convert physical address to virtual address.
 *
 * @param m     mapping info
 * @param paddr physical address
 * @return virtual address
 */
void *mm_phys_to_virt(const struct mm_mapping *m, uint64_t paddr)
{
	if (!mm_is_valid(m, paddr)) {
		fprintf(stderr, "p2v: 0x%08"PRIx64" is out of bounds.\n",
			paddr);
		return 0;
	}

	return (void *)((uint8_t *)m->mapping + (paddr - m->offset));
}

uint64_t mm_readq(const struct mm_mapping *m, uint64_t paddr)
{
	volatile uint64_t *ptr;

	if (!mm_is_valid(m, paddr)) {
		fprintf(stderr, "rq: 0x%08"PRIx64" is out of bounds.\n",
			paddr);
		return 0;
	}

	ptr = mm_phys_to_virt(m, paddr);

	return *ptr;
}

uint32_t mm_readl(const struct mm_mapping *m, uint64_t paddr)
{
	volatile uint32_t *ptr;

	if (!mm_is_valid(m, paddr)) {
		fprintf(stderr, "rl: 0x%08"PRIx64" is out of bounds.\n",
			paddr);
		return 0;
	}

	ptr = mm_phys_to_virt(m, paddr);

	return *ptr;
}

uint16_t mm_readw(const struct mm_mapping *m, uint64_t paddr)
{
	volatile uint16_t *ptr;

	if (!mm_is_valid(m, paddr)) {
		fprintf(stderr, "rw: 0x%08"PRIx64" is out of bounds.\n",
			paddr);
		return 0;
	}

	ptr = mm_phys_to_virt(m, paddr);

	return *ptr;
}

uint8_t mm_readb(const struct mm_mapping *m, uint64_t paddr)
{
	volatile uint8_t *ptr;

	if (!mm_is_valid(m, paddr)) {
		fprintf(stderr, "rb: 0x%08"PRIx64" is out of bounds.\n",
			paddr);
		return 0;
	}

	ptr = mm_phys_to_virt(m, paddr);

	return *ptr;
}

void mm_writeq(const struct mm_mapping *m, uint64_t val, uint64_t paddr)
{
	volatile uint64_t *ptr;

	if (!mm_is_valid(m, paddr)) {
		fprintf(stderr, "wq: 0x%08"PRIx64" is out of bounds.\n",
			paddr);
		return;
	}

	ptr = mm_phys_to_virt(m, paddr);

	*ptr = val;
}

void mm_writel(const struct mm_mapping *m, uint32_t val, uint64_t paddr)
{
	volatile uint32_t *ptr;

	if (!mm_is_valid(m, paddr)) {
		fprintf(stderr, "wl: 0x%08"PRIx64" is out of bounds.\n",
			paddr);
		return;
	}

	ptr = mm_phys_to_virt(m, paddr);

	*ptr = val;
}

void mm_writew(const struct mm_mapping *m, uint16_t val, uint64_t paddr)
{
	volatile uint16_t *ptr;

	if (!mm_is_valid(m, paddr)) {
		fprintf(stderr, "ww: 0x%08"PRIx64" is out of bounds.\n",
			paddr);
		return;
	}

	ptr = mm_phys_to_virt(m, paddr);

	*ptr = val;
}

void mm_writeb(const struct mm_mapping *m, uint8_t val, uint64_t paddr)
{
	volatile uint8_t *ptr;

	if (!mm_is_valid(m, paddr)) {
		fprintf(stderr, "wb: 0x%08"PRIx64" is out of bounds.\n",
			paddr);
		return;
	}

	ptr = mm_phys_to_virt(m, paddr);

	*ptr = val;
}
