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

//#define DEBUG_PACKET



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
  bool request_specific_address_ = false;

  char ifname_[IFNAMSIZ];
  u_int8_t hwaddr_[IFHWADDRLEN];

  int listen_raw_sock_fd_;

  u_int32_t packet_xid_;

  dhcp_packet discovery_;

  std::vector<dhcp_packet> offers_;

  dhcp_packet request_;

  dhcp_packet acknowledge_;

  struct dhcp_packet dhcp_packet_with_headers_set();

public:
  DHCPClient(char* interface_name);

  void setRequestSpecificAddress(decltype(request_specific_address_) val) {request_specific_address_ = val;}
  auto getRequestSpecificAddress() -> decltype(request_specific_address_) { return request_specific_address_; }

  auto get_discovery() -> decltype(discovery_) { return discovery_; }
  auto get_offers() -> decltype(offers_) { return offers_; }
  auto get_request() -> decltype(request_) { return request_; }
  auto get_acknowledge() -> decltype(acknowledge_) { return acknowledge_; }


  int do_discover();

  void listen_offer();

  int do_request(struct in_addr server, struct in_addr requested);

  int listen_acknowledgement(struct in_addr server);

  void initialize();

  void cleanup();

  static struct lease gather_lease(char* interface_name);
};

#endif //DHCPCLIENT_DHCPC_H
