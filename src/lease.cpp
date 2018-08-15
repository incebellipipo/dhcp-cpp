#include <iostream>
#include <lease.h>
#include <dhcp-packet.h>
#include <cstring>

struct lease process_lease(struct dhcp_packet* packet){

  struct lease l = {};
  bzero(&l, sizeof(l));
  l.valid = true;
  auto options = parse_dhcp_packet(packet);
  for(auto option : options) {
    if (option.type == DHO_DOMAIN_NAME_SERVERS) {
      for (int i = 0; i < option.length; i += sizeof(struct in_addr)) {
        struct in_addr dns = {};
        memcpy(&dns, option.data + i, sizeof(dns));
        l.domain_name_servers.push_back(dns);
      }
    } else if (option.type == DHO_DHCP_SERVER_IDENTIFIER) {
      memcpy(&l.server_identifier, option.data, option.length);
    } else if (option.type == DHO_SUBNET_MASK) {
      memcpy(&l.subnet_mask, option.data, option.length);
    } else if (option.type == DHO_ROUTERS) {
      memcpy(&l.routers, option.data,
             sizeof(l.routers)); // im not so sure why it is names routerS. Staying in safe zone.
    } else if (option.type == DHO_DHCP_LEASE_TIME) {
      memcpy(&l.lease_time, option.data, option.length);
      l.lease_time = htonl((u_int32_t) l.lease_time);
    } else if (option.type == DHO_DHCP_RENEWAL_TIME) {
      memcpy(&l.renew, option.data, option.length);
      l.renew = htonl((u_int32_t) l.renew);
    } else if (option.type == DHO_DHCP_REBINDING_TIME) {
      memcpy(&l.rebind, option.data, option.length);
      l.rebind = htonl((u_int32_t) l.rebind);
    } else if (option.type == DHO_DHCP_MESSAGE_TYPE) {
      memcpy(&l.message_type, option.data, option.length);
    }
  }

  memcpy(&l.address, &packet->yiaddr, sizeof(l.address));
  if(l.renew == 0){
    l.renew = l.lease_time / 2;
  }

  return l;
}