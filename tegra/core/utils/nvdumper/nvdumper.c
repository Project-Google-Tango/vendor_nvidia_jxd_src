/*
 * Copyright (c) 2011 NVIDIA Corporation.  All Rights Reserved.
 *
 * NVIDIA Corporation and its licensors retain all intellectual property and
 * proprietary rights in and to this software and related documentation.  Any
 * use, reproduction, disclosure or distribution of this software and related
 * documentation without an express license agreement from NVIDIA Corporation
 * is strictly prohibited.
 */

/**
 * nvdumper
 *
 * A tool to read an NVIDIA RAM image dump from a partition or file.
 *
 * @author		Colin Patrick McCabe <cmccabe@nvidia.com>
 */

#include <arpa/inet.h>
#include <errno.h>
#include <fcntl.h>
#include <inttypes.h>
#include <limits.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

#if (defined(__APPLE__))
/** OS X */
#include <libkern/OSByteOrder.h>
#include <machine/endian.h>
#define htobe32 OSSwapHostToBigInt32
#define htobe64 OSSwapHostToBigInt64
#define be32toh OSSwapBigToHostInt32
#define be64toh OSSwapBigToHostInt64
#define htole32 OSSwapHostToLittleInt32
#define htole64 OSSwapHostToLittleInt64
#define le32toh OSSwapLittleToHostInt32
#define le64toh OSSwapLittleToHostInt64
#define O_DIRECT 0
#define O_NOATIME 0
#define MAP_ANONYMOUS MAP_ANON

#elif (defined(_GNU_SOURCE))
/** Linux */
#include <endian.h>

#else
/** Android */
#include <byteswap.h>
#include <endian.h>
#if _BYTE_ORDER == _LITTLE_ENDIAN
#define htobe32 __swap32
#define htobe64 __swap64
#define be32toh __swap32
#define be64toh __swap64
#define htole32(x) (x)
#define htole64(x) (x)
#define le32toh(x) (x)
#define le64toh(x) (x)
#elif _BYTE_ORDER == BIG_ENDIAN
#define htobe32(x) (x)
#define htobe64(x) (x)
#define be32toh(x) (x)
#define be64toh(x) (x)
#define htole32(x) __swap32
#define htole64(x) __swap64
#define le32toh(x) __swap32
#define le64toh(x) __swap64
#else
#error "unknown endianness"
#endif
#endif

/**************************** constants ***********************************/
#define NVDUMP_PART_MAGIC_LEN 4
#define RDP_BUFFER_LEN 4194304
#define NVDUMP_CUR_VER 2
static const char NVDUMP_PART_MAGIC[NVDUMP_PART_MAGIC_LEN] = "DumP";
static const char * const DEFAULT_DUMP_NAME =
	"/dev/block/platform/sdhci-tegra.3/by-name/DUM";

/**************************** types ***********************************/
enum nvdumper_act {
	NVDUMPER_ACT_READ,
	NVDUMPER_ACT_VERIFY,
	NVDUMPER_ACT_WRITE,
};

struct nvdumper_opts {
	const char *d_fname;
	int force;
	int verbose;
};

struct __attribute__((__packed__)) nvdump_part_hdr {
	unsigned char magic[NVDUMP_PART_MAGIC_LEN];
	uint32_t version;
	uint64_t dump_len;
};

struct __attribute__((__packed__)) nvdump_part_ftr {
	unsigned char magic[NVDUMP_PART_MAGIC_LEN];
	uint32_t csum;
};

typedef int (*hdr_cb_t)(const struct nvdump_part_hdr *hdr,
			const struct nvdumper_opts *opts);
typedef int (*read_cb_t)(const uint32_t *data, size_t len,
			const struct nvdumper_opts *opts);
typedef int (*ftr_cb_t)(const struct nvdump_part_ftr *ftr,
			const struct nvdumper_opts *opts);

/**************************** functions ***********************************/
static void print_hdr_info(const struct nvdump_part_hdr *hdr, FILE *out)
{
	char magic[NVDUMP_PART_MAGIC_LEN + 1] =  { 0 };

	memcpy(magic, hdr->magic, NVDUMP_PART_MAGIC_LEN);
	fprintf(out, "{\n");
	fprintf(out, "\t\"magic\" : \"%4s\",\n", magic);
	fprintf(out, "\t\"version\" : %d,\n", be32toh(hdr->version));
	fprintf(out, "\t\"dump_len\" : %" PRId64"\n", be64toh(hdr->dump_len));
	fprintf(out, "}\n");
}

static void print_ftr_info(const struct nvdump_part_ftr *ftr, FILE *out)
{
	char magic[NVDUMP_PART_MAGIC_LEN + 1] =  { 0 };

	memcpy(magic, ftr->magic, NVDUMP_PART_MAGIC_LEN);
	fprintf(out, "{\n");
	fprintf(out, "\t\"magic\" : \"%4s\",\n", magic);
	fprintf(out, "\t\"csum\" : 0x%" PRIx32"\n", be32toh(ftr->csum));
	fprintf(out, "}\n");
}

static int dump_bytes_to_stdout_cb(const uint32_t *data, size_t len,
			const struct nvdumper_opts *opts)
{
	size_t res;

	if (len == 0)
		return 0;
	res = fwrite(data, 1, len, stdout);
	if (res != len)
		return -EIO;
	return 0;
}

static uint32_t g_csum;
static int g_validated_hdr;

static int validate_hdr_cb(const struct nvdump_part_hdr *hdr,
			const struct nvdumper_opts *opts)
{
	uint32_t version;

	if (g_validated_hdr)
		return 0;
	g_validated_hdr = 1;
	if (opts->verbose) {
		fprintf(stderr, "\"nvdump header\" : ");
		print_hdr_info(hdr, stderr);
	}
	if (memcmp(hdr->magic, NVDUMP_PART_MAGIC,
			NVDUMP_PART_MAGIC_LEN)) {
		fprintf(stderr, "bad header magic value.  "
			"Got 0x%02x%02x%02x%02x; "
			"expected 0x%02x%02x%02x%02x\n",
			hdr->magic[0], hdr->magic[1], hdr->magic[2], hdr->magic[3],
			NVDUMP_PART_MAGIC[0], NVDUMP_PART_MAGIC[1],
			NVDUMP_PART_MAGIC[2], NVDUMP_PART_MAGIC[3]);
		return -EINVAL;
	}
	version = be32toh(hdr->version);
	if (version != NVDUMP_CUR_VER) {
		fprintf(stderr, "unknown dump version %d\n", version);
		return -EINVAL;
	}
	return 0;
}

static int validate_data_cb(const uint32_t *data, size_t len,
			const struct nvdumper_opts *opts)
{
	size_t i;

	for (i = 0; i < len / 4; ++i) {
		g_csum += htole32(data[i]);
	}
	return 0;
}

static int g_validated_ftr;

static int validate_ftr_cb(const struct nvdump_part_ftr *ftr,
			const struct nvdumper_opts *opts)
{
	uint32_t target_csum;

	if (g_validated_ftr)
		return 0;
	g_validated_ftr = 1;
	if (opts->verbose) {
		fprintf(stderr, "\"nvdump footer\" : ");
		print_ftr_info(ftr, stderr);
	}
	if (memcmp(ftr->magic, NVDUMP_PART_MAGIC,
			NVDUMP_PART_MAGIC_LEN)) {
		fprintf(stderr, "bad footer magic value.  "
			"Got 0x%02x%02x%02x%02x; "
			"expected 0x%02x%02x%02x%02x\n",
			ftr->magic[0], ftr->magic[1], ftr->magic[2], ftr->magic[3],
			NVDUMP_PART_MAGIC[0], NVDUMP_PART_MAGIC[1],
			NVDUMP_PART_MAGIC[2], NVDUMP_PART_MAGIC[3]);
		if (!opts->force)
			return -EINVAL;
	}
	target_csum = be32toh(ftr->csum);
	if (g_csum != target_csum) {
		fprintf(stderr, "footer checksum is 0x%08x, but "
			"computed checksum is 0x%08x\n",
			target_csum, g_csum);
		if (!opts->force)
			return -EINVAL;
	}
	return 0;
}

static void *do_get_pages(size_t amt, size_t *buf_len)
{
	long page_size;
	void *v;

	/* Figure out how many pages to allocate */
	page_size = sysconf(_SC_PAGESIZE);
	amt = (amt + page_size - 1) / page_size;
	amt *= page_size;
	*buf_len = amt;
	v = mmap(NULL, amt, PROT_READ | PROT_WRITE,
		MAP_PRIVATE | MAP_ANONYMOUS, 0, 0);
	if (v == MAP_FAILED)
		return NULL;
	return v;
}

static int scan_dump(const char *fname,
		hdr_cb_t hdr_cb, read_cb_t read_cb,
		ftr_cb_t ftr_cb, const struct nvdumper_opts *opts)
{
	int fd, ret, res, dlen, first, done;
	uint64_t cur_offset, total_len = 0, ftr_off = 0;
	uint32_t *buf, *b;
	struct nvdump_part_ftr ftr;
	size_t buf_len;

	memset(&ftr, 0, sizeof(ftr));
	if (RDP_BUFFER_LEN < sizeof(struct nvdump_part_hdr)) {
		fprintf(stderr, "logic error: RDP_BUFFER_LEN must be greater "
			"to or equal to the size of the partition header.\n");
		return -ENOSYS;
	}
	buf = do_get_pages(RDP_BUFFER_LEN, &buf_len);
	if (!buf) {
		ret = -ENOMEM;
		fprintf(stderr, "scan_dump(%s): OOM\n", fname);
		goto done;
	}
	fd = open(fname, O_RDONLY | O_DIRECT | O_NOATIME);
	if (fd < 0) {
		ret = -errno;
		fprintf(stderr, "scan_dump(%s): failed to open "
			"file: error %d\n", fname, ret);
		goto free_buf;
	}
	first = 1;
	cur_offset = 0;
	done = 0;
	do {
		res = read(fd, buf, RDP_BUFFER_LEN);
		b = buf;
		if (res < 0) {
			ret = -errno;
			if (ret == -EINTR)
				continue;
			else {
				fprintf(stderr, "scan_dump(%s): read(2) "
					"error: %d\n", fname, ret);
				goto close_fd;
			}
		}
		else if (res == 0) {
			fprintf(stderr, "scan_dump(%s): dump file is shorter "
				"than the length given in the header.  The "
				"header said the dump was %"PRId64 " bytes, "
				"but it's actually only %"PRId64".\n",
				fname, total_len, cur_offset);
			if (!opts->force) {
				ret = -EINVAL;
				goto close_fd;
			}
			done = 1;
		}
		if (first) {
			struct nvdump_part_hdr hdr;

			if (res < (int)sizeof(struct nvdump_part_hdr)) {
				fprintf(stderr, "scan_dump(%s): the dump file "
					"is too short for a header!\n", fname);
				ret = -EIO;
				goto close_fd;
			}
			memcpy(&hdr, b, sizeof(struct nvdump_part_hdr));
			ftr_off = be64toh(hdr.dump_len);
			total_len = ftr_off + sizeof(struct nvdump_part_ftr);
			if (total_len % 4 != 0) {
				fprintf(stderr, "can't handle memory dumps "
					"whose length is not a multiple "
					"of 4\n");
				ret = -EINVAL;
				goto close_fd;
			}
			b += (sizeof(struct nvdump_part_hdr) / 4);
			res -= sizeof(struct nvdump_part_hdr);
			if (hdr_cb) {
				ret = hdr_cb(&hdr, opts);
				if (ret)
					goto close_fd;
			}
			first = 0;
		}
		if (cur_offset + res >= total_len) {
			/*                              =+=
			 *                               |
			 * =+= cur_offset                |
			 *  |                            |
			 *  |                           =+= total_len
			 *  |
			 * =+= cur_offset + res
			 */
			res = (int)(total_len - cur_offset);
			done = 1;
		}
		if (cur_offset >= ftr_off) {
			/*                              =+=
			 *                               |
			 * =+= cur_offset                |  FOOTER DATA
			 *  |                            |
			 * =+= cur_offset + res          |
			 *                               |
			 *                              =+= total_len
			 */
			int fi = (int)(cur_offset - ftr_off);
			memcpy(((char*)&ftr) + fi, b, res);
			dlen = 0;
		}
		else if (cur_offset + res > ftr_off) {
			/* =+= cur_offset                |
			 *  |                            |  BODY DATA
			 *  |                            |
			 *  |                           =+= ftr_off
			 *  |                            |
			 *  |                            |
			 * =+= cur_offset + res          |  FOOTER DATA
			 *                               |
			 *                               |
			 *                              =+= total_len
			 */
			int fi = (int)(ftr_off - cur_offset);
			int fo = (int)(cur_offset - ftr_off) + res;
			memcpy((char*)&ftr, ((char*)b) + fi, fo);
			dlen = fi;
		}
		else
			dlen = res;
		if (dlen > 0) {
			ret = read_cb(b, dlen, opts);
			if (ret)
				goto close_fd;
		}
		cur_offset += res;
	} while (!done);
	if (ftr_cb) {
		ret = ftr_cb(&ftr, opts);
		if (ret)
			goto close_fd;
	}
	ret = 0;
close_fd:
	while(close(fd) < 0) {
		if (errno != EINTR)
			break;
	}
free_buf:
	munmap(buf, buf_len);
done:
	return ret;
}

static int nvdumper_act_read(const struct nvdumper_opts *opts)
{
	int ret;

	if (!opts->force) {
		/* If we are not using -f (force), then we need to validate
		 * that the data is OK before outputting it. */
		ret = scan_dump(opts->d_fname, validate_hdr_cb,
			validate_data_cb, validate_ftr_cb, opts);
		if (ret)
			return ret;
	}
	ret = scan_dump(opts->d_fname, validate_hdr_cb,
		dump_bytes_to_stdout_cb, validate_ftr_cb, opts);
	return ret;
}

static int nvdumper_act_verify(const struct nvdumper_opts *opts)
{
	int ret;

	ret = scan_dump(opts->d_fname, validate_hdr_cb,
		validate_data_cb, validate_ftr_cb, opts);
	return ret;
}

static int nvdumper_act_write(const struct nvdumper_opts *opts)
{
	unsigned char *buf = NULL;
	int shift, ret;
	FILE *fp = NULL;
	size_t size, wsize, i;
	struct nvdump_part_hdr hdr;
	struct nvdump_part_ftr ftr;
	uint32_t csum;
	uint64_t dump_len;

	buf = malloc(RDP_BUFFER_LEN);
	if (!buf) {
		ret = -ENOMEM;
		fprintf(stderr, "write_dump: OOM\n");
		goto done;
	}
	fp = fopen(opts->d_fname, "w");
	if (!fp) {
		ret = -errno;
		fprintf(stderr, "write_dump: failed to open %s for write: "
			"error %d\n", opts->d_fname, ret);
		goto done;
	}
	/* write the header */
	memset(&hdr, 0, sizeof(hdr));
	wsize = fwrite(&hdr, 1, sizeof(struct nvdump_part_hdr), fp);
	if (wsize != sizeof(struct nvdump_part_hdr)) {
		ret = -errno;
		fprintf(stderr, "write_dump: failed to write zeros to the "
			"header. error %d\n", ret);
		goto done;
	}
	/* write the body */
	shift = 0;
	csum = 0;
	dump_len = 0;
	while (1) {
		size = fread(buf, 1, RDP_BUFFER_LEN, stdin);
		if (size == 0) {
			break;
		}
		wsize = fwrite(buf, 1, size, fp);
		if (size != wsize) {
			ret = -errno;
			fprintf(stderr, "write_dump: error writing %s: "
				"error %d\n", opts->d_fname, ret);
			goto done;
		}
		dump_len += wsize;
		for (i = 0; i < size; ++i) {
			csum += (buf[i] << shift);
			shift += 8;
			if (shift == 32)
				shift = 0;
		}
	}
	if (dump_len % 4) {
		/* Pad the dump length out to a multiple of 4.  Note: the
		 * checksum doesn't need to be updated because we write zeros
		 */
		char slack[3];
		size_t len = 4 - (dump_len % 4);

		memset(slack, 0, sizeof(slack));
		wsize = fwrite(slack, 1, len, fp);
		if (wsize != len) {
			ret = -errno;
			fprintf(stderr, "write_dump(%s): error writing slack: "
				"error %d\n", opts->d_fname, ret);
			goto done;
		}
		dump_len += len;
	}
	if (ferror(fp)) {
		ret = -errno;
		fprintf(stderr, "write_dump: error writing %s: error %d\n",
			opts->d_fname, ret);
		goto done;
	}
	/* write the footer */
	memset(&ftr, 0, sizeof(ftr));
	memcpy(ftr.magic, NVDUMP_PART_MAGIC,
		NVDUMP_PART_MAGIC_LEN);
	ftr.csum = htobe32(csum);
	if (opts->verbose) {
		fprintf(stderr, "\"created nvdump footer\" : ");
		print_ftr_info(&ftr, stderr);
	}
	wsize = fwrite(&ftr, 1, sizeof(struct nvdump_part_ftr), fp);
	if (wsize != sizeof(struct nvdump_part_ftr)) {
		ret = -errno;
		fprintf(stderr, "write_dump: failed to write the footer. "
			"error %d\n", ret);
		goto done;
	}
	/* rewrite the header */
	rewind(fp);
	memset(&hdr, 0, sizeof(hdr));
	memcpy(hdr.magic, NVDUMP_PART_MAGIC,
		NVDUMP_PART_MAGIC_LEN);
	hdr.version = htobe32(NVDUMP_CUR_VER);
	hdr.dump_len = htobe64(dump_len);
	if (opts->verbose) {
		fprintf(stderr, "\"created nvdump header\" : ");
		print_hdr_info(&hdr, stderr);
	}
	wsize = fwrite(&hdr, 1, sizeof(struct nvdump_part_hdr), fp);
	if (wsize != sizeof(struct nvdump_part_hdr)) {
		ret = -errno;
		fprintf(stderr, "write_dump: failed to write the header. "
			"error %d\n", ret);
		goto done;
	}
	fflush(fp);
	ret = fsync(fileno(fp));
	if (ret) {
		ret = -errno;
		fprintf(stderr, "write_dump: fsync error writing %s: error %d\n",
			opts->d_fname, ret);
		goto done;
	}
	ret = 0;
done:
	free(buf);
	if ((fp != NULL) && (fclose(fp))) {
		ret = -errno;
		fprintf(stderr, "write_dump: fclose error writing %s: "
			"error %d\n", opts->d_fname, ret);
	}
	return 0;
}

static void usage(const char *argv0, int status)
{
	fprintf(stderr, "\
%s: a tool to read an NVIDIA RAM image dump from a partition or file.\n\
\n\
USAGE:\n\
%s [options] <dump-file-name>\n\
The dump file defaults to '%s' if none is given.\n\
\n\
OPTIONS:\n\
-a [read|write|verify]  Specify an action.  The default is 'read'.\n\
                        read: reads the dump to stdout.\n\
                        verify: verify that the length and CRC are correct, \n\
			        but do not dump\n\
                        write: overwrite the current dump file with data \n\
                                supplied from stdin\n\
-f:                     Continue even if the dump checksum is bad or the file is \n\
                        too short\n\
-h:                     This help message\n\
-v:                     Be verbose\n\
", argv0, argv0, DEFAULT_DUMP_NAME);
	exit(status);
}

static void parse_argv(int argc, char **argv, struct nvdumper_opts *opts,
		       enum nvdumper_act *act)
{
	int c;

	*act = NVDUMPER_ACT_READ;
	memset(opts, 0, sizeof(struct nvdumper_opts));
	opts->d_fname = DEFAULT_DUMP_NAME;
	opts->force = 0;
	opts->verbose = 0;
	while ((c = getopt(argc, argv, "a:fhv")) != -1) {
		switch (c) {
		case 'a':
			if (!strcmp(optarg, "read"))
				*act = NVDUMPER_ACT_READ;
			else if (!strcmp(optarg, "verify"))
				*act = NVDUMPER_ACT_VERIFY;
			else if (!strcmp(optarg, "write"))
				*act = NVDUMPER_ACT_WRITE;
			else {
				fprintf(stderr, "error: action must be one of "
					"read, verify, write.  -h for help.\n");
				exit(EXIT_FAILURE);
			}
			break;
		case 'f':
			opts->force = 1;
			break;
		case 'h':
			usage(argv[0], EXIT_SUCCESS);
			break;
		case 'v':
			opts->verbose = 1;
			break;
		default:
			usage(argv[0], EXIT_FAILURE);
			break;
		}
	}
	if (optind == argc - 1) {
		opts->d_fname = argv[optind];
	}
	else if (optind != argc) {
		fprintf(stderr, "Junk at end of command line.  -h for help.\n");
		exit(EXIT_FAILURE);
	}
}

int main(int argc, char **argv)
{
	int ret;
	struct nvdumper_opts opts;
	enum nvdumper_act act;

	parse_argv(argc, argv, &opts, &act);
	switch (act) {
	case NVDUMPER_ACT_READ:
		return nvdumper_act_read(&opts);
	case NVDUMPER_ACT_VERIFY:
		return nvdumper_act_verify(&opts);
	case NVDUMPER_ACT_WRITE:
		return nvdumper_act_write(&opts);
	default:
		return EXIT_FAILURE;
	}
}
