#pragma once
#include <stdint.h>

int is_leap(int year);
uint64_t unix_timestamp(int year, int month, int day, int hour, int minute, int second);
uint64_t get_timestamp();
