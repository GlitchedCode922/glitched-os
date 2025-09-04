#include <stdint.h>

#define ARP_HTYPE_ETHERNET 1
#define ARP_PTYPE_IPV4 0x0800
#define ARP_ETHERTYPE 0x0806
#define ARP_HLEN_ETHERNET 6
#define ARP_PLEN_IPV4 4
#define ARP_OPCODE_REQUEST 1
#define ARP_OPCODE_REPLY 2
#define ARP_CACHE_SIZE 256
#define ARP_ENTRY_TIMEOUT 300 // seconds
#define ARP_MAX_RETRIES 3
#define ARP_RETRY_INTERVAL 5 // seconds

typedef struct {
    uint16_t htype; // Hardware type
    uint16_t ptype; // Protocol type
    uint8_t hlen;   // Hardware address length
    uint8_t plen;   // Protocol address length
    uint16_t opcode; // Operation code (request/reply)
    uint8_t sender_mac[6]; // Sender hardware address
    uint8_t sender_ip[4];  // Sender protocol address
    uint8_t target_mac[6]; // Target hardware address
    uint8_t target_ip[4];  // Target protocol address
} __attribute__((packed)) arp_packet_t;

typedef struct {
    uint8_t ip[4];
    uint8_t mac[6];
} arp_entry_t;

void arp_reply(uint8_t* request_frame, int card);
void arp_request(uint8_t* target_ip, uint8_t* target_mac_buffer, uint8_t* src_ip, uint8_t* src_mac, int card);
