#include "arp.h"
#include "../drivers/net/rtl8139.h"
#include "ethernet.h"
#include "../memory/mman.h"
#include <stdint.h>

static arp_entry_t arp_cache[ARP_CACHE_SIZE] = {0};
static char ip[4] = {10, 0, 2, 15};

void arp_reply(uint8_t *request_frame, int card) {
    arp_packet_t* request = (arp_packet_t*)(request_frame); // Skip Ethernet header
    arp_packet_t reply;

    uint8_t* our_mac = rtl8139_get_mac_address(card);
    if (ntohs(request->opcode) != ARP_OPCODE_REQUEST) {
        return; // Not an ARP request
    }

    // Compare target IP with our IP
    if (memcmp(request->target_ip, ip, 4) != 0) {
        return; // Not for us
    }


    // Fill ARP reply fields
    reply.htype = htons(ARP_HTYPE_ETHERNET);
    reply.ptype = htons(ARP_PTYPE_IPV4);
    reply.hlen = ARP_HLEN_ETHERNET;
    reply.plen = ARP_PLEN_IPV4;
    reply.opcode = htons(ARP_OPCODE_REPLY);
    memcpy(reply.sender_mac, our_mac, 6);
    memcpy(reply.sender_ip, request->target_ip, 4);
    memcpy(reply.target_mac, request->sender_mac, 6);
    memcpy(reply.target_ip, request->sender_ip, 4);

    // Send the ARP reply
    send_ethernet((char *)reply.sender_mac, (char *)reply.target_mac, ARP_ETHERTYPE, (uint8_t *)&reply, sizeof(arp_packet_t), card);
}