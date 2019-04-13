#define _FILE_OFFSET_BITS 64

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdarg.h>
#include <inttypes.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/ioctl.h>
#include <unistd.h>
#include <fcntl.h>
#include <signal.h>
#include <errno.h>

#include "mapping.h"

#define PAGE_SIZE    sysconf(_SC_PAGESIZE)
#define PAGE_MASK    (~(PAGE_SIZE - 1))

#define DEFAULT_DUMP_SIZE    256

#define APPNAME    "ma"
#define DPRINTF(fmt, args ...)    dbg_printf(APPNAME ": " fmt, ##args)

int g_debug = 0;

struct app_option {
	const char *fname;
	int is_read;
	size_t size_unit;
};

__attribute__ ((format (printf, 1, 2)))
int dbg_printf(const char *format, ...)
{
	va_list ap;
	int res;

	if (g_debug == 0)
		return 0;

	va_start(ap, format);
	res = vprintf(format, ap);
	va_end(ap);

	return res;
}

void usage(int argc, char *argv[])
{
	printf("usage: \n"
		"  %s [options] dump_command address [size]\n"
		"  %s [options] edit_command address value\n"
		"  %s [options] edit_command address size value_list\n"
		"  options\n"
		"    -k    use filename instead of /dev/mem\n"
		"    -d    show debug message\n"
		"    -h    show this help\n"
		"\n"
		"  dump_command\n"
		"    db, dw, dd, dq: b: byte(8bits), w: word(16bits), \n"
		"                    d, l: double(32bits), q: quad(64bits).\n"
		"  edit_command\n"
		"    eb, ew, ed, eq: b: byte(8bits), w: word(16bits), \n"
		"                    d, l: double(32bits), q: quad(64bits).\n"
		"  address\n"
		"    Physical address in Hex.\n"
		"  size\n"
		"    Bytes of dump/edit.\n"
		"    0xXX: Hex, 0XX: Oct, XX: Dec.\n"
		"  value_list\n"
		"    Data list to write.\n"
		"    0xXX: Hex, 0XX: Oct, XX: Dec.\n",
		argv[0], argv[0], argv[0]);
}

int main(int argc, char *argv[])
{
	struct app_option o;
	int ind;
	const char *cmd;
	char *endchar;
	off_t addr;
	size_t size;
	uint64_t *list_val;
	int cnt_list;

	struct mm_mapping m;
	int opt, list_start;
	size_t size_map;
	off_t addr_align_st, addr_align_ed, start;
	int result;
	off_t i, j;

	//get arguments
	o.fname = "/dev/mem";
	while ((opt = getopt(argc, argv, "k:dh")) != -1) {
		switch (opt) {
		case 'k':
			o.fname = optarg;
			break;
		case 'd':
			g_debug = 1;
			break;
		case 'h':
			usage(argc, argv);
			return 0;
		}
	}
	ind = optind;

	if (argc - ind < 2) {
		usage(argc, argv);
		result = -EINVAL;
		goto err_out1;
	}

	//remain
	cmd = argv[ind];
	if (strncasecmp(cmd, "db", 2) == 0) {
		o.is_read = 1;
		o.size_unit = 1;
	} else if (strncasecmp(cmd, "dw", 2) == 0) {
		o.is_read = 1;
		o.size_unit = 2;
	} else if (strncasecmp(cmd, "dd", 2) == 0 ||
		   strncasecmp(cmd, "dl", 2) == 0) {
		o.is_read = 1;
		o.size_unit = 4;
	} else if (strncasecmp(cmd, "dq", 2) == 0) {
		o.is_read = 1;
		o.size_unit = 8;
	} else if (strncasecmp(cmd, "eb", 2) == 0) {
		o.is_read = 0;
		o.size_unit = 1;
	} else if (strncasecmp(cmd, "ew", 2) == 0) {
		o.is_read = 0;
		o.size_unit = 2;
	} else if (strncasecmp(cmd, "ed", 2) == 0 ||
		   strncasecmp(cmd, "el", 2) == 0) {
		o.is_read = 0;
		o.size_unit = 4;
	} else if (strncasecmp(cmd, "eq", 2) == 0) {
		o.is_read = 0;
		o.size_unit = 8;
	} else {
		//unknown command
		fprintf(stderr, "unknown command '%s'.\n", cmd);
		result = -EINVAL;
		goto err_out1;
	}

	addr = strtoull(argv[ind + 1], &endchar, 16);
	if (*endchar != '\0') {
		fprintf(stderr, "invalid address '%s'.\n", argv[ind + 1]);
		result = -EINVAL;
		goto err_out1;
	}

	if (o.is_read) {
		if (argc - ind < 3) {
			size = DEFAULT_DUMP_SIZE;
		} else {
			size = strtol(argv[ind + 2], NULL, 0);
		}

		list_start = 0;
		cnt_list = 0;
	} else {
		if (argc - ind < 3) {
			usage(argc, argv);
			result = -EINVAL;
			goto err_out1;
		}
		if (argc - ind < 4) {
			size = o.size_unit;
			list_start = ind + 2;
		} else {
			size = strtol(argv[ind + 2], NULL, 0);
			list_start = ind + 3;
		}

		cnt_list = argc - list_start;
	}
	DPRINTF("cmd:%s, addr:0x%08llx, size:%d, "
		"size_unit:%d list_start:%d, cnt:%d\n",
		cmd, (unsigned long long)addr, (int)size,
		(int)o.size_unit, list_start, cnt_list);

	//alloc value_list
	list_val = calloc(cnt_list, sizeof(uint64_t));
	if (list_val == NULL) {
		perror("calloc(list)");
		result = -EINVAL;
		goto err_out1;
	}
	for (i = 0; i < cnt_list; i++) {
		list_val[i] = strtoll(argv[list_start + i], NULL, 0);
	}

	//print 'value_list' arguments
	DPRINTF("list:%d\n  ", cnt_list);
	for (i = 0; i < cnt_list; i++) {
		DPRINTF("%08"PRIx64" ", list_val[i]);
	}
	DPRINTF("\n");

	//check arguments
	if (addr % o.size_unit != 0) {
		fprintf(stderr, "address 0x%08llx is not aligned of %d.\n",
			(unsigned long long)addr, (int)o.size_unit);
		result = -EINVAL;
		goto err_out2;
	}
	if (size % o.size_unit != 0 || size == 0) {
		fprintf(stderr, "size %d is not multiples of %d.\n",
			(int)size, (int)o.size_unit);
		result = -EINVAL;
		goto err_out2;
	}

	//mmap device
	addr_align_st = addr & PAGE_MASK;
	if (((addr + size) & ~PAGE_MASK) == 0)
		addr_align_ed = (addr + size) & PAGE_MASK;
	else
		addr_align_ed = (addr + size + PAGE_SIZE) & PAGE_MASK;
	size_map = addr_align_ed - addr_align_st;
	if (sizeof(size_map) == 4 && addr_align_st > 0xf0000000) {
		size_map = 0 - addr_align_st - 4;
	}

	DPRINTF("map:0x%08llx - 0x%08llx, file:%s\n",
		(unsigned long long)addr_align_st,
		(unsigned long long)addr_align_ed,
		o.fname);
	result = mm_map(&m, o.fname, size_map, addr_align_st);
	if (result != 0) {
		result = -EACCES;
		goto err_out2;
	}

	DPRINTF("addr:0x%08llx, addr+size:0x%08llx\n",
		(unsigned long long)addr,
		(unsigned long long)addr + size);
	start = addr & ~0xfULL;
	if (o.is_read) {
		i = start;

		printf("%08llx  ", (unsigned long long)i);
		while (i < addr) {
			switch (o.size_unit) {
			case 8:
				printf("---------------- ");
				break;
			case 4:
				printf("-------- ");
				break;
			case 2:
				printf("---- ");
				break;
			case 1:
				printf("-- ");
				break;
			default:
				result = -EINVAL;
				goto err_out3;
			}

			i += o.size_unit;

			if ((unsigned)i % 16 == 8) {
				printf(" ");
			}
		}
		while (i < addr + size) {
			switch (o.size_unit) {
			case 8:
				printf("%016"PRIx64" ", mm_readq(&m, i));
				break;
			case 4:
				printf("%08"PRIx32" ", mm_readl(&m, i));
				break;
			case 2:
				printf("%04x ", mm_readw(&m, i));
				break;
			case 1:
				printf("%02x ", mm_readb(&m, i));
				break;
			default:
				result = -EINVAL;
				goto err_out3;
			}

			i += o.size_unit;

			if ((unsigned)i % 16 == 0) {
				printf("\n%08llx  ", (unsigned long long)i);
			}
			if ((unsigned)i % 16 == 8) {
				printf(" ");
			}
		}
		printf("\n");
	} else {
		j = 0;
		for (i = addr; i < addr + size; i += o.size_unit, j++) {
			switch (o.size_unit) {
			case 8:
				mm_writeq(&m,
					list_val[j % cnt_list], i);
				break;
			case 4:
				mm_writel(&m,
					list_val[j % cnt_list], i);
				break;
			case 2:
				mm_writew(&m,
					list_val[j % cnt_list], i);
				break;
			case 1:
				mm_writeb(&m,
					list_val[j % cnt_list], i);
				break;
			default:
				result = -EINVAL;
				goto err_out3;
			}
		}
	}

	mm_unmap(&m);

	free(list_val);
	list_val = NULL;
	cnt_list = 0;

	return 0;

err_out3:
	mm_unmap(&m);

err_out2:
	free(list_val);
	list_val = NULL;
	cnt_list = 0;

err_out1:
	return result;
}

