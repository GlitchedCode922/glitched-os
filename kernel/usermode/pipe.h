#pragma once
#include <stdint.h>

#define PIPE_SIZE 8184 // Pipe size for the struct to be exactly 2 pages long

typedef struct {
    uint16_t read_head;
    uint16_t write_head;
    uint16_t reader_count;
    uint16_t writer_count;
    uint8_t buffer[PIPE_SIZE];
} pipe_t;

pipe_t* create_pipe();
void remove_pipe_reader(pipe_t* pipe);
void remove_pipe_writer(pipe_t* pipe);
int64_t pipe_read(pipe_t* pipe, void* buffer, uint64_t len);
int64_t pipe_write(pipe_t* pipe, const void* buffer, uint64_t len);
