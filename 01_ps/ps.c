// No copyright. 2021, Vladislav Aleinik

#define _POSIX_C_SOURCE 200809L
#define _DEFAULT_SOURCE

#include <stdlib.h>
#include <stdio.h>
#include <stdint.h>
#include <string.h> 

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

#include <unistd.h>
#include <sys/syscall.h>
#include <dirent.h>

// Define the linux_dirent64 structure from the manual:
struct linux_dirent64
{
	uint64_t       d_ino;    /* 64-bit inode number */
	uint64_t       d_off;    /* 64-bit offset to next structure */
	unsigned short d_reclen; /* Size of this dirent */
	unsigned char  d_type;   /* File type */
	char           d_name[]; /* Filename (null-terminated) */
};

const unsigned DIRENT_BUF_SIZE = 1024;
const unsigned FILENAME_SIZE   = 32;
const unsigned COMMAND_SIZE    = 256;
const char     FILENAME_COMM[] = "/comm";

int main(int argc, char* argv[])
{
	// Open /proc directory:
	int procdir_fd = open("/proc", O_RDONLY|O_DIRECTORY);
	if (procdir_fd == -1)
	{
		perror("[ERROR] Unable to open /proc directory\n");
		exit(EXIT_FAILURE);
	}

	// Printf the heading:
	printf("   PID CMD\n");

	// Parse the /proc FS:
	int bytes_read = 0;
	do
	{
		// Read more /proc entries:
		char entries[DIRENT_BUF_SIZE];
		bytes_read = syscall(SYS_getdents64, procdir_fd, entries, DIRENT_BUF_SIZE);
		if (bytes_read == -1)
		{
			perror("[ERROR] Unable to get /proc directory entry\n");
			exit(EXIT_FAILURE);
		}

		for (int byte_i = 0; byte_i < bytes_read;)
		{
			struct linux_dirent64* entry = (struct linux_dirent64*) &entries[byte_i];

			byte_i += entry->d_reclen;
			unsigned pid_name_len = entry->d_reclen - ((char*)&entry->d_name - (char*)entry) - 1;

			char* endptr = NULL;
			long pid_num = strtol(entry->d_name, &endptr, 10);
			if (*endptr != '\0')
			{
				// Inspect only [pid] directories:
				break;
			}

			// Concat the "[pid]/comm" pathname:
			char pathname[FILENAME_SIZE];
			
			if (pid_name_len + sizeof(FILENAME_COMM) < FILENAME_SIZE)
			{
				strncpy(&pathname[0], entry->d_name, pid_name_len);

				char* null = strchr(pathname, '\0');
				strncpy(null, FILENAME_COMM, sizeof(FILENAME_COMM));
			}
			else
			{
				perror("[ERROR] Process PID is too long\n");
				exit(EXIT_FAILURE);
			}

			// Open the /proc/[pid]/comm file:
			int comm_fd = openat(procdir_fd, pathname, O_RDONLY);
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
	}
	while (bytes_read != 0);

	// Close the /proc directory:
	if (close(procdir_fd) == -1)
	{
		perror("[ERROR] Unable to close /proc directory\n");
		exit(EXIT_FAILURE);
	}


	return EXIT_SUCCESS;
}