//
// Created by cem on 23.07.2018.
//

#ifndef DHCPCLIENT_DHCPC_H
#define DHCPCLIENT_DHCPC_H

#include <dhcp-packet.h>
#include <set>
#include <memory>
#include <vector>
#include <netinet/in.h>
#include <string>
#include <net/if.h>

#define DEBUG_PACKET

typedef struct dhcp_offer_struct{
  struct in_addr server_address;   /* address of DHCP server that sent this offer */
  struct in_addr offered_address;  /* the IP address that was offered to us */
  u_int32_t lease_time;            /* lease time in seconds */
  u_int32_t renewal_time;          /* renewal time in seconds */
  u_int32_t rebinding_time;        /* rebinding time in seconds */
} dhcp_offer;

typedef struct udp_psedoheader {
  uint32_t srcaddr;
  uint32_t dstaddr;
  uint8_t zero;
  uint8_t protocol;
  uint16_t length;
} udp_psedoheader;


void close_dhcp_socket(int);

int get_hardware_address(std::string interface_name, std::string *result);

typedef struct dhcp_option{
  u_int8_t length;
  u_int8_t type;
  u_int8_t* data;
} dhcp_option;

typedef struct dhcp_result{

} dhcp_result;

class DHCPClient {
private:
  struct in_addr requested_address_;
  int requested_responses_ = 0;
  bool request_specific_address_ = false;
  bool received_requested_address_;

  char ifname_[IFNAMSIZ];
  u_int8_t hwaddr_[IFHWADDRLEN];

  int valid_responses_ = 0;

  int listen_raw_sock_fd_;

  u_int32_t packet_xid_;
  std::vector<struct in_addr> requested_servers_;
  std::vector<struct in_addr> responding_servers_;

  dhcp_packet discovery_;

  std::vector<dhcp_packet> offers_;

  dhcp_packet request_;

  dhcp_packet acknowledge_;

  struct in_addr selected_server_;

  struct dhcp_packet dhcp_packet_with_headers_set();

  int add_dhcp_offer(struct in_addr source,dhcp_packet* packet);

public:
  DHCPClient(char* interface_name);

  void setRequestSpecificAddress(decltype(request_specific_address_) val) {request_specific_address_ = val;}
  auto getRequestSpecificAddress() -> decltype(request_specific_address_) { return request_specific_address_; }

  int send_dhcp_discover();

  void get_dhcp_offer();

  auto getOffers() -> decltype(offers_) { return offers_; }

  int send_dhcp_request(struct in_addr server, struct in_addr requested);

  int get_dhcp_acknowledgement(struct in_addr server);

  void initialize();

  void cleanup();

};

#endif //DHCPCLIENT_DHCPC_H
