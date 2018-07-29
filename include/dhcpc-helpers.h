//
// Created by cem on 23.07.2018.
//

#ifndef DHCPCLIENT_DHCPC_HELPERS_H
#define DHCPCLIENT_DHCPC_HELPERS_H

#include <netinet/in.h>
#include <string>

typedef struct dhcp_offer_struct{
  struct in_addr server_address;   /* address of DHCP server that sent this offer */
  struct in_addr offered_address;  /* the IP address that was offered to us */
  u_int32_t lease_time;            /* lease time in seconds */
  u_int32_t renewal_time;          /* renewal time in seconds */
  u_int32_t rebinding_time;        /* rebinding time in seconds */
} dhcp_offer;

int create_dhcp_socket(std::string interface_name);

void close_dhcp_socket(int);

int get_hardware_address(std::string interface_name, std::string *result);


#endif //DHCPCLIENT_DHCPC_HELPERS_H
