//
// Created by cem on 23.07.2018.
//

#ifndef DHCPCLIENT_DHCPC_H
#define DHCPCLIENT_DHCPC_H

#include <dhcp-packet.h>
#include <dhcpc-helpers.h>
#include <set>
#include <memory>
#include <vector>

#define verbose true
// #define DEBUG

class DHCPClient {
private:
  struct in_addr requested_address_;
  int requested_responses_ = 0;
  bool request_specific_address_;
  bool received_requested_address_;

  u_int32_t packet_xid_;
  std::string client_hardware_address_;
  int valid_responses_ = 0;

  u_int32_t dhcp_lease_time_ = 0;
  u_int32_t dhcp_renewal_time_ = 0;
  u_int32_t dhcp_rebinding_time_ = 0;

  std::vector<dhcp_offer> dhcp_offers_;
  std::vector<struct in_addr> requested_servers_;
  std::vector<struct in_addr> responding_servers_;

  struct in_addr selected_server_;

  struct dhcp_packet dhcp_packet_with_headers_set();


  int add_dhcp_offer(struct in_addr source,dhcp_packet* packet);

  void add_requested_server(struct in_addr server_address);

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

  int get_results();


};

#endif //DHCPCLIENT_DHCPC_H
