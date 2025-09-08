#include "arp.h"
#include "../drivers/net.h"
#include "ethernet.h"
#include "../memory/mman.h"
#include <stdint.h>

static arp_entry_t arp_cache[ARP_CACHE_SIZE] = {0};

static int waiting_for_reply = 0;

void arp_reply(uint8_t *request_frame, int card) {
    arp_packet_t* request = (arp_packet_t*)(request_frame); // Skip Ethernet header
    arp_packet_t reply;

    uint8_t ip[4];
    get_ip(card, (uint32_t*)ip);

    uint8_t our_mac[6]; 
    get_mac(card, our_mac);
    if (waiting_for_reply && ntohs(request->opcode) == ARP_OPCODE_REPLY) {
        // Update ARP cache
        for (int i = 0; i < ARP_CACHE_SIZE; i++) {
            if (arp_cache[i].ip[0] == 0 && arp_cache[i].ip[1] == 0 &&
                arp_cache[i].ip[2] == 0 && arp_cache[i].ip[3] == 0) {
                memcpy(arp_cache[i].ip, request->sender_ip, 4);
                memcpy(arp_cache[i].mac, request->sender_mac, 6);
                break;
            }
        }
        waiting_for_reply = 0; // Reset the flag
    }

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

void arp_request(uint8_t* target_ip, uint8_t* target_mac_buffer, int card) {
    // Check ARP cache first
    for (int i = 0; i < ARP_CACHE_SIZE; i++) {
        if (memcmp(arp_cache[i].ip, target_ip, 4) == 0) {
            memcpy(target_mac_buffer, arp_cache[i].mac, 6);
            return; // MAC address found in cache
        }
    }

    arp_packet_t request;
    uint8_t our_mac[6];
    get_mac(card, our_mac);

    uint8_t ip[4];
    get_ip(card, (uint32_t*)ip);
    // Fill ARP request fields
    request.htype = htons(ARP_HTYPE_ETHERNET);
    request.ptype = htons(ARP_PTYPE_IPV4);
    request.hlen = ARP_HLEN_ETHERNET;
    request.plen = ARP_PLEN_IPV4;
    request.opcode = htons(ARP_OPCODE_REQUEST);
    memcpy(request.sender_mac, our_mac, 6);
    memcpy(request.sender_ip, ip, 4);
    memset(request.target_mac, 0, 6); // Target MAC is unknown
    memcpy(request.target_ip, target_ip, 4);

    // Padding to meet minimum Ethernet frame size
    uint8_t padded_frame[60 - 14] = {0};
    memcpy(padded_frame, &request, sizeof(arp_packet_t));
    
    waiting_for_reply = 1;
    send_ethernet((char *)request.sender_mac, BROADCAST_MAC, ARP_ETHERTYPE, padded_frame, 60 - 14, card);

    while (waiting_for_reply); // Wait for the reply (blocking)

    // After receiving the reply, the MAC address should be in the cache
    for (int i = 0; i < ARP_CACHE_SIZE; i++) {
        if (memcmp(arp_cache[i].ip, target_ip, 4) == 0) {
            memcpy(target_mac_buffer, arp_cache[i].mac, 6);
            return; // MAC address found in cache
        }
    }

    // If we reach here, the MAC address was not found (should not happen)
    memset(target_mac_buffer, 0, 6); // Indicate failure
    return;
}
