/*
 * Copyright (c) 2013-2014, NVIDIA CORPORATION. All rights reserved
 *
 * Permission is hereby granted, free of charge, to any person obtaining
 * a copy of this software and associated documentation files
 * (the "Software"), to deal in the Software without restriction,
 * including without limitation the rights to use, copy, modify, merge,
 * publish, distribute, sublicense, and/or sell copies of the Software,
 * and to permit persons to whom the Software is furnished to do so,
 * subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be
 * included in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 * EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
 * IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY
 * CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT,
 * TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE
 * SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 */

#define LOG_TAG "TLK_Daemon"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/ioctl.h>

#include <client/ote_client.h>

#include <android/log.h>
#include <utils/Log.h>

#define OTE_NEW_REQ_MAX_FAILURES	50

int main(int argc, char* argv[])
{
	ote_file_req *req;
	te_error_t err = OTE_SUCCESS;
	int fd = 0;
	mode_t mode = S_IRUSR | S_IWUSR | S_IRGRP;
	char *homedir = NULL, *file = NULL, *fileloc = NULL;
	pid_t pid, sid;
	struct stat dstat;
	int result = 0, req_failures;

	char devicename[64];
	int dev_fd = 0;

	if (argc < 3) {
		ALOGE("insufficient input arguments\n");
		exit(2);
	}

	while (argc != 0) {
		argv++;
		argc--;
		if (strncmp(argv[0], "--storagedir",
					strlen("--storagedir")) == 0) {
			fileloc = malloc(strlen(argv[1]) + strlen("/tlk/") +
					OTE_MAX_FILE_NAME_LEN + 1);
			if (!fileloc) {
				ALOGE("Out of memory\n");
				return 2;
			}

			strncpy(fileloc, argv[1], strlen(argv[1]));
			argc -= 2;
		}
	}

	result = stat(fileloc, &dstat);
	if (result == 0) {
		/* Check access rights if directory exists */
		if ((dstat.st_mode & (S_IXUSR | S_IWUSR)) !=
			(S_IXUSR | S_IWUSR)) {
			ALOGE("'%s' - no read-write access",
				fileloc);
			free(fileloc);
			return 1;
		}
	} else if (errno == ENOENT) {
		ALOGE("'%s' does not exist", fileloc);
		free(fileloc);
		return 1;
	} else{
		/* Another error */
		ALOGE("'%s' is incorrect or cannot be reached",
			fileloc);
		free(fileloc);
		return 1;
	}

	strncat(fileloc, "/tlk", strlen("/tlk"));

	/*
	 * Turns this application into a daemon => fork off parent process
	 */

	/* Fork off the parent process */
	pid = fork();
	if (pid < 0) {
		ALOGE("could not fork daemon\n");
		free(fileloc);
		return 1;
	}

	if (pid > 0) {
		/* parent */
		free(fileloc);
		return 0;
	}

	/* Change the file mode mask */
	umask(0077);

	/* Detach from the console */
	sid = setsid();
	if (sid < 0) {
		ALOGE("daemon group creation failed");
		free(fileloc);
		return 1;
	}

	ALOGI("started\n");

	strcpy(devicename, "/dev/");
	strcat(devicename, TLK_DEVICE_BASE_NAME);

	dev_fd = open(devicename, O_RDWR, 0);
	if (dev_fd < 0) {
		ALOGE("Failed to open device (%s)\n", devicename);
		free(fileloc);
		return 1;
	}

	homedir = malloc(strlen(fileloc) + 1);
	if (!homedir) {
		ALOGE("Out of memory\n");
		free(fileloc);
		return 2;
	}

	strncpy(homedir, fileloc, strlen(fileloc));

	/*
	 * create tlk directory with read/write/search permissions for owner
	 * and group, and with read/search permissions for
	 * others
	 */
	mkdir(fileloc, S_IXUSR | S_IWUSR | S_IRUSR);

	req = malloc(sizeof(ote_file_req));
	if (!req) {
		ALOGE("Request allocation failed\n");
		req->error = -ENOMEM;
		goto exit;
	}

	strcat(fileloc, "/");

	for (;;) {
		memset(req, 0, sizeof(ote_file_req));

		/* get new file request */
		req_failures = OTE_NEW_REQ_MAX_FAILURES;
		do {
			result = ioctl(dev_fd, TE_IOCTL_FILE_NEW_REQ, req);
			if (result < 0)
				ALOGE("IOCTL_FILE_NEW_REQ returned %d\n", errno);
		} while ((result < 0) && (errno == ENODATA) && (--req_failures > 0));

		if (result < 0) {
			ALOGE("IOCTL_FILE_NEW_REQ Failed (%d)\n", errno);
			err = -ENOMEM;
			goto exit;
		}

		/*
		 * Check if home directory exists. Andriod cleans the /data
		 * directory during file system encryption.
		 */
		result = stat(homedir, &dstat);
		if (result == 0) {
			/* Check access rights if directory exists */
			if ((dstat.st_mode & (S_IXUSR | S_IWUSR)) !=
				(S_IXUSR | S_IWUSR)) {
				ALOGE("'%s' - no read-write access",
					homedir);
				goto exit;
			}
		} else if (errno == ENOENT) {
				/*
				 * create tlk directory with read/write/search
				 * permissions for owner and group, and with
				 * read/search permissions for others
				 */
				mkdir(homedir, S_IXUSR | S_IWUSR | S_IRUSR);
		} else{
			/* Another error */
			ALOGE("'%s' is incorrect or cannot be reached",
				homedir);
			goto exit;
		}

		req->user_data_buf = malloc(req->data_len);
		if (!req->user_data_buf) {
			ALOGE("Data memory allocation failed\n");
			req->error = -ENOMEM;
			goto exit;
		}

		file = fileloc + strlen(argv[1]) + strlen("/tlk/");
		if (!file) {
			ALOGE("file memory allocation failed\n");
			err = -ENOMEM;
			goto exit;
		}

		if (strlen(req->name) > OTE_MAX_FILE_NAME_LEN) {
			err = -EINVAL;
			goto exit;
		}

		memset(file, 0, OTE_MAX_FILE_NAME_LEN);
		strncpy(file, req->name, strlen(req->name));
		file = fileloc;

		ALOGD("New request: (%s) %s\n", file,
			(req->type == OTE_FILE_REQ_READ) ? "read" :
			(req->type == OTE_FILE_REQ_DELETE) ? "delete" :
			(req->type == OTE_FILE_REQ_SIZE) ? "size" : "write");

		/*
		 * OTE_FileGetNewRequest gives us the length in terms of bytes
		 * to be read/written. Allocate memory, pass it to the kernel
		 * for it to copy data to in case of writes. For reads, allocate
		 * memory, read into it and pass the buffer to the kernel.
		 */
		switch (req->type) {
		case OTE_FILE_REQ_READ:
			fd = open(file, O_RDONLY, mode);
			if (fd < 0) {
				ALOGE("%s failed to open\n", file);
				req->error = -ENODEV;
				goto wait;
			}

			ALOGI("read %d bytes\n", req->data_len);
			req->error = read(fd, req->user_data_buf, req->data_len);
			if (req->error <= 0) {
				ALOGE("%s failed to read\n", file);
				goto wait;
			}

			/* how many bytes did we actually read? */
			req->result = req->error;
			req->error = 0;
			break;

		case OTE_FILE_REQ_WRITE:
			fd = open(file, O_WRONLY | O_CREAT, mode);
			if (fd < 0) {
				ALOGE("%s failed to open\n", file);
				req->error = -ENODEV;
				goto wait;
			}

			result = ioctl(dev_fd, TE_IOCTL_FILE_FILL_BUF, req);
			if (result < 0) {
				ALOGE("TEE_IOCTL_FILE_FILL_BUF failed\n");
				err = -ENOMEM;
				req->error = -ENOMEM;
				goto exit;
			}

			ALOGI("write %d bytes\n", req->data_len);
			req->error = write(fd, req->user_data_buf,
					req->data_len);
			if (req->error <= 0) {
				ALOGE("%s failed to write\n", file);
				goto wait;
			}

			/* how many bytes did we actually write? */
			req->result = req->error;
			req->error = 0;
			break;

		case OTE_FILE_REQ_DELETE:
			fd = 0;
			remove(file);
			req->error = 0;
			break;

		case OTE_FILE_REQ_SIZE:
			result = stat(file, &dstat);
			if (result == 0) {
				/* return file size in bytes */
				req->error = 0;
				req->result = dstat.st_size;
				ALOGI("%s contains %d bytes\n", file,
						req->result);
			} else {
				ALOGE("'%s' does not exist, is incorrect or "
					"cannot be reached", file);
				req->error = -ENOENT;
			}
			break;

		default:
			ALOGE("Incorrect request type\n");
			err = -EINVAL;
			goto exit;
		}

wait:
		if (fd)
			close(fd);

		/* indicate completion */
		ioctl(dev_fd, TE_IOCTL_FILE_REQ_COMPLETE, req);
		free(req->user_data_buf);
	}

exit:
	if (fd)
		close(fd);
	if (dev_fd)
		close(dev_fd);
	free(fileloc);
	free(homedir);
	free(file);
	free(req->user_data_buf);
	free(req);

	ALOGE("exit\n");
	return 0;
}
