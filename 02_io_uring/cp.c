// No copyright. 2021, Vladislav Aleinik

#define _DEFAULT_SOURCE 1
#define _GNU_SOURCE 1

#include <stdlib.h>
#include <stdio.h>
#include <stdbool.h>

#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <fcntl.h>

#include "Logging.h"
#include "IO_Ring.h"

//===========================
// Copy procedure parameters 
//===========================

const unsigned READ_BLOCK_SIZE  = 4096U;
const unsigned NUM_RING_ENTRIES =    8U;

//================
// Copying status
//================

typedef enum {
    BLOCK_IDLE     = 0,
    BLOCK_IN_READ  = 1,
    BLOCK_IN_WRITE = 2
} BlockStage;

struct BlockStatus
{
    BlockStage stage;

    off_t offset;
    uint32_t size;
};

struct CopyStatus
{
    int src_fd;
    int dst_fd;

    off_t src_off;
    uint32_t src_size;

    uint16_t num_block_in_progress;

    struct BlockStatus* block_statuses;
};

void init_copying_status(struct CopyStatus* status, uint32_t src_size, int src_fd, int dst_fd)
{
    status->src_fd   = src_fd;
    status->dst_fd   = dst_fd;
    status->src_off  = 0;
    status->src_size = src_size;

    status->num_block_in_progress = 0;

    status->block_statuses = malloc(NUM_RING_ENTRIES * sizeof(struct BlockStatus));
    if (status->block_statuses == NULL)
    {
        LOG_ERROR("Unable to allocate memory for block statuses");
        exit(EXIT_FAILURE);
    }

    for (uint16_t i = 0; i < NUM_RING_ENTRIES; ++i)
    {
        status->block_statuses[i].stage  = BLOCK_IDLE;
        status->block_statuses[i].offset = 0;
        status->block_statuses[i].size   = 0;
    }
}

void prepare_read_request(struct IO_Ring* io_ring, struct CopyStatus* status, unsigned cell)
{
    struct BlockStatus* block = &status->block_statuses[cell];

    uint32_t bytes_left = status->src_size - status->src_off;
    if (bytes_left == 0) return;

    // Get the block to transfer:
    block->stage  = BLOCK_IN_READ;
    block->offset = status->src_off;
    block->size   = (bytes_left < READ_BLOCK_SIZE)? bytes_left : READ_BLOCK_SIZE;

    enqueue_io_request(io_ring, IORING_OP_READ_FIXED, status->src_fd, cell, block->offset, block->size);

    status->src_off += block->size;
    status->num_block_in_progress += 1;
}

void prepare_write_request(struct IO_Ring* io_ring, struct CopyStatus* status, unsigned cell)
{
    struct BlockStatus* block = &status->block_statuses[cell];

    block->stage = BLOCK_IN_WRITE;
    
    enqueue_io_request(io_ring, IORING_OP_WRITE_FIXED, status->dst_fd, cell, block->offset, block->size);
}

void finish_write_request(struct CopyStatus* status, unsigned cell)
{
    struct BlockStatus* block = &status->block_statuses[cell];

    block->stage = BLOCK_IDLE;

    status->num_block_in_progress -= 1;

    LOG("Cell#%02d is IDLE", cell);
}

//=================
// File operations 
//=================

void open_src_file(const char* filename, int* fd, uint32_t* file_size)
{
    *fd = open(filename, O_RDONLY);
    if (*fd == -1)
    {
        LOG_ERROR("Unable to open source file '%s'", filename);
    }

    struct stat statbuf;
    if (fstat(*fd, &statbuf) == -1)
    {
        LOG_ERROR("Unable to determine source file size");
    }

    *file_size = statbuf.st_size;
}

void open_dst_file(const char* filename, int* fd, uint32_t src_size)
{
    *fd = open(filename, O_WRONLY|O_CREAT|O_TRUNC, 0644);
    if (*fd == -1)
    {
        LOG_ERROR("Unable to open destination file '%s'", filename);
    }

    uint32_t to_allocate = src_size;
    if (src_size >= (1ULL << 32ULL))
    {
        to_allocate = 0xFFFFFFFFU;
    }

    if (fallocate(*fd, 0, 0, to_allocate) == -1)
    {
        LOG_ERROR("Not enough space for file '%s'", filename);
    }
}

//================
// Copy operation 
//================

int main(int argc, char* argv[])
{
    if (argc != 3)
    {
        LOG_ERROR("Usage: uring_cp <src> <dst>");
    }

    // Open source file and determine it's size:
    int src_fd;
    uint32_t src_size;
    open_src_file(argv[1], &src_fd, &src_size);

    // Create the destination file and allocate space on the disk:
    int dst_fd;
    open_dst_file(argv[2], &dst_fd, src_size);

    LOG("Start initializing IO-ring");

    // Initialize IO-userspace-ring:
    struct IO_Ring io_ring;
    init_io_ring(&io_ring, NUM_RING_ENTRIES);

    // Create buffers to store intermediate data:
    char* aligned_buffers = (char*) aligned_alloc(READ_BLOCK_SIZE, NUM_RING_ENTRIES * READ_BLOCK_SIZE);
    if (aligned_buffers == NULL)
    {
        LOG_ERROR("Unable to allocate aligned memory");
    }

    struct iovec* fixed_buffers = (struct iovec*) malloc(NUM_RING_ENTRIES * sizeof(struct iovec));
    if (fixed_buffers == NULL)
    {
        LOG_ERROR("Unable to allocate iovec buffer");
    }

    for (unsigned i = 0; i < NUM_RING_ENTRIES; ++i)
    {
        fixed_buffers[i].iov_base = aligned_buffers + i * READ_BLOCK_SIZE;
        fixed_buffers[i].iov_len  = READ_BLOCK_SIZE;
    }

    // Register buffers for IO-ring:
    register_io_buffers(&io_ring, fixed_buffers, NUM_RING_ENTRIES);

    // Allocate copying status structure:
    struct CopyStatus status;
    init_copying_status(&status, src_size, src_fd, dst_fd);

    // Perform copy operation:
    LOG("Copying '%s' of size %u", argv[1], src_size);

    while (status.src_off != status.src_size ||
           status.num_block_in_progress != 0)
    {
        // Use all idle cells for reads:
        for (int cell_i = 0; cell_i < NUM_RING_ENTRIES; ++cell_i)
        {
            if (status.block_statuses[cell_i].stage == BLOCK_IDLE)
            {
                prepare_read_request(&io_ring, &status, cell_i);
            }
        }

        // Submit all unsubmitted reqs:
        submit_and_wait(&io_ring);

        int cell_i = -1;
        do
        {
            struct io_uring_cqe ret;
            ret = get_one_completed_io(&io_ring);

            cell_i = ret.user_data;

            if (cell_i != -1 && status.block_statuses[cell_i].stage == BLOCK_IN_READ)
            {
                if (ret.res == -1)
                {
                    LOG_ERROR("Read operation failed at offset: %lu", status.block_statuses[cell_i].offset);
                }

                prepare_write_request(&io_ring, &status, cell_i);
            }
            else if (cell_i != -1 && status.block_statuses[cell_i].stage == BLOCK_IN_WRITE)
            {
                if (ret.res == -1)
                {
                    LOG_ERROR("Write operation failed at offset: %lu", status.block_statuses[cell_i].offset);
                }

                finish_write_request(&status, cell_i);
            } 
        }
        while (cell_i != -1);
    }

    // Free IO-ring:
    free_io_ring(&io_ring);

    // Deallocate memory:
    free(aligned_buffers);
    free(fixed_buffers);

    // Close files:
    if (close(src_fd) == -1)
    {
        LOG_ERROR("Unable to close file '%s'", argv[1]);
    }

    if (close(dst_fd) == -1)
    {
        LOG_ERROR("Unable to close file '%s'", argv[2]);
    }
    
    return EXIT_SUCCESS;
}
