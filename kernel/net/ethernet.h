#include <stdint.h>

#define BROADCAST_MAC "\xff\xff\xff\xff\xff\xff"

uint16_t htons(uint16_t val);
uint16_t ntohs(uint16_t val);

void frame_received(int card);
void send_ethernet(char* src_mac, char* dst_mac, uint16_t ethertype, uint8_t* payload, int payload_length, int card);
