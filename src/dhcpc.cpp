//
// Created by cem on 23.07.2018.
//
#include <iostream>
#include <cstring>
#include <cstdlib>
#include <cstdio>
#include <ctime>
#include <climits>

#include <unistd.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <arpa/inet.h>
#include <netinet/in.h>


#include <dhcp.h>
#include <dhcp-packet.h>
#include <dhcpc.h>
#include <dhcpc-helpers.h>

#include <net/if.h>

struct dhcp_packet DHCPClient::dhcp_packet_with_headers_set() {
  struct dhcp_packet packet;

  // clear the packet data structure
  bzero(&packet, sizeof(packet));

  // hardware address type
  packet.htype = HTYPE_ETHER;

  // length of our hardware address
  packet.hlen = IFHWADDRLEN;

  packet.hops = 0;

  // transaction id is supposed to be random
  srand((unsigned int)time(nullptr));

  packet.xid = htonl(packet_xid_);

  packet.secs = htons(USHRT_MAX);

  packet.flags = htons(BOOTP_BROADCAST);


  memcpy(packet.chaddr, client_hardware_address_.c_str(), IFHWADDRLEN);

  return packet;
}

/* sends a DHCPDISCOVER broadcast message in an attempt to find DHCP servers */
int DHCPClient::send_dhcp_discover(int sock){
  auto discover_packet = dhcp_packet_with_headers_set();
  struct sockaddr_in sockaddr_broadcast;

  int offset = 0;
  discover_packet.options[offset++] = 0x63;
  discover_packet.options[offset++] = 0x82;
  discover_packet.options[offset++] = 0x53;
  discover_packet.options[offset++] = 0x63;

  u_int8_t option;

  discover_packet.op = BOOTREQUEST;
  offset += add_dhcp_option(&discover_packet,
                            DHO_DHCP_MESSAGE_TYPE,
                            &(option = DHCPDISCOVER),
                            offset,
                            sizeof(option)
                            );

  /* the IP address we're requesting */
  if(request_specific_address_){
    offset += add_dhcp_option(&discover_packet,
                              DHO_DHCP_REQUESTED_ADDRESS,
                              (u_int8_t*)&requested_address_,
                              offset,
                              sizeof(requested_address_)
                              );
  }

  char hostname[1024];
  hostname[1023] = 0x00;
  gethostname(hostname, 1023);
  offset += add_dhcp_option(&discover_packet,
                            DHO_HOST_NAME,
                            (u_int8_t*)&hostname,
                            offset,
                            (size_t) strlen(hostname)
                            );

  u_int8_t parameter_requst_list[] = {DHO_SUBNET_MASK,
                                      DHO_BROADCAST_ADDRESS,
                                      DHO_TIME_OFFSET,
                                      DHO_ROUTERS,
                                      DHO_DOMAIN_NAME,
                                      DHO_DOMAIN_NAME_SERVERS,
                                      DHO_HOST_NAME,
                                      DHO_NETBIOS_NAME_SERVERS,
                                      DHO_INTERFACE_MTU,
                                      DHO_STATIC_ROUTES,
                                      DHO_NTP_SERVERS };

  offset += add_dhcp_option(&discover_packet,
                            DHO_DHCP_PARAMETER_REQUEST_LIST,
                            (u_int8_t*)&parameter_requst_list,
                            offset,
                            sizeof(parameter_requst_list)
                            );

  end_dhcp_option(&discover_packet, offset);



  /* send the DHCPDISCOVER packet to broadcast address */
  sockaddr_broadcast.sin_family=AF_INET;
  sockaddr_broadcast.sin_port=htons(DHCP_SERVER_PORT);
  sockaddr_broadcast.sin_addr.s_addr=INADDR_BROADCAST;
  bzero(&sockaddr_broadcast.sin_zero,sizeof(sockaddr_broadcast.sin_zero));

  print_packet((u_int8_t*)&discover_packet, sizeof(discover_packet));

  /* send the DHCPDISCOVER packet out */
  send_dhcp_packet(&discover_packet,sizeof(discover_packet),sock,&sockaddr_broadcast);
  return EXIT_SUCCESS;
}


/* waits for a DHCPOFFER message from one or more DHCP servers */
std::vector<dhcp_packet> DHCPClient::get_dhcp_offer(int sock){
  struct dhcp_packet offer_packet;
  std::vector<dhcp_packet> dhcp_packets;
  struct sockaddr_in source;
  int result;
  int responses = 0;
  time_t start_time;
  time_t current_time;

  time(&start_time);

  /* receive as many responses as we can */
  for(responses=0, valid_responses_=0 ; ; ){
    // todo: this should be changed.
    time(&current_time);
    if((current_time-start_time) >= DHCP_OFFER_TIMEOUT) {
      break;
    }

    bzero(&source, sizeof(source));
    bzero(&offer_packet, sizeof(offer_packet));

    receive_dhcp_packet(&offer_packet, sizeof(offer_packet), sock, DHCP_OFFER_TIMEOUT, &source);
    print_packet((u_int8_t*)&offer_packet, sizeof(offer_packet));
    responses++;

    /* check packet xid to see if its the same as the one we used in the discover packet */
    if(ntohl(offer_packet.xid)!=packet_xid_){
      continue;
    }
    /* check hardware address */
    result=EXIT_SUCCESS;
    for( int x = 0 ; x < IFHWADDRLEN ; x++ ) {
      if(offer_packet.chaddr[x] != client_hardware_address_.at((unsigned long)x)) {
        result = EXIT_FAILURE;
      }
    }
    if(result==EXIT_FAILURE){
      continue;
    }
    add_dhcp_offer(source.sin_addr,&offer_packet);
    dhcp_packets.push_back(offer_packet);
    valid_responses_++;
  }

  return dhcp_packets;
}


int DHCPClient::send_dhcp_request(int sock, struct in_addr server, struct in_addr requested) {
  auto request_packet = dhcp_packet_with_headers_set();
  struct sockaddr_in sockaddr_broadcast;

  request_packet.siaddr = server;

  int offset = 0;
  u_int8_t option;

  request_packet.options[offset++] = 0x63;
  request_packet.options[offset++] = 0x82;
  request_packet.options[offset++] = 0x53;
  request_packet.options[offset++] = 0x63;

  request_packet.op = BOOTREQUEST;

  offset += add_dhcp_option(&request_packet,
                            DHO_DHCP_MESSAGE_TYPE,
                            &(option = DHCPREQUEST),
                            offset,
                            sizeof(option)
  );

  /* the IP address we're requesting */
  offset += add_dhcp_option(&request_packet,
                            DHO_DHCP_REQUESTED_ADDRESS,
                            (u_int8_t*)&requested,
                            offset,
                            sizeof(requested)
  );

  /* the IP address of the server */
  offset += add_dhcp_option(&request_packet,
                            DHO_DHCP_SERVER_IDENTIFIER,
                            (u_int8_t*)&server,
                            offset,
                            sizeof(requested)
  );

  char hostname[1024];
  hostname[1023] = 0x00;
  gethostname(hostname, 1023);
  offset += add_dhcp_option(&request_packet,
                            DHO_HOST_NAME,
                            (u_int8_t*)&hostname,
                            offset,
                            (size_t) strlen(hostname)
  );

  end_dhcp_option(&request_packet, offset);


  /* send the DHCPREQUEST packet to broadcast address */
  sockaddr_broadcast.sin_family=AF_INET;
  sockaddr_broadcast.sin_port=htons(DHCP_SERVER_PORT);
  sockaddr_broadcast.sin_addr.s_addr=INADDR_BROADCAST;
  bzero(&sockaddr_broadcast.sin_zero,sizeof(sockaddr_broadcast.sin_zero));

  /* send the DHCPREQUEST packet out */
  send_dhcp_packet(&request_packet,sizeof(request_packet),sock,&sockaddr_broadcast);

  selected_server_ = request_packet.siaddr;


  return 0;
}

int DHCPClient::get_dhcp_acknowledgement(int socket, struct in_addr server) {

  dhcp_packet packet;

  struct sockaddr_in selected_server;
  selected_server.sin_addr = server;
  receive_dhcp_packet(&packet, sizeof(packet), socket, DHCP_OFFER_TIMEOUT, &selected_server);

  print_packet((u_int8_t *)&packet, DHCP_MIN_OPTION_LEN);

  if(packet.options[0] != 0x63){
    return -1;
  } else if (packet.options[1] != 0x82){
    return -1;
  } else if (packet.options[2] != 0x53){
    return -1;
  } else if (packet.options[3] != 0x63){
    return -1;
  }

  for(int x = 4 ; x < DHCP_MAX_OPTION_LEN ; ) {

    if((int) packet.options[x] == -1 || (int)packet.options[x]==0){
      break;
    }
    unsigned int option_type;
    unsigned int option_length;
    option_type = packet.options[x++];
    option_length = packet.options[x++];

    if( verbose ){
      printf("Option: %3d (0x%02X): ", option_type, option_length);
    }

    if(option_type == DHO_DHCP_MESSAGE_TYPE){
      u_int8_t message_type;
      memcpy(&message_type, &packet.options[x],option_length);
      // message_type = ntohs(message_type);
      if(message_type == 0x05 ){
        printf("Acknowledged");
      } else if (message_type == 0x06) {
        printf("Nack received");
      } else {
        printf("Message type is not identified: %02x\n", message_type);
      }
    } else if (option_type == DHO_DHCP_SERVER_IDENTIFIER) {
      struct in_addr server;
      memcpy(&server, &packet.options[x], option_length);
      printf("Server: %s", inet_ntoa(server));
    } else if (option_type == DHO_SUBNET_MASK) {
      struct in_addr subnet_mask;
      memcpy(&subnet_mask, &packet.options[x], option_length);
      printf("Subnet Mask: %s", inet_ntoa(subnet_mask));
    } else if (option_type == DHO_DOMAIN_NAME_SERVERS) {

      struct in_addr dns;
      for(int i = 0; i < option_length / sizeof(dns); i ++){
        memcpy(&dns, &packet.options[x + i * sizeof(dns)], sizeof(struct in_addr));
        printf("Dns: %s", inet_ntoa(dns));
      }
    } else if ( option_type == DHO_DHCP_LEASE_TIME) {
      u_int32_t lease_time;
      memcpy(&lease_time, &packet.options[x], option_length);
      lease_time = ntohl(lease_time);
      printf("Lease time: %u", lease_time);
    } else if (option_type == DHO_ROUTERS){
      struct in_addr router;
      memcpy(&router, &packet.options[x], option_length);
      printf("Router: %s", inet_ntoa(router));
    } else if ( option_type == DHO_END) {
      break;
    }
    printf("\n");
    x += option_length;
  }


  return 0;
}

/* adds a DHCP OFFER to list in memory */
int DHCPClient::add_dhcp_offer(struct in_addr source, dhcp_packet* offer_packet){
  dhcp_offer *new_offer;
  int x;
  int y;
  unsigned option_type;
  unsigned option_length;

  if(offer_packet==nullptr)
    return EXIT_FAILURE;
  if(offer_packet->options[0] != 0x63){
    return -1;
  } else if (offer_packet->options[1] != 0x82){
    return -1;
  } else if (offer_packet->options[2] != 0x53){
    return -1;
  } else if (offer_packet->options[3] != 0x63){
    return -1;
  }

  /* process all DHCP options present in the packet */
  for( x = 4 ; x <  DHCP_MAX_OPTION_LEN; ){

    /* end of options (0 is really just a pad, but bail out anyway) */
    if((int)offer_packet->options[x]==-1 || (int)offer_packet->options[x]==0) {
      break;
    }
    /* get option type */
    option_type=(unsigned int)offer_packet->options[x++];
    /* get option length */
    option_length=(unsigned int)offer_packet->options[x++];

    if (verbose) {
      printf("Option: %3d (0x%02X): ", option_type, option_length);
    }
    /* get option data */
    if (option_type == DHO_DHCP_LEASE_TIME) {
      memcpy(&dhcp_lease_time_, &offer_packet->options[x],
             sizeof(dhcp_lease_time_));
      dhcp_lease_time_ = ntohl(dhcp_lease_time_);
    } else if (option_type==DHO_DHCP_RENEWAL_TIME) {
      memcpy(&dhcp_renewal_time_, &offer_packet->options[x],
             sizeof(dhcp_renewal_time_));
      dhcp_renewal_time_ = ntohl(dhcp_renewal_time_);
    } else if (option_type == DHO_DHCP_REBINDING_TIME) {
      memcpy(&dhcp_rebinding_time_, &offer_packet->options[x],
             sizeof(dhcp_rebinding_time_));
      dhcp_rebinding_time_ = ntohl(dhcp_rebinding_time_);
    } else if (option_type == DHO_SUBNET_MASK){

    } else if (option_type == DHO_ROUTERS){

    } else if (option_type == DHO_DHCP_SERVER_IDENTIFIER){

    } else if (option_type == DHO_DHCP_MESSAGE_TYPE){

    } else if (option_type == DHO_END) {
      break;
    }
    printf("\n");
    x += option_length;
  }

  new_offer=(dhcp_offer *)malloc(sizeof(dhcp_offer));

  if(new_offer== nullptr)
    return EXIT_FAILURE;

  new_offer->server_address=source;
  new_offer->offered_address=offer_packet->yiaddr;
  new_offer->lease_time=dhcp_lease_time_;
  new_offer->renewal_time=dhcp_renewal_time_;
  new_offer->rebinding_time=dhcp_rebinding_time_;

  dhcp_offers_.push_back(*new_offer);
  responding_servers_.push_back(source);
  return EXIT_SUCCESS;
}

/* adds a requested server address to list in memory */
void DHCPClient::add_requested_server(struct in_addr server_address){

  requested_servers_.push_back(server_address);

  if (verbose) {
    printf("Requested server address: %s\n", inet_ntoa(server_address));
  }

}

void DHCPClient::free_dhcp_offer_list(){
  dhcp_offers_.clear();
}


/* frees memory allocated to requested server list */
int DHCPClient::free_requested_server_list(){

  requested_servers_.clear();

  return EXIT_SUCCESS;
}

