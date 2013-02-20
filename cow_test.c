/*
 * Test the new flag FIEMAP_FLAG_COW for fiemap ioctl(2) interface
 *
 * Jie Liu <jeff.liu@oracle.com>
 * gcc -o cow_test cow_test.c -g -Wall
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <stdbool.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <linux/fs.h>
#include <linux/fiemap.h>
#include <errno.h>

#define FIEMAP_FLAG_COW		0x00000004 /* map copy-on-write extents */

static int process_cow_extents(const char *filename)
{
	union {struct fiemap f; char c[4096];} fiemap_buf;
	struct fiemap *fiemap = &fiemap_buf.f;
	struct fiemap_extent *fm_extents = &fiemap->fm_extents[0];
	enum {count = (sizeof(fiemap_buf) - sizeof(*fiemap)) / sizeof(*fm_extents)};
	unsigned long long cow_ext_count = 0;
	bool last = false;
	int fd, ret = 0;

	fd = open(filename, O_RDONLY);
	if (fd < 0) {
		fprintf(stderr, "Failed to open %s: %s\n", filename, strerror(errno));
		return -1;
	}

#ifdef DEBUG
	fprintf(stdout, "count=%d\n", count);
#endif

	memset(&fiemap_buf, 0, sizeof fiemap_buf);
	do {
		int i;
		fiemap->fm_length = ~0ULL;
		fiemap->fm_extent_count = count;
		fiemap->fm_flags = FIEMAP_FLAG_COW;

		if (ioctl(fd, FS_IOC_FIEMAP, fiemap) < 0) {
			fprintf(stderr, "Failed to fetch file mapping as %s\n",
				strerror(errno));
			ret = -1;
			break;
		}

		/* No more extents */
		if (fiemap->fm_mapped_extents == 0) {
			fprintf(stdout, "No more extents mapped, bye\n");
			break;
		}
#ifdef DEBUG
		fprintf(stdout, "Got %u extents\n", fiemap->fm_mapped_extents);
#endif 
		for (i = 0; i < fiemap->fm_mapped_extents; i++) {
#if 0
			uint64_t loffset = fm_extents[i].fe_logical;
			uint64_t len = fm_extents[i].fe_length;
#endif
			/* Skip inline file which its data mixed with metadata */
			if (!(fm_extents[i].fe_flags & FIEMAP_EXTENT_SHARED)) {
				fprintf(stderr, "FATAL: unshared extent returned.\n");
				goto close_file;
			}

			/* Increase the shared extents size per file */
			if (fm_extents[i].fe_flags & FIEMAP_EXTENT_SHARED) {
#ifdef DEBUG
				fprintf(stdout, "Yeah, extent is shared\n");
#endif
				cow_ext_count++;
			}

			if (fm_extents[i].fe_flags & FIEMAP_EXTENT_LAST)
				goto out;
		}

		/*
		 * Do not calculate the next fiemap start offset if only one
		 * extent mapped and it is inline.
		 */
		if (i > 0)
			fiemap->fm_start = fm_extents[i-1].fe_logical +
					   fm_extents[i-1].fe_length;
	} while (!last);

out:
	fprintf(stdout, "Find %llu COW extents\n", cow_ext_count);

close_file:
	if (close (fd) < 0) {
		fprintf(stderr, "Failed to close file %s: %s\n", filename, strerror(errno));
		ret = -1;
	}

	return ret;
}

int main(int argc, char *argv[])
{
	if (argc != 2) {
		fprintf(stdout, "Usage: %s file\n", argv[0]);
		return 1;
	}

	return process_cow_extents(argv[1]);
}
