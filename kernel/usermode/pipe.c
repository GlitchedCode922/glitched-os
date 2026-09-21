#include "usermode/pipe.h"
#include "memory/mman.h"
#include "usermode/scheduler.h"
#include "error.h"
#include <stdint.h>

pipe_t* create_pipe() {
    pipe_t* ret = kmalloc(sizeof(pipe_t));
    ret->reader_count = 1;
    ret->writer_count = 1;
    return ret;
}

void remove_pipe_reader(pipe_t* pipe) {
    pipe->reader_count--;
    if (pipe->reader_count == 0 && pipe->writer_count == 0) kfree(pipe);
}

void remove_pipe_writer(pipe_t* pipe) {
    pipe->writer_count--;
    if (pipe->reader_count == 0 && pipe->writer_count == 0) kfree(pipe);
}

int64_t pipe_read(pipe_t* pipe, void* buffer, uint64_t len) {
    uint64_t max_bytes = 0;
    if (pipe->write_head >= pipe->read_head) {
        while (pipe->write_head == pipe->read_head) {
            if (pipe->writer_count == 0) return 0;
            yield_current();
        }
        max_bytes = pipe->write_head - pipe->read_head;
        if (len > max_bytes) len = max_bytes;
        memcpy(buffer, pipe->buffer + pipe->read_head, len);
        pipe->read_head += len;
        return len;
    } else {
        max_bytes = (PIPE_SIZE - pipe->read_head + pipe->write_head) % PIPE_SIZE;
        if (len > max_bytes) len = max_bytes;
        uint64_t first_part_length = len < PIPE_SIZE - pipe->read_head ? len : PIPE_SIZE - pipe->read_head;
        memcpy(buffer, pipe->buffer + pipe->read_head, first_part_length);
        pipe->read_head = (pipe->read_head + first_part_length) % PIPE_SIZE;
        if (first_part_length == len) return len;
        memcpy(buffer + first_part_length, pipe->buffer, len - first_part_length);
        pipe->read_head = (pipe->read_head + len - first_part_length) % PIPE_SIZE;
        return len;
    }
}

int64_t pipe_write(pipe_t* pipe, const void* buffer, uint64_t len) {
    int64_t max_bytes = (pipe->read_head - pipe->write_head - 1 + PIPE_SIZE) % PIPE_SIZE;
    int64_t remaining = 0;
    if (len > max_bytes) {
        remaining = len - max_bytes;
        len -= remaining;
    }
    if (pipe->reader_count == 0) return -EPIPE;
    if (pipe->read_head > pipe->write_head) {
        memcpy(pipe->buffer + pipe->write_head, buffer, len);
        pipe->write_head += len;
    } else {
        int64_t first_part_length = len < PIPE_SIZE - pipe->write_head ? len : PIPE_SIZE - pipe->write_head;
        memcpy(pipe->buffer + pipe->write_head, buffer, first_part_length);
        pipe->write_head = (pipe->write_head + first_part_length) % PIPE_SIZE;
        if (first_part_length == len) goto write_end;
        memcpy(pipe->buffer, buffer + first_part_length, len - first_part_length);
        pipe->write_head = (pipe->write_head + len - first_part_length) % PIPE_SIZE;
    }
    write_end:
    if (remaining) {
        int64_t available = (pipe->read_head - pipe->write_head - 1 + PIPE_SIZE) % PIPE_SIZE;
        while (!available) {
            available = (pipe->read_head - pipe->write_head - 1 + PIPE_SIZE) % PIPE_SIZE;
            if (pipe->reader_count == 0) return -EPIPE;
            yield_current();
        }
        return len + pipe_write(pipe, buffer + len, remaining);
    }
    return len;
}
