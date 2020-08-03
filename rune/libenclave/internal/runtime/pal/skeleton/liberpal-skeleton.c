// SPDX-License-Identifier: (GPL-2.0 OR BSD-3-Clause)
// Copyright(c) 2016-18 Intel Corporation.

#include <elf.h>
#include <errno.h>
#include <fcntl.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/wait.h>
#include "defines.h"
#include "sgx_call.h"

#define PAGE_SIZE  4096

#define SGX_REG_PAGE_FLAGS \
	(SGX_SECINFO_REG | SGX_SECINFO_R | SGX_SECINFO_W | SGX_SECINFO_X)

#define IMAGE		"encl.bin"
#define SIGSTRUCT	"encl.ss"
#define TOKEN		"encl.token"

static struct sgx_secs secs;
static bool initialized = false;
static char *sgx_dev_path;
static bool is_oot_driver;
static bool no_sgx_flc = false;
static bool fork_test = false;
/*
 * For SGX in-tree driver, dev_fd cannot be closed until an enclave instance
 * intends to exit.
 */
static int enclave_fd = -1;
void *tcs_busy;

static bool is_sgx_device(const char *dev)
{
	struct stat st;
	int rc;

	rc = stat(dev, &st);
	if (!rc) {
		if ((st.st_mode & S_IFCHR) && (major(st.st_rdev) == 10))
			return true;
	}

	return false;
}

static void detect_driver_type(void)
{
	if (is_sgx_device("/dev/isgx")) {
		sgx_dev_path = "/dev/isgx";
		is_oot_driver = true;
		return;
	}

	sgx_dev_path = "/dev/sgx/enclave";
	is_oot_driver = false;
}

static uint64_t create_enclave_range(int dev_fd, uint64_t size)
{
	void *area;
	int fd;
	int flags = MAP_SHARED;

	if (is_oot_driver) {
		fd = dev_fd;
	} else {
		fd = -1;
		flags |= MAP_ANONYMOUS;
	}

	area = mmap(NULL, size * 2, PROT_NONE, flags, fd, 0);
	if (area == MAP_FAILED) {
		perror("mmap");
		return 0;
	}

	uint64_t base = ((uint64_t)area + size - 1) & ~(size - 1);
	munmap(area, base - (uint64_t)area);
	munmap((void *)(base + size), (uint64_t)area + size - base);

	if (is_oot_driver) {
		if (mprotect((void *)base, size, PROT_READ | PROT_WRITE | PROT_EXEC)) {
			perror("mprotect");
			munmap((void *)base, size);
			return 0;
		}
	}

	return base;
}

static bool encl_create(int dev_fd, unsigned long bin_size,
			struct sgx_secs *secs)
{
	struct sgx_enclave_create ioc;
	int rc;

	memset(secs, 0, sizeof(*secs));
	secs->ssa_frame_size = 1;
	secs->attributes = SGX_ATTR_MODE64BIT | SGX_ATTR_DEBUG;
	secs->xfrm = 7;

	for (secs->size = PAGE_SIZE; secs->size < bin_size; )
		secs->size <<= 1;

	uint64_t base = create_enclave_range(dev_fd, secs->size);
	if (!base)
		return false;

	secs->base = base;
	ioc.src = (unsigned long)secs;
	rc = ioctl(dev_fd, SGX_IOC_ENCLAVE_CREATE, &ioc);
	if (rc) {
		fprintf(stderr, "ECREATE failed rc=%d, err=%d.\n", rc, errno);
		munmap((void *)secs->base, secs->size);
		return false;
	}

	return true;
}


static bool encl_add_pages_with_mrmask(int dev_fd, uint64_t addr, void *data,
				       unsigned long length, uint64_t flags)
{
	struct sgx_enclave_add_pages_with_mrmask ioc;
	struct sgx_secinfo secinfo;
	int rc;

	memset(&secinfo, 0, sizeof(secinfo));
	secinfo.flags = flags;

	ioc.src = (uint64_t)data;
	ioc.addr = addr;
	ioc.secinfo = (unsigned long)&secinfo;
	ioc.mrmask = (__u16)-1;

	uint64_t added_size = 0;
	while (added_size < length) {
		rc = ioctl(dev_fd, SGX_IOC_ENCLAVE_ADD_PAGES_WITH_MRMASK, &ioc);
		if (rc) {
			fprintf(stderr, "EADD failed rc=%d.\n", rc);
			return false;
		}

		ioc.addr += PAGE_SIZE;
		ioc.src += PAGE_SIZE;
		added_size += PAGE_SIZE;
	}

	return true;
}

static bool encl_add_pages(int dev_fd, uint64_t addr, void *data,
			   unsigned long length, uint64_t flags)
{
	struct sgx_enclave_add_pages ioc;
	struct sgx_secinfo secinfo;
	int rc;

	memset(&secinfo, 0, sizeof(secinfo));
	secinfo.flags = flags;

	ioc.src = (uint64_t)data;
	ioc.offset = addr;
	ioc.length = length;
	ioc.secinfo = (unsigned long)&secinfo;
	ioc.flags = SGX_PAGE_MEASURE;

	rc = ioctl(dev_fd, SGX_IOC_ENCLAVE_ADD_PAGES, &ioc);
	if (rc) {
		fprintf(stderr, "EADD failed rc=%d.\n", rc);
		return false;
	}

	if (ioc.count != length) {
		fprintf(stderr, "EADD short of data.\n");
		return false;
	}

	return true;
}

static bool encl_build(struct sgx_secs *secs, void *bin, unsigned long bin_size, 
		       struct sgx_sigstruct *sigstruct,
		       struct sgx_einittoken *token)
{
	int dev_fd;
	int rc;

	dev_fd = open(sgx_dev_path, O_RDWR);
	if (dev_fd < 0) {
		fprintf(stderr, "Unable to open %s\n", sgx_dev_path);
		return false;
	}

	if (!encl_create(dev_fd, bin_size, secs))
		goto out_dev_fd;

	if (is_oot_driver) {
		if (!encl_add_pages_with_mrmask(dev_fd, secs->base, bin, PAGE_SIZE, SGX_SECINFO_TCS))
			goto out_map;

		if (!encl_add_pages_with_mrmask(dev_fd, secs->base + PAGE_SIZE, bin + PAGE_SIZE,
						bin_size - PAGE_SIZE, SGX_REG_PAGE_FLAGS))
			goto out_map;
	} else {
		if (!encl_add_pages(dev_fd, 0, bin, PAGE_SIZE, SGX_SECINFO_TCS))
			goto out_map;

		if (!encl_add_pages(dev_fd, PAGE_SIZE, bin + PAGE_SIZE,
				    bin_size - PAGE_SIZE, SGX_REG_PAGE_FLAGS))
			goto out_map;
	}

	if (is_oot_driver || no_sgx_flc) {
		struct sgx_enclave_init_with_token ioc;
		ioc.addr = secs->base;
		ioc.sigstruct = (uint64_t)sigstruct;
		ioc.einittoken = (uint64_t)token;
		rc = ioctl(dev_fd, SGX_IOC_ENCLAVE_INIT_WITH_TOKEN, &ioc);
	} else {
		struct sgx_enclave_init ioc;
		ioc.sigstruct = (uint64_t)sigstruct;
		rc = ioctl(dev_fd, SGX_IOC_ENCLAVE_INIT, &ioc);
	}

	if (rc) {
		printf("EINIT failed rc=%d\n", rc);
		goto out_map;
	}

	if (is_oot_driver)
		close(dev_fd);
	else {
		void *rc;

		rc = mmap((void *)secs->base, PAGE_SIZE,
			  PROT_READ | PROT_WRITE, MAP_FIXED | MAP_SHARED,
			  dev_fd, 0);
		if (rc == MAP_FAILED) {
			perror("mmap TCS");
			goto out_map;
		}

		rc = mmap((void *)secs->base + PAGE_SIZE, bin_size - PAGE_SIZE,
			  PROT_READ | PROT_WRITE | PROT_EXEC, MAP_FIXED | MAP_SHARED,
			  dev_fd, 0);
		if (rc == MAP_FAILED) {
			perror("mmap text & data");
			goto out_map;
		}

		enclave_fd = dev_fd;
	}

	return true;
out_map:
	munmap((void *)secs->base, secs->size);
out_dev_fd:
	close(dev_fd);
	return false;
}

static bool get_file_size(const char *path, off_t *bin_size)
{
	struct stat sb;
	int ret;

	ret = stat(path, &sb);
	if (ret) {
		perror("stat");
		return false;
	}

	if (!sb.st_size || sb.st_size & 0xfff) {
		fprintf(stderr, "Invalid blob size %lu\n", sb.st_size);
		return false;
	}

	*bin_size = sb.st_size;
	return true;
}

static bool encl_data_map(const char *path, void **bin, off_t *bin_size)
{
	int fd;

	fd = open(path, O_RDONLY);
	if (fd == -1)  {
		fprintf(stderr, "open() %s failed, errno=%d.\n", path, errno);
		return false;
	}

	if (!get_file_size(path, bin_size))
		goto err_out;

	*bin = mmap(NULL, *bin_size, PROT_READ, MAP_PRIVATE, fd, 0);
	if (*bin == MAP_FAILED) {
		fprintf(stderr, "mmap() %s failed, errno=%d.\n", path, errno);
		goto err_out;
	}

	close(fd);
	return true;

err_out:
	close(fd);
	return false;
}

static bool load_sigstruct(const char *path, void *sigstruct)
{
	int fd;

	fd = open(path, O_RDONLY);
	if (fd == -1)  {
		fprintf(stderr, "open() %s failed, errno=%d.\n", path, errno);
		return false;
	}

	if (read(fd, sigstruct, sizeof(struct sgx_sigstruct)) !=
	    sizeof(struct sgx_sigstruct)) {
		fprintf(stderr, "read() %s failed, errno=%d.\n", path, errno);
		close(fd);
		return false;
	}

	close(fd);
	return true;
}

static bool load_token(const char *path, void *token)
{
	int fd;

	fd = open(path, O_RDONLY);
	if (fd == -1)  {
		fprintf(stderr, "open() %s failed, errno=%d.\n", path, errno);
		return false;
	}

	if (read(fd, token, sizeof(struct sgx_einittoken)) !=
		sizeof(struct sgx_einittoken)) {
		fprintf(stderr, "read() %s failed, errno=%d.\n", path, errno);
		close(fd);
		return false;
	}

	close(fd);
	return true;
}

static void check_opts(const char *opt)
{
	if (!strcmp(opt, "no-sgx-flc"))
		no_sgx_flc = true;
	else if (!strcmp(opt, "fork"))
		fork_test = true;
}

static void parse_args(const char *args)
{
	char *a = strdup(args);
	if (!a)
		return;

	char *opt = strtok(a, " ");
	check_opts(opt);

	if (!opt) {
		free(a);
		return;
	}

	do {
		char *opt = strtok(NULL, " ");
		if (!opt)
			break;

		check_opts(opt);
	} while (1);

	free(a);
}

int pal_get_version(void)
{
	return 1;
}

typedef struct {
	const char *args;
	const char *log_level;
} pal_attr_t;

int pal_init(pal_attr_t *attr)
{
	struct sgx_sigstruct sigstruct;
	struct sgx_einittoken token;
	off_t bin_size;
	void *bin;

	parse_args(attr->args);

	detect_driver_type();

	tcs_busy = mmap(NULL, PAGE_SIZE, PROT_READ | PROT_WRITE,
			MAP_SHARED | MAP_ANONYMOUS, -1, 0);
	if (tcs_busy == MAP_FAILED)
		return -EINVAL;
	*(uint8_t *)tcs_busy = 0;
	
	if (!encl_data_map(IMAGE, &bin, &bin_size))
		return -ENOENT;

	if (!load_sigstruct(SIGSTRUCT, &sigstruct))
		return -ENOENT;

	if (!load_token(TOKEN, &token))
		return -ENOENT;

	if (!encl_build(&secs, bin, bin_size, &sigstruct, &token))
		return -EINVAL;

	initialized = true;	

	return 0;
}

typedef struct {
	int stdin, stdout, stderr;
} pal_stdio_fds;

int pal_exec(char *path, char *argv[], pal_stdio_fds *stdio,
	     int *exit_code)
{
	FILE *fp = fdopen(stdio->stderr, "w");
	if (!fp)
		return -1;

	if (!initialized) {
		fprintf(fp, "enclave runtime skeleton uninitialized yet!\n");
		fclose(fp);
		return -1;
	}

	bool is_child = false;

	if (fork_test) {
		switch (fork()) {
		case -1:
			fprintf(fp, "fork(), errno = %d\n", errno);
			fclose(fp);
			return -1;
		case 0:
			fprintf(fp, "run in child process, pid = %d\n", (int)getpid());
			is_child = true;
			break;
		default:
			wait(NULL);
			fprintf(fp, "run in parent process, pid = %d\n", (int)getpid());
			break;
		}
	}

	uint64_t result = 0;
	int ret = SGX_ENTER_1_ARG(ECALL_MAGIC, (void *)secs.base, &result);
	if (ret) {
		fprintf(fp, "failed to initialize enclave\n");
		fclose(fp);
		return ret;
	}
	if (result != INIT_MAGIC) {
		fprintf(fp, "Unexpected result: 0x%lx != 0x%lx\n", result, INIT_MAGIC);
		fclose(fp);
		return -1;
	}

	fprintf(fp, "Enclave runtime skeleton initialization succeeded\n");
	fclose(fp);

	if (fork_test && is_child)
		exit(0);

	*exit_code = 0;

	return 0;
}

int pal_destroy(void)
{
	if (!initialized) {
		fprintf(stderr, "Enclave runtime skeleton uninitialized yet!\n");
		return -1;
	}

	close(enclave_fd);

	return 0;
}
