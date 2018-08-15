//
// Created by cem on 23.07.2018.
//

#ifndef DHCPCLIENT_DHCP_REQUEST_H
#define DHCPCLIENT_DHCP_REQUEST_H

#include <cstdio>
#include <dhcp.h>
#include <vector>
#include <dhcp-client.h>

/**
 * @brief Prints packet just for debugging
 * @param data Data to be printed
 * @param len lenght of the data
 */
void print_packet(const u_int8_t *data, int len);

/**
 * @brief Sends packets with specified values over socket
 * @param buffer value to send to socket
 * @param buffer_size size of the package
 * @param sock socket file descriptor
 * @param address information in sockaddr_in struct
 * @return Success value: -1 if fails 0 if succeed
 */
int
send_dhcp_packet(void *buffer, int buffer_size, char *ifname);

/**
 * @brief Receives packet with specified values
 * \param packet to be written
 * \param packet_size buffer size to be written in buffer
 * \param sock socker file descriptor
 * \param timeout in seconds
 * \param address address to be received
 * @return Success value: -1 if fails 0 if succeed
 */
bool receive_dhcp_packet(int sock, void *packet, int packet_size, int timeout);

/**
 * @brief Adds option to dhcp packet with given offset
 * @param packet
 * @param code
 * @param data
 * @param offset
 * @param len
 * @return
 */
int add_dhcp_option(struct dhcp_packet* packet, u_int8_t code, u_int8_t* data, int offset, u_int8_t len);

/**
 * @brief Specialized add dhcp option function that puts end flag to end of the options
 * @param packet
 * @param offset
 */
void end_dhcp_option(struct dhcp_packet* packet, int offset);


/**
 * @brief Checks if packet is invalid or not
 * @param packet
 * @return true if valid, else false
 */
bool validate_packet(struct dhcp_packet* packet);
/**
 * @brief Parses the packet and put it in the vector
 * @param packet pointer
 * @return objects
 */
std::vector<dhcp_option> parse_dhcp_packet(struct dhcp_packet *packet);

#endif //DHCPCLIENT_DHCP_REQUEST_H
