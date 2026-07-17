#pragma once
#include <stdint.h>

extern uint64_t time_base;
extern uint64_t pit_ticks;

typedef struct {
    uint32_t second;
    uint32_t minute;
    uint32_t hour;
    uint32_t day;
    uint32_t month;
    uint32_t year;
} datetime_t;

void pit_tick();
uint64_t pit_get_ticks();
void pit_set_frequency(uint32_t frequency);
uint64_t get_time();
uint64_t get_uptime_seconds();
uint64_t get_uptime_milliseconds();
void timer_init();

int is_leap(int year);
uint64_t to_unix_timestamp(datetime_t dt);
datetime_t to_datetime(uint64_t timestamp);
