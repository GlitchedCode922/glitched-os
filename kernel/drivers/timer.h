#pragma once
#include <stdint.h>

extern uint64_t time_base;
extern uint64_t pit_ticks;

void pit_tick();
uint64_t pit_get_ticks();
void pit_set_frequency(uint32_t frequency);
uint64_t get_time();
uint64_t get_uptime_seconds();
uint64_t get_uptime_milliseconds();
void timer_init();
