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

  packet.magic_cookie = htonl(DHCP_MAGIC_COOKIE);

  // hardware address type
  packet.htype = HTYPE_ETHER;

  // length of our hardware address
  packet.hlen = IFHWADDRLEN;

  packet.hops = 0;

  // transaction id is supposed to be random
  srand((unsigned int)time(nullptr));

  packet.xid = htonl(packet_xid_);

  // TODO: this is dirty fix: If below call is not made only one server response is processed
  ntohl(packet.xid);

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

  char hostname[1024];
  hostname[1023] = 0x00;
  gethostname(hostname, 1023);
  offset += add_dhcp_option(&discover_packet,
                            DHO_HOST_NAME,
                            (u_int8_t*)&hostname,
                            offset,
                            (size_t) strlen(hostname)
                            );

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

  printf("\n\n");


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


int DHCPClient::send_dhcp_request(int sock, dhcp_packet discover_packet, struct in_addr server) {
  auto request_packet = dhcp_packet_with_headers_set();
  struct sockaddr_in sockaddr_broadcast;

  request_packet.siaddr = discover_packet.siaddr;


  /* send the DHCPREQUEST packet to broadcast address */
  sockaddr_broadcast.sin_family=AF_INET;
  sockaddr_broadcast.sin_port=htons(DHCP_SERVER_PORT);
  sockaddr_broadcast.sin_addr.s_addr=INADDR_BROADCAST;
  bzero(&sockaddr_broadcast.sin_zero,sizeof(sockaddr_broadcast.sin_zero));

  /* send the DHCPREQUEST packet out */
  send_dhcp_packet(&request_packet,sizeof(request_packet),sock,&sockaddr_broadcast);

  selected_server_ = request_packet.siaddr;

  if(verbose) {
    printf("\n\n");
  }

  return 0;
}

int DHCPClient::get_dhcp_acknowledgement(int socket){

  dhcp_packet packet;

  struct sockaddr_in selected_server;
  selected_server.sin_addr = selected_server_;
  receive_dhcp_packet(&packet, sizeof(packet), socket, DHCP_OFFER_TIMEOUT, &selected_server);

  for(int x = 4 ; x < DHCP_MAX_OPTION_LEN ; ) {

    if((int) packet.options[x] == -1 || (int)packet.options[x]==0){
      break;
    }
    unsigned int option_type;
    unsigned int option_length;

    option_type = packet.options[x++];
    option_length = packet.options[x++];

    if( verbose ){
      printf("Option: %d (0x%02X)\n", option_type, option_length);
    }

    if(option_type == DHO_DHCP_MESSAGE_TYPE){
      int message_type;
      memcpy(&message_type, &packet.options[x],option_length);
      //message_type = ntohl(message_type);
    } else if (option_type == DHO_SUBNET_MASK) {
      struct in_addr subnet_mask;
      memcpy(&subnet_mask, &packet.options[x], option_length);
    } else if (option_type == DHO_DOMAIN_NAME_SERVERS) {
      int dns_size;
      memcpy(&dns_size, &packet.options[x++], 1);
      struct in_addr dns;
      for(int i = 0; i < option_length / dns_size; i += dns_size , x+= dns_size){
        memcpy(&dns, &packet.options[x], dns_size);
        printf("dns: %s\n", inet_ntoa(dns));
      }
      printf("size of inet %lu\n" , sizeof(struct in_addr));
    } else if (option_type == DHO_ROUTERS ){
      printf("router\n");
      x+= option_length;
    } else if ( option_type == DHO_DHCP_LEASE_TIME){
      printf("lease time\n");
      x += option_length;
    } else if ( option_type == DHO_DHCP_SERVER_IDENTIFIER) {
      x += option_length;
      printf("dhcp server");
    }

    else {
      for( int y = 0 ; y < option_length ; y++ , x++);
    }


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
      printf("Option: %d (0x%02X)\n", option_type, option_length);
    }
    /* get option data */
    if(option_type==DHO_DHCP_LEASE_TIME) {
      memcpy(&dhcp_lease_time_, &offer_packet->options[x],
             sizeof(dhcp_lease_time_));
      dhcp_lease_time_ = ntohl(dhcp_lease_time_);
    } else if(option_type==DHO_DHCP_RENEWAL_TIME) {
      memcpy(&dhcp_renewal_time_, &offer_packet->options[x],
             sizeof(dhcp_renewal_time_));
      dhcp_renewal_time_ = ntohl(dhcp_renewal_time_);
    } else if(option_type==DHO_DHCP_REBINDING_TIME) {
      memcpy(&dhcp_rebinding_time_, &offer_packet->options[x],
             sizeof(dhcp_rebinding_time_));
      dhcp_rebinding_time_ = ntohl(dhcp_rebinding_time_);
    }

      /* skip option data we're ignoring */
    else {
      for (y = 0; y < option_length; y++, x++);
    }
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

