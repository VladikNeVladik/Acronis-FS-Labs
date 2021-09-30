// No copyright. 2020, Vladislav Aleinik
#ifndef IO_RING_COPY_USERSPACE_RING_H
#define IO_RING_COPY_USERSPACE_RING_H

#include <stdlib.h>
#include <stdint.h>
#include <memory.h>

#include <sys/uio.h>
#include <unistd.h>
#include <signal.h>
#include <sys/syscall.h>
#include <sys/mman.h>

#include "io_uring.h"

#include "Logging.h"

//===========
// Constants 
//===========

// IO-uring syscall numbers:
const long NR_io_uring_setup    = 425;
const long NR_io_uring_enter    = 426;
const long NR_io_uring_register = 427;

//=================
// Data Structures 
//=================

struct IO_RingSQ
{
    unsigned* head;
    unsigned* tail;
    unsigned* ring_mask;
    unsigned* ring_entries;
    unsigned* flags;
    unsigned* dropped;

    struct io_uring_sqe* sq_entries;
    uint32_t* sq_ring;
};

struct IO_RingCQ
{
    unsigned* head;
    unsigned* tail;
    unsigned* ring_mask;
    unsigned* ring_entries;
    unsigned* overflow;

    struct io_uring_cqe* cq_ring;
};

struct IO_Ring
{
    int fd;

    struct IO_RingSQ sq;
    struct IO_RingCQ cq;

    unsigned num_entries;
    unsigned num_to_submit;
};

//=========================
// Init, Register And Free 
//=========================

void init_io_ring(struct IO_Ring* io_ring, uint32_t num_entries)
{
    // Set IO-userspace-ring parameters to defaults:
    struct io_uring_params params;
    memset(&params, 0, sizeof(params));

    // Get an IO-ring:
    int io_ring_fd = syscall(NR_io_uring_setup, num_entries, &params);
    if (io_ring_fd == -1)
    {
        LOG_ERROR("[init_io_ring] Unable to setup IO-ring");
    }

    io_ring->fd          = io_ring_fd;
    io_ring->num_entries = num_entries;

    // Map IO-ring submission queue:
    void* sq_ring_ptr = mmap(NULL, params.sq_off.array + params.sq_entries * sizeof(uint32_t),
                             PROT_READ|PROT_WRITE, MAP_SHARED|MAP_POPULATE,
                             io_ring_fd, IORING_OFF_SQ_RING);
    if (sq_ring_ptr == MAP_FAILED)
    {
        LOG_ERROR("[init_io_ring] Unable to map IO-ring submission queue to userspace");
    }

    // Map submission entry array:
    void* sq_entries_ptr = mmap(NULL, params.sq_entries * sizeof(struct io_uring_sqe),
                                PROT_READ|PROT_WRITE, MAP_SHARED|MAP_POPULATE,
                                io_ring_fd, IORING_OFF_SQES);
    if (sq_entries_ptr == MAP_FAILED)
    {
        LOG_ERROR("[init_io_ring] Unable to map IO-ring entry array to userspace");
    }

    io_ring->num_to_submit = 0;

    // Map IO-ring completion queue:
    void* cq_ring_ptr = mmap(NULL, params.cq_off.cqes + params.cq_entries * sizeof(struct io_uring_cqe),
                             PROT_READ|PROT_WRITE, MAP_SHARED|MAP_POPULATE,
                             io_ring_fd, IORING_OFF_CQ_RING);
    if (cq_ring_ptr == MAP_FAILED)
    {
        LOG_ERROR("[init_io_ring] Unable to map IO-ring completion queue to userspace");
    }

    // Calculate pointers:
    io_ring->sq.head         = sq_ring_ptr + params.sq_off.head;
    io_ring->sq.tail         = sq_ring_ptr + params.sq_off.tail;
    io_ring->sq.ring_mask    = sq_ring_ptr + params.sq_off.ring_mask;
    io_ring->sq.ring_entries = sq_ring_ptr + params.sq_off.ring_entries;
    io_ring->sq.flags        = sq_ring_ptr + params.sq_off.flags;
    io_ring->sq.sq_ring      = sq_ring_ptr + params.sq_off.array;
    io_ring->sq.dropped      = sq_ring_ptr + params.sq_off.dropped;

    io_ring->sq.sq_entries   = sq_entries_ptr;

    io_ring->cq.head         = cq_ring_ptr + params.cq_off.head;
    io_ring->cq.tail         = cq_ring_ptr + params.cq_off.tail;
    io_ring->cq.ring_mask    = cq_ring_ptr + params.cq_off.ring_mask;
    io_ring->cq.ring_entries = cq_ring_ptr + params.cq_off.ring_entries;
    io_ring->cq.overflow     = cq_ring_ptr + params.cq_off.overflow;
    io_ring->cq.cq_ring      = cq_ring_ptr + params.cq_off.cqes;

    // Preconfigure SQ-entries:
    for (unsigned i = 0; i < *io_ring->sq.ring_entries; ++i)
    {
        io_ring->sq.sq_entries[i].flags     = 0;
        io_ring->sq.sq_entries[i].ioprio    = 0; // Default priority
        io_ring->sq.sq_entries[i].rw_flags  = 0;
        io_ring->sq.sq_entries[i].user_data = i;
    }

    LOG("IO-ring initialised");
}

void register_io_buffers(struct IO_Ring* io_ring, struct iovec* buffers, uint32_t num_buffers)
{
    if (syscall(NR_io_uring_register, io_ring->fd, IORING_REGISTER_BUFFERS, buffers, num_buffers) == -1)
    {
        LOG_ERROR("[register_io_buffers] Unable to register IO buffers");
    }

    // Preconfigure SQ-entries:
    for (unsigned i = 0; i < *io_ring->sq.ring_entries; ++i)
    {
        io_ring->sq.sq_entries[i].addr      = (int64_t) buffers[i].iov_base;
        io_ring->sq.sq_entries[i].buf_index = i;
    }

    LOG("Registered IO-buffers for IO-ring");
}

void free_io_ring(struct IO_Ring* io_ring)
{
    if (close(io_ring->fd) == -1)
    {
        LOG_ERROR("[free_io_ring] Unable to close IO-ring");
    }

    LOG("IO-ring freed");
}

//================================
// Compiler Optimization Barriers
//================================

#define WRITE_ONCE(var, val) (*((volatile __typeof(var) *)(&(var))) = (val))
#define READ_ONCE(var)       (*((volatile __typeof(var) *)(&(var))))

//=====================
// CPU Memory Barriers
//=====================

#if defined(__x86_64) || defined(__i386__)
#define memory_barrier() __asm__ __volatile__("":::"memory")
#else
#define memory_barrier() __sync_synchronize()
#endif

//===============
// IO Submission 
//===============

int enqueue_io_request(struct IO_Ring* io_ring,
                      int opcode, int fd, uint16_t cell, uint64_t off, uint32_t len)
{
    unsigned tail = READ_ONCE(*io_ring->sq.tail);

    // Ensure the update to the sq.head have propagated to this CPU:
    memory_barrier();
    unsigned head = READ_ONCE(*io_ring->sq.head);

    BUG_ON(tail - head == io_ring->num_entries, "IO-ring full");

    // Configure the SQ-ring entry:
    WRITE_ONCE(io_ring->sq.sq_entries[cell].opcode, opcode);
    WRITE_ONCE(io_ring->sq.sq_entries[cell].fd    ,     fd);
    WRITE_ONCE(io_ring->sq.sq_entries[cell].off   ,    off);
    WRITE_ONCE(io_ring->sq.sq_entries[cell].len   ,    len);
    WRITE_ONCE(io_ring->sq.sq_entries[cell].flags ,      0);

    WRITE_ONCE(io_ring->sq.sq_ring[tail & *io_ring->sq.ring_mask], cell);

    tail += 1;

    // Ensure the kernel sees the sq-entries update before the tail update:
    memory_barrier();
    WRITE_ONCE(*io_ring->sq.tail, tail);

    io_ring->num_to_submit += 1;

    LOG("Enqueued IO-request {op=%s, fd=%d, off=%lu, len=%u} on cell#%u",
        (opcode == IORING_OP_READ_FIXED)? "read" : "write", fd, off, len, cell);

    return 0;
}

void submit_and_wait(struct IO_Ring* io_ring)
{
    // Ensure all the tail updates are propagated to the kernel CPU:
    memory_barrier();

    LOG("Submitted %d enqueued requests", io_ring->num_to_submit);

    int ios_submitted = syscall(NR_io_uring_enter, io_ring->fd, io_ring->num_to_submit, 1, IORING_ENTER_GETEVENTS, NULL, _NSIG/8);
    if (ios_submitted != io_ring->num_to_submit)
    {
        LOG_ERROR("[submit_io_requests] Unable to submit request to IO-ring submission queue");
    }

    io_ring->num_to_submit = 0;
}

//===============
// IO Completion 
//===============

struct io_uring_cqe get_one_completed_io(struct IO_Ring* io_ring)
{
    struct io_uring_cqe to_return =
    {
        .user_data = -1,
        .res       = -1,
        .flags     = -1
    };

    unsigned head = READ_ONCE(*io_ring->cq.head);

    // Ensure the updates to the tail have propagated to this CPU:
    memory_barrier();
    unsigned tail = READ_ONCE(*io_ring->cq.tail);

    if (head != tail)
    {
        to_return = READ_ONCE(io_ring->cq.cq_ring[head & *io_ring->cq.ring_mask]);

        // Ensure the read is performed before the head update:
        memory_barrier();
        WRITE_ONCE(*io_ring->cq.head, head + 1);
    }

    return to_return;
}

#endif // IO_RING_COPY_USERSPACE_RING_H
