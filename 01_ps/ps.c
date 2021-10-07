// No copyright. 2021, Vladislav Aleinik

#define _POSIX_C_SOURCE 200809L
#define _DEFAULT_SOURCE

#include <stdlib.h>
#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>
#include <string.h> 

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <dirent.h>

const unsigned DIRENT_BUF_SIZE = 1024;
const unsigned FILENAME_SIZE   = 32;
const unsigned COMMAND_SIZE    = 256;

int main(int argc, char* argv[])
{
	// Open /proc directory:
	DIR* proc_dir = opendir("/proc");
	if (proc_dir == NULL)
	{
		perror("[ERROR] Unable to open /proc directory\n");
		exit(EXIT_FAILURE);
	}

	int proc_fd = dirfd(proc_dir);
	if (proc_fd == -1)
	{
		perror("[ERROR] Unable to get /proc directory file descriptor\n");
		exit(EXIT_FAILURE);
	}

	// Printf the heading:
	printf("   PID CMD\n");

	while (true)
	{
		// Read one /proc entry:
		struct dirent* proc_entry = readdir(proc_dir);
		if (proc_entry == NULL)
		{
			break;
		}

		// Try to parse PID:
		char* endptr = NULL;
		long pid_num = strtol(proc_entry->d_name, &endptr, 10);
		if (*endptr != '\0')
		{
			// Inspect only [pid] directories:
			continue;
		}

		// Concat the "[pid]/comm" pathname:
		char pathname[FILENAME_SIZE];

		int written = snprintf(pathname, sizeof(pathname), "%s/comm", proc_entry->d_name);
		if (written >= FILENAME_SIZE)
		{
			perror("[ERROR] Process PID is too long\n");
			exit(EXIT_FAILURE);
		}

		// Open the /proc/[pid]/comm file:
		int comm_fd = openat(proc_fd, pathname, O_RDONLY);
		if (comm_fd == -1)
		{
			fprintf(stderr, "[ERROR] Unable to open file '%s'\n", pathname);
			exit(EXIT_FAILURE);
		}

		// Read the command name:
		char command[COMMAND_SIZE];
		int cmd_bytes = read(comm_fd, command, COMMAND_SIZE - 1);
		if (cmd_bytes == -1)
		{
			perror("[ERROR] Unable to read from /proc/[PID]/comm file\n");
			exit(EXIT_FAILURE);
		}

		command[cmd_bytes] = '\0';

		// Close /proc/[PID]/comm:
		if (close(comm_fd) == -1)
		{
			perror("[ERROR] Unable to close /proc/[PID]/comm file\n");
			exit(EXIT_FAILURE);
		}

		// Print the result:
		printf(" %5ld %s", pid_num, command);
	}

	// Close the /proc directory:
	if (closedir(proc_dir) == -1)
	{
		perror("[ERROR] Unable to close /proc directory\n");
		exit(EXIT_FAILURE);
	}

	return EXIT_SUCCESS;
}