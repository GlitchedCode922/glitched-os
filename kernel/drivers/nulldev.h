#include <stdint.h>

int64_t null_read(int minor_number, uint64_t offset, uint8_t *buffer, uint64_t size);
int64_t null_write(int minor_number, uint64_t offset, const uint8_t *buffer, uint64_t size);
int register_null_devices();
