#include "dhcp.h"
#include "udp.h"
#include "ip.h"
#include "ethernet.h"
#include "../drivers/net.h"
#include "../memory/mman.h"
#include <stdint.h>


int waiting_for_dhcp = 0;
dhcp_packet_t dhcp_offer_packet;

extern int broadcast_card;

void dhcp_run(int card) {
    static uint32_t transaction_id = 0x12345678; // Example transaction ID
    static uint8_t client_mac[6];
    static uint8_t offered_ip[4];
    static uint8_t dhcp_server_ip[4];

    int broadcast_card_old = broadcast_card;
    broadcast_card = card; // Set the broadcast card to the current card

    // Get MAC address of the network card
    uint8_t mac[6]; 
    get_mac(card, mac);
    memcpy(client_mac, mac, 6);

    // Construct DHCP DISCOVER packet
    dhcp_packet_t dhcp_discover = {0};
    dhcp_discover.op = 1; // BOOTREQUEST
    dhcp_discover.htype = 1; // Ethernet
    dhcp_discover.hlen = 6; // MAC address length
    dhcp_discover.hops = 0;
    dhcp_discover.xid = htonl(transaction_id);
    dhcp_discover.secs = 0;
    dhcp_discover.flags = htons(0x8000); // Broadcast flag
    memcpy(dhcp_discover.chaddr, client_mac, 6);
    dhcp_discover.magic_cookie = htonl(DHCP_MAGIC_COOKIE);
    uint8_t* options = dhcp_discover.options;
    options[0] = DHCP_OPTION_MESSAGE_TYPE;
    options[1] = 1;
    options[2] = DHCP_DISCOVER;
    options[3] = DHCP_OPTION_PARAMETER_REQUEST_LIST;
    options[4] = 3; // Length
    options[5] = DHCP_OPTION_SUBNET_MASK;
    options[6] = DHCP_OPTION_ROUTER;
    options[7] = DHCP_OPTION_DNS;
    options[8] = DHCP_OPTION_END;

    // Register UDP listener for DHCP responses
    register_udp_listener(DHCP_CLIENT_PORT, dhcp_listener);

    // Send DHCP DISCOVER
    waiting_for_dhcp = 1;
    udp_send(IP_BROADCAST_ADDR, DHCP_CLIENT_PORT, DHCP_SERVER_PORT, (uint8_t*)&dhcp_discover, sizeof(dhcp_packet_t));
    // Wait for DHCP OFFER
    while (waiting_for_dhcp);

    // Parse DHCP OFFER packet
    memcpy(offered_ip, dhcp_offer_packet.yiaddr, 4);
    // Extract DHCP server IP from options
    uint8_t* opt_ptr = dhcp_offer_packet.options;
    while (*opt_ptr != DHCP_OPTION_END) {
        if (*opt_ptr == DHCP_OPTION_SERVER_IDENTIFIER && *(opt_ptr + 1) == 4) {
            memcpy(dhcp_server_ip, opt_ptr + 2, 4);
            break;
        }
        opt_ptr += 2 + *(opt_ptr + 1);
    }
    // Construct DHCP REQUEST packet
    dhcp_packet_t dhcp_request = {0};
    dhcp_request.op = 1; // BOOTREQUEST
    dhcp_request.htype = 1; // Ethernet
    dhcp_request.hlen = 6; // MAC address length
    dhcp_request.hops = 0;
    dhcp_request.xid = htonl(transaction_id);
    dhcp_request.secs = 0;
    dhcp_request.flags = htons(0x8000); // Broadcast flag
    memcpy(dhcp_request.chaddr, client_mac, 6);
    dhcp_request.magic_cookie = htonl(DHCP_MAGIC_COOKIE);
    options = dhcp_request.options;
    options[0] = DHCP_OPTION_MESSAGE_TYPE;
    options[1] = 1;
    options[2] = DHCP_REQUEST;
    options[3] = DHCP_OPTION_REQUESTED_IP;
    options[4] = 4;
    memcpy(&options[5], offered_ip, 4);
    options[9] = DHCP_OPTION_SERVER_IDENTIFIER;
    options[10] = 4;
    memcpy(&options[11], dhcp_server_ip, 4);
    options[15] = DHCP_OPTION_PARAMETER_REQUEST_LIST;
    options[16] = 3; // Length
    options[17] = DHCP_OPTION_SUBNET_MASK;
    options[18] = DHCP_OPTION_ROUTER;
    options[19] = DHCP_OPTION_DNS;
    options[20] = DHCP_OPTION_END;
    // Send DHCP REQUEST
    waiting_for_dhcp = 1;
    udp_send(IP_BROADCAST_ADDR, DHCP_CLIENT_PORT, DHCP_SERVER_PORT, (uint8_t*)&dhcp_request, sizeof(dhcp_packet_t));
    // Wait for DHCP ACK
    while (waiting_for_dhcp);

    uint8_t ip[4];
    uint8_t subnet_mask[4];
    uint8_t router_ip[4];
    // Parse DHCP ACK packet
    memcpy(ip, dhcp_offer_packet.yiaddr, 4);
    // Extract subnet mask and router from options
    opt_ptr = dhcp_offer_packet.options;
    while (*opt_ptr != DHCP_OPTION_END) {
        if (*opt_ptr == DHCP_OPTION_SUBNET_MASK && *(opt_ptr + 1) == 4) {
            memcpy(subnet_mask, opt_ptr + 2, 4);
        } else if (*opt_ptr == DHCP_OPTION_ROUTER && *(opt_ptr + 1) >= 4) {
            memcpy(router_ip, opt_ptr + 2, 4);
        }
        opt_ptr += 2 + *(opt_ptr + 1);
    }

    // Configure network interface with obtained IP, subnet mask, and router
    configure_network_interface_static(card, *(uint32_t*)ip, *(uint32_t*)subnet_mask, *(uint32_t*)router_ip);

    unregister_udp_listener(DHCP_CLIENT_PORT);
    broadcast_card = broadcast_card_old; // Restore the original broadcast card
    return;
}

void dhcp_listener(uint8_t* sender_ip, uint16_t sender_port, uint8_t* data, int len) {
    if (len > sizeof(dhcp_packet_t)) return;
    dhcp_packet_t* packet = (dhcp_packet_t*)data;
    if (packet->xid != htonl(0x12345678)) return; // Not our transaction ID
    memset(&dhcp_offer_packet, 0, sizeof(dhcp_packet_t));
    memcpy(&dhcp_offer_packet, packet, len);
    waiting_for_dhcp = 0;
}
