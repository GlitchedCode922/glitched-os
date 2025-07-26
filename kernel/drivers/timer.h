#pragma once

#include <stdint.h>

void pit_tick();
uint64_t pit_get_ticks();
void pit_set_frequency(uint32_t frequency);
uint64_t get_uptime_seconds();
uint64_t get_uptime_milliseconds();
void timer_init();
