#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/ioctl.h>

static int
reflink_file(char const *src_name, char const *dst_name,
	     bool preserve_attrs)
{
	int fd;

#ifndef REFLINK_ATTR_NONE
#  define REFLINK_ATTR_NONE 0
#endif
#ifndef REFLINK_ATTR_PRESERVE
#  define REFLINK_ATTR_PRESERVE 1
#endif
#ifndef OCFS2_IOC_REFLINK
	struct reflink_arguments {
		uint64_t old_path;
		uint64_t new_path;
		uint64_t preserve;
	};

#  define OCFS2_IOC_REFLINK _IOW ('o', 4, struct reflink_arguments)
#endif
	struct reflink_arguments args = {
		.old_path = (unsigned long) src_name,
		.new_path = (unsigned long) dst_name,
		.preserve = preserve_attrs ? REFLINK_ATTR_PRESERVE :
					     REFLINK_ATTR_NONE,
	};

	fd = open(src_name, O_RDONLY);
	if (fd < 0) {
		fprintf(stderr, "Failed to open %s: %s\n",
			src_name, strerror(errno));
		return -1;
	}

	if (ioctl(fd, OCFS2_IOC_REFLINK, &args) < 0) {
		fprintf(stderr, "Failed to reflink %s to %s: %s\n",
			src_name, dst_name, strerror(errno));
		return -1;
	}
}

int
main(int argc, char *argv[])
{
	if (argc != 3) {
		fprintf(stdout, "Usage: %s source dest\n", argv[0]);
		return 1;
	}

	return reflink_file(argv[1], argv[2], 0);
}
