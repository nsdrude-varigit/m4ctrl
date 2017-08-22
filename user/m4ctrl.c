/*
 * Copyright 2017 NXP
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without modification,
 * are permitted provided that the following conditions are met:
 *
 * o Redistributions of source code must retain the above copyright notice, this list
 *   of conditions and the following disclaimer.
 *
 * o Redistributions in binary form must reproduce the above copyright notice, this
 *   list of conditions and the following disclaimer in the documentation and/or
 *   other materials provided with the distribution.
 *
 * o Neither the name of NXP Semiconductor, Inc. nor the names of its
 *   contributors may be used to endorse or promote products derived from this
 *   software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE FOR
 * ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON
 * ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 * SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */
/*
 * User-space tool for M4 control
 */

#include <fcntl.h>
#include <getopt.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <fcntl.h>
#include "../include/m4ctrl.h"

#define DEVICE_PATH	"/dev/m4ctrl"

/*
 * getopt structures
 */

/* The tool's command line options */
static struct option long_options[] =
{
    {"help", no_argument,       0, 'h'},
    {"core", required_argument, 0, 'c'},
    {"start",  no_argument,       0, 's'},
    {"stop",   no_argument, 0, 'x'},
    {"deploy", required_argument, 0, 'd'},
    {0, 0, 0, 0}
};

/*
 * use case
 */
static void usage(const char *argv0)
{
	fprintf(stdout, "M4 Control Tool\n"
	       "Usage: %s <options> [--core=<n>] \n options:\n"
		"\t --help - display the list of supported commands\n"
		"\t --start - start the specified M4 core\n"
		"\t --stop - stop the specified M4 core\n"
		"\t --deploy=<firmware_file> - deploy firmware_file on the specified M4 core\n",
	       argv0);
	exit(EXIT_FAILURE);
}

static int core_idx = 0, start = 0, stop = 0, deploy = 0;
static char * filename = NULL;
static int fd, fd_mem;
static char * tcml = NULL;
static void platform_setup()
{

	fd = open(DEVICE_PATH, O_RDONLY);
	if (fd < 0) {
		perror("Error during opening the m4ctrl kernel device \n");
		exit(EXIT_FAILURE);
	}

	fd_mem = open("/dev/mem", O_RDWR | O_SYNC);

	if (fd_mem < 0) {
		perror("failed to open /dev/mem \n");
		exit(EXIT_FAILURE);
	}

	tcml = (void *) mmap(NULL,
				 core_idx ? TCML_RESERVED_SIZE_M1 : TCML_RESERVED_SIZE_M0,
				 PROT_READ | PROT_WRITE,
				 MAP_SHARED, fd_mem,
				 core_idx ? TCML_ADDR_M1 : TCML_ADDR_M0
				);
	if ((void *) tcml == MAP_FAILED)
	{
	    perror("failed to mmap tcml");
	    exit(EXIT_FAILURE);
	}

}
static void parse_cmds(int argc, char ** argv)
{
	int c;
	struct stat sb;

	if (argc < 2) {
		fprintf(stderr, "too few parameters\n");
		usage(argv[0]);
	}

	while(1)
	{
		/* getopt_long stores the option index here. */
		int option_index = 0;

		c = getopt_long(argc, argv, "c:h::sxd:",
				long_options, &option_index);

		/* Detect the end of the options. */
		if (c == -1)
		        break;

		switch (c)
		{
			case 'h':
				usage(argv[0]);
				exit(EXIT_SUCCESS);
			case 'c':
				core_idx = atoi(optarg);
				if (core_idx < 0 || core_idx > (M4_CORES_NUM - 1)) {
					fprintf(stderr, "Invalid M4 core index.\n");
					if ((M4_CORES_NUM - 1) == 0) {
						fprintf(stderr, "It shall be 0.\n");
					}
					else {
						fprintf(stderr, "It shall be between 0 and %d.\n", M4_CORES_NUM - 1);
					}
					exit(EXIT_FAILURE);
			}

				break;

			case 's':
				if (stop) {
					fprintf(stderr, "It is not allowed to use simultaneously start and stop commands\n");
					exit(EXIT_FAILURE);
				}

				start = 1;

				break;

			case 'x':
				if (start) {
					fprintf(stderr, "It is not allowed to use simultaneously start and stop commands\n");
					exit(EXIT_FAILURE);
				}

				stop = 1;

				break;

			case 'd':
				if (start || stop) {
					fprintf(stderr, "It is not allowed to use simultaneously start or stop commands together with deploy\n");
					exit(EXIT_FAILURE);
				}

				if (stat(optarg, &sb) < 0) {
					perror("Impossible to access the firmware file\n");
					exit(EXIT_FAILURE);
				}

				deploy = 1;
				filename = strdup(optarg);

				break;

			case '?':
				/* getopt_long already printed an error message. */
				break;

			default:
				abort ();
		}
	};
}
static void  m4_core_start()
{
    int cmd = core_idx ? M4CTRL_START_CORE_M1 : M4CTRL_START_CORE_M0;
    if (ioctl(fd, cmd, 0) < 0) {
	perror("ioctl");
	exit(EXIT_FAILURE);
	printf("Start cortex M4, core %d\n", core_idx);
    }
}
static void m4_core_stop()
{
    int cmd = core_idx ? M4CTRL_STOP_CORE_M1 : M4CTRL_STOP_CORE_M0;
    if (ioctl(fd, cmd, 0) < 0) {
	perror("ioctl");
	exit(EXIT_FAILURE);
    }

    printf("Stop cortex M4, core %d\n", core_idx);
}
static void  m4_core_pwroff()
{
    int cmd = core_idx ? M4CTRL_PWROFF_CORE_M1 : M4CTRL_PWROFF_CORE_M0;
    if (ioctl(fd, cmd, 0) < 0) {
	perror("ioctl");
	exit(EXIT_FAILURE);
	printf("Poweroff cortex M4, core %d\n", core_idx);
    }
}
static void m4_core_pwron()
{
    int cmd = core_idx ? M4CTRL_PWRON_CORE_M1 : M4CTRL_PWRON_CORE_M0;
    if (ioctl(fd, cmd, 0) < 0) {
	perror("ioctl");
	exit(EXIT_FAILURE);
    }

    printf("Poweron cortex M4, core %d\n", core_idx);
}

static void m4_image_transfer(char *dst, const char * binary_name)
{
    int fw_fd;
    char block;

    if ((fw_fd = open(binary_name, O_RDONLY)) < 0) {
	perror("failed to open firmware image\n");
	exit(EXIT_FAILURE);
    }

    sync();

    while (read(fw_fd, &block, sizeof(block)) ==sizeof(block))
    {
	*dst++ = block;
	/* workaround for caching bug from imx8qm*/
	*(dst-1);
    }

    sync();

    close(fw_fd);
}

static void  m4_core_deploy()
{
	m4_core_stop();
	m4_core_pwroff();
	m4_core_pwron();
	m4_image_transfer(tcml, filename);
	m4_core_start();
}

static void execute_cmds()
{
	/* Start the specified M4 core */
	if (start)
	{
		m4_core_start();
	}

	/* Stop the specified M4 core */
	if (stop)
	{
		m4_core_stop();
	}

	if (deploy)
	{
		m4_core_deploy();
	}

	close(fd);
}
static void cleanup()
{
	/* Close the file descriptor for m4ctrl device driver */
	if (fd > 0)
	    close(fd);
	/* Unmap the memory */
    if (munmap(tcml, core_idx ? TCML_RESERVED_SIZE_M1 : TCML_RESERVED_SIZE_M0) == -1) {
        perror("failed to munmap tcml");
        exit(EXIT_FAILURE);
    }
    if (close(fd_mem) == -1) {
        perror("failed to close /dev/mem");
        exit(EXIT_FAILURE);
    }

}
int main(int argc, char **argv)
{
	parse_cmds(argc, argv);
	platform_setup();
	execute_cmds();
	cleanup();
	return 0;
}
