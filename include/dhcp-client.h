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

#define verbose true
// #define DEBUG

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

typedef struct dhcp_option{
  u_int8_t length;
  u_int8_t type;
  u_int8_t* data;
} dhcp_option;

class DHCPClient {
private:
  struct in_addr requested_address_;
  int requested_responses_ = 0;
  bool request_specific_address_ = false;
  bool received_requested_address_;

  u_int32_t packet_xid_;
  std::string client_hardware_address_;
  int valid_responses_ = 0;

  std::vector<dhcp_offer> dhcp_offers_;
  std::vector<struct in_addr> requested_servers_;
  std::vector<struct in_addr> responding_servers_;

  dhcp_packet discovery;

  std::vector<dhcp_packet> offer;

  dhcp_packet request;

  dhcp_packet acknowledge;

  struct in_addr selected_server_;

  struct dhcp_packet dhcp_packet_with_headers_set();

  int add_dhcp_offer(struct in_addr source,dhcp_packet* packet);

  void free_dhcp_offer_list(void);

  int free_requested_server_list(void);

public:

  auto getDhcpOffers() -> decltype(dhcp_offers_) { return dhcp_offers_; }

  void setRequestSpecificAddress(decltype(request_specific_address_) val) {request_specific_address_ = val;}
  auto getRequestSpecificAddress() -> decltype(request_specific_address_) { return request_specific_address_; }

  void set_packet_xid(decltype(packet_xid_) xid ){ packet_xid_ = xid;}

  void set_client_hardware_address(decltype(client_hardware_address_) addr) { client_hardware_address_ = addr;}

  int send_dhcp_discover(int fd);

  std::vector<dhcp_packet> get_dhcp_offer(int fd);

  int send_dhcp_request(int fd, struct in_addr server, struct in_addr requested);

  int get_dhcp_acknowledgement(int, struct in_addr server);

};

#endif //DHCPCLIENT_DHCPC_H
