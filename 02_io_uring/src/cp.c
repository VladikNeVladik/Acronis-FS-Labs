// No copyright. 2021, Vladislav Aleinik
#define _GNU_SOURCE

#include <stdlib.h>
#include <stdio.h>
#include <stdbool.h>
#include <string.h>

#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <fcntl.h>

#include <xmalloc.h>
#include <logging.h>
#include <liburing.h>

//===========================
// Copy procedure parameters 
//===========================

#define NUM_RING_ENTRIES    8U
#define READ_BLOCK_SIZE  4096U

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

    off64_t offset;
    uint32_t size;
};

struct CopyStatus
{
    int src_fd;
    int dst_fd;

    off_t src_off;
    uint32_t src_size;

    uint16_t num_block_in_progress;

    struct BlockStatus block_statuses[NUM_RING_ENTRIES];

    char* aligned_buffers;
    struct iovec* fixed_buffers;

    struct io_uring io_ring;
};

int init_copying_status(struct CopyStatus* status, uint32_t src_size, int src_fd, int dst_fd)
{
    status->src_fd   = src_fd;
    status->dst_fd   = dst_fd;
    status->src_off  = 0;
    status->src_size = src_size;

    status->num_block_in_progress = 0;

    for (uint16_t i = 0; i < NUM_RING_ENTRIES; ++i)
    {
        status->block_statuses[i].stage  = BLOCK_IDLE;
        status->block_statuses[i].offset = 0;
        status->block_statuses[i].size   = 0;
    }

    // Initialize IO-userspace-ring:
    if (io_uring_queue_init(NUM_RING_ENTRIES, &status->io_ring, 0) != 0)
    {
        LOG_ERROR("Unable to initialize IO-ring");
        return -1;
    }

    // Create buffers to store intermediate data:
    status->aligned_buffers = (char*) aligned_xalloc(READ_BLOCK_SIZE, NUM_RING_ENTRIES * READ_BLOCK_SIZE);

    status->fixed_buffers = xmalloc(NUM_RING_ENTRIES * sizeof(struct iovec));

    for (unsigned i = 0; i < NUM_RING_ENTRIES; ++i)
    {
        status->fixed_buffers[i].iov_base = status->aligned_buffers + i * READ_BLOCK_SIZE;
        status->fixed_buffers[i].iov_len  = READ_BLOCK_SIZE;
    }

    if (io_uring_register_buffers(&status->io_ring, status->fixed_buffers, NUM_RING_ENTRIES) != 0)
    {
        LOG_ERROR("Unable to register intermediate buffers: errno=%i (%s)", errno, strerror(errno));
        return -1;
    }

    return 0;
}

void free_copying_status(struct CopyStatus* status)
{
    free(status->aligned_buffers);
    free(status->fixed_buffers);
}

//=====================
// Io-ring interaction 
//=====================

void prepare_read_request(struct CopyStatus* status, unsigned cell)
{
    struct BlockStatus* block = &status->block_statuses[cell];

    uint32_t bytes_left = status->src_size - status->src_off;
    if (bytes_left == 0) return;

    // Get the block to transfer:
    BUG_ON(block->stage != BLOCK_IDLE, "Invalid cell#%02d stage: expected BLOCK_IDLE", cell);

    block->stage  = BLOCK_IN_READ;
    block->offset = status->src_off;
    block->size   = (bytes_left < READ_BLOCK_SIZE)? bytes_left : READ_BLOCK_SIZE;

    // Enqueue read request: 
    struct io_uring_sqe* read_sqe = io_uring_get_sqe(&status->io_ring);

    BUG_ON(read_sqe == NULL, "Expected non-full IO-ring!");

    io_uring_prep_read_fixed(read_sqe, status->src_fd,
                             status->fixed_buffers[cell].iov_base,
                             block->size, block->offset, cell);

    read_sqe->user_data = cell;

    status->src_off += block->size;
    status->num_block_in_progress += 1;

    LOG("Cell#%02d:  read (off=%lu, size=%u)", cell, block->offset, block->size);
}

void prepare_write_request(struct CopyStatus* status, unsigned cell)
{
    struct BlockStatus* block = &status->block_statuses[cell];

    BUG_ON(block->stage != BLOCK_IN_READ, "Invalid cell#%02d stage: expected BLOCK_IN_READ", cell);
    block->stage = BLOCK_IN_WRITE;

    // Enqueue write request:
    struct io_uring_sqe* write_sqe = io_uring_get_sqe(&status->io_ring);

    BUG_ON(write_sqe == NULL, "Expected non-full IO-ring!");

    io_uring_prep_write_fixed(write_sqe, status->dst_fd,
                              status->aligned_buffers + cell * READ_BLOCK_SIZE,
                              block->size, block->offset, cell);

    write_sqe->user_data = cell;

    LOG("Cell#%02d: write (off=%lu, size=%u)", cell, block->offset, block->size);
}

void finish_write_request(struct CopyStatus* status, unsigned cell)
{
    struct BlockStatus* block = &status->block_statuses[cell];

    BUG_ON(!(block->offset < status->src_off), "Invalid cell%02d block offset: %zu (src_off=%zu)", cell, block->offset, status->src_off);

    BUG_ON(block->stage != BLOCK_IN_WRITE, "Invalid cell#%02d stage: expected BLOCK_IN_WRITE", cell);
    block->stage = BLOCK_IDLE;

    BUG_ON(status->num_block_in_progress == 0, "Expected at least one block operation to be in progress");
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
        LOG_ERROR("Unable to open source file '%s': errno=%i (%s)", filename, errno, strerror(errno));
        exit(EXIT_FAILURE);
    }

    struct stat statbuf;
    if (fstat(*fd, &statbuf) == -1)
    {
        LOG_ERROR("Unable to determine source file size: errno=%i (%s)", errno, strerror(errno));
        exit(EXIT_FAILURE);
    }

    *file_size = statbuf.st_size;
}

void open_dst_file(const char* filename, int* fd, uint32_t src_size)
{
    *fd = open(filename, O_WRONLY|O_CREAT|O_TRUNC, 0644);
    if (*fd == -1)
    {
        LOG_ERROR("Unable to open destination file '%s': errno=%i (%s)", filename, errno, strerror(errno));
        exit(EXIT_FAILURE);
    }

    uint32_t to_allocate = src_size;
    if (src_size >= (1ULL << 32ULL)) to_allocate = 0xFFFFFFFFU;

    if (fallocate(*fd, 0, 0, to_allocate) == -1)
    {
        LOG_ERROR("Not enough space for file '%s': errno=%i (%s)", filename, errno, strerror(errno));
        exit(EXIT_FAILURE);
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
        exit(EXIT_FAILURE);
    }

    // Open source file and determine it's size:
    int src_fd;
    uint32_t src_size;
    open_src_file(argv[1], &src_fd, &src_size);

    // Create the destination file and allocate space on the disk:
    int dst_fd;
    open_dst_file(argv[2], &dst_fd, src_size);

    LOG("Start initializing IO-ring");

    // Perform copy operation:
    struct CopyStatus status;
    if (init_copying_status(&status, src_size, src_fd, dst_fd) == -1)
    {
        LOG_ERROR("Unable to initialize copying status");
        exit(EXIT_FAILURE);
    }

    LOG("Copying '%s' of size %u", argv[1], src_size);

    while (status.src_off != status.src_size ||
           status.num_block_in_progress != 0)
    {
        // Use all idle cells for reads:
        for (int cell_i = 0; cell_i < NUM_RING_ENTRIES; ++cell_i)
        {
            if (status.block_statuses[cell_i].stage == BLOCK_IDLE) prepare_read_request(&status, cell_i);
        }

        // Submit all unsubmitted reqs:
        io_uring_submit_and_wait(&status.io_ring, 1);

        int64_t cell_i = -1;
        do
        {
            struct io_uring_cqe* done_req;

            int ret = io_uring_peek_cqe(&status.io_ring, &done_req);
            if (ret == 0) cell_i = done_req->user_data;
            else          cell_i = -1;

            if (cell_i != -1 && status.block_statuses[cell_i].stage == BLOCK_IN_READ)
            {
                if (done_req->res == -1)
                {
                    LOG_ERROR("Read operation failed at offset: %lu", status.block_statuses[cell_i].offset);
                    exit(EXIT_FAILURE);
                }

                prepare_write_request(&status, cell_i);
            }
            else if (cell_i != -1 && status.block_statuses[cell_i].stage == BLOCK_IN_WRITE)
            {
                if (done_req->res == -1)
                {
                    LOG_ERROR("Write operation failed at offset: %lu", status.block_statuses[cell_i].offset);
                    exit(EXIT_FAILURE);
                }

                finish_write_request(&status, cell_i);
            } 

            io_uring_cqe_seen(&status.io_ring, done_req);
        }
        while (cell_i != -1);
    }

    // Deallocate resources:
    io_uring_queue_exit(&status.io_ring);

    if (fsync(dst_fd) == -1)
    {
        LOG_ERROR("Unable to sync file '%s': errno=%i (%s)", argv[2], errno, strerror(errno));
        exit(EXIT_FAILURE);
    }

    free_copying_status(&status);

    if (close(src_fd) == -1)
    {
        LOG_ERROR("Unable to close file '%s': errno=%i (%s)", argv[1], errno, strerror(errno));
        exit(EXIT_FAILURE);
    }

    if (close(dst_fd) == -1)
    {
        LOG_ERROR("Unable to close file '%s': errno=%i (%s)", argv[2], errno, strerror(errno));
        exit(EXIT_FAILURE);
    }
    
    return EXIT_SUCCESS;
}
