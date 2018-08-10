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
#include <dhcp-client.h>

#include <net/if.h>
#include <sys/ioctl.h>

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

  /* send the DHCPDISCOVER packet out */
  send_dhcp_packet(&discover_packet,sizeof(discover_packet),sock,&sockaddr_broadcast);

  parse_dhcp_packet(&discover_packet);
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
  request = dhcp_packet_with_headers_set();
  struct sockaddr_in sockaddr_broadcast;

  request.siaddr = server;

  int offset = 0;
  u_int8_t option;

  request.options[offset++] = 0x63;
  request.options[offset++] = 0x82;
  request.options[offset++] = 0x53;
  request.options[offset++] = 0x63;

  request.op = BOOTREQUEST;

  offset += add_dhcp_option(&request,
                            DHO_DHCP_MESSAGE_TYPE,
                            &(option = DHCPREQUEST),
                            offset,
                            sizeof(option)
  );

  /* the IP address we're requesting */
  offset += add_dhcp_option(&request,
                            DHO_DHCP_REQUESTED_ADDRESS,
                            (u_int8_t*)&requested,
                            offset,
                            sizeof(requested)
  );

  /* the IP address of the server */
  offset += add_dhcp_option(&request,
                            DHO_DHCP_SERVER_IDENTIFIER,
                            (u_int8_t*)&server,
                            offset,
                            sizeof(requested)
  );

  char hostname[1024];
  hostname[1023] = 0x00;
  gethostname(hostname, 1023);
  offset += add_dhcp_option(&request,
                            DHO_HOST_NAME,
                            (u_int8_t*)&hostname,
                            offset,
                            (size_t) strlen(hostname)
  );

  end_dhcp_option(&request, offset);


  /* send the DHCPREQUEST packet to broadcast address */
  sockaddr_broadcast.sin_family=AF_INET;
  sockaddr_broadcast.sin_port=htons(DHCP_SERVER_PORT);
  sockaddr_broadcast.sin_addr.s_addr=INADDR_BROADCAST;
  bzero(&sockaddr_broadcast.sin_zero,sizeof(sockaddr_broadcast.sin_zero));

  /* send the DHCPREQUEST packet out */
  send_dhcp_packet(&request,sizeof(dhcp_packet),sock,&sockaddr_broadcast);

  selected_server_ = request.siaddr;


  parse_dhcp_packet(&request);
  return 0;
}

int DHCPClient::get_dhcp_acknowledgement(int socket, struct in_addr server) {

  struct sockaddr_in selected_server;
  selected_server.sin_addr = server;
  receive_dhcp_packet(&acknowledge, sizeof(dhcp_packet), socket, DHCP_OFFER_TIMEOUT, &selected_server);


  parse_dhcp_packet(&acknowledge);

  return 0;
}

/* adds a DHCP OFFER to list in memory */
int DHCPClient::add_dhcp_offer(struct in_addr source, dhcp_packet* offer_packet){
  dhcp_offer *new_offer;
  int x;
  int y;

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

  auto options = parse_dhcp_packet(offer_packet);

  u_int32_t lease_time;
  u_int32_t renewal_time;
  u_int32_t rebinding_time;
  struct in_addr subnet_mask = {};
  std::vector<struct in_addr> domain_name_servers;
  struct in_addr router_address = {};

  new_offer=(dhcp_offer *)malloc(sizeof(dhcp_offer));
  if(new_offer== nullptr)
    return EXIT_FAILURE;

  for(auto option : options){
    if (option.type == DHO_DHCP_LEASE_TIME) {
      memcpy(&lease_time, &option.data,
             sizeof(lease_time));
      lease_time = ntohl(lease_time);
      new_offer->lease_time = lease_time;
    } else if (option.type==DHO_DHCP_RENEWAL_TIME) {
      memcpy(&renewal_time, &option.data,
             sizeof(renewal_time));
      renewal_time = ntohl(renewal_time);
      new_offer->renewal_time = renewal_time;
    } else if (option.type == DHO_DHCP_REBINDING_TIME) {
      memcpy(&rebinding_time, &option.data,
             sizeof(rebinding_time));
      rebinding_time = ntohl(rebinding_time);
      new_offer->rebinding_time = rebinding_time;
    } else if (option.type == DHO_SUBNET_MASK) {
      memcpy(&subnet_mask, option.data, option.length);
    } else if (option.type == DHO_DOMAIN_NAME_SERVERS){
      struct in_addr dns = {};
      for(int i = 0; i < (option.length / sizeof(dns)) ; i++){
        memcpy(&dns, &option.data[i * sizeof(dns)], sizeof(dns));
        domain_name_servers.push_back(dns);
      }
    } else if (option.type == DHO_ROUTERS) {
      memcpy(&router_address, option.data, option.length);
    }
  }

  new_offer->server_address = source;
  new_offer->offered_address = offer_packet->yiaddr;

  dhcp_offers_.push_back(*new_offer);
  responding_servers_.push_back(source);
  return EXIT_SUCCESS;
}

void DHCPClient::free_dhcp_offer_list(){
  dhcp_offers_.clear();
}


/* frees memory allocated to requested server list */
int DHCPClient::free_requested_server_list(){

  requested_servers_.clear();

  return EXIT_SUCCESS;
}


/* creates a socket for DHCP communication */
int create_dhcp_socket(std::string interface_name){
  struct sockaddr_in myname;
	struct ifreq interface;
	int sock;
  int flag=1;

  /* Set up the address we're going to bind to. */
	bzero(&myname,sizeof(myname));
  myname.sin_family=AF_INET;
  myname.sin_port=htons(DHCP_CLIENT_PORT);
  myname.sin_addr.s_addr=INADDR_ANY;                 /* listen on any address */
  bzero(&myname.sin_zero,sizeof(myname.sin_zero));

        /* create a socket for DHCP communications */
	sock = socket(AF_INET,SOCK_DGRAM,IPPROTO_UDP);

  if(sock<0){
		printf("Error: Could not create socket!\n");
		exit(errno);
	}

	/* set the reuse address flag so we don't get errors when restarting */
  flag = 1;
  if( setsockopt(sock,SOL_SOCKET,SO_REUSEADDR,(char *)&flag,sizeof(flag)) < 0 ){
    perror("Error: Could not set reuse address option on DHCP socket!\n");
    exit(errno);
  }

  /* set the broadcast option - we need this to listen to DHCP broadcast messages */
  if(setsockopt(sock,SOL_SOCKET,SO_BROADCAST,(char *)&flag,sizeof flag)<0){
    perror("Error: Could not set broadcast option on DHCP socket!");
    exit(errno);
  }

  /* bind socket to interface */
  strncpy(interface.ifr_ifrn.ifrn_name, interface_name.c_str(),IFNAMSIZ);
  if(setsockopt(sock,SOL_SOCKET,SO_BINDTODEVICE,(char *)&interface,sizeof(interface))<0){
    perror("Error: Could not bind socket to interface.  Check your privileges...");
		exit(errno);
	}

  /* bind the socket */
  if(bind(sock,(struct sockaddr *)&myname,sizeof(myname))<0){
		perror("Error: Could not bind to DHCP socket !  Check your privileges...");
		exit(errno);
	}

  return sock;
}

void close_dhcp_socket(int sock){
	close(sock);
}


/* determines hardware address on client machine */
int get_hardware_address(std::string interface_name, std::string *result) {
	int i;
	struct ifreq ifr;

	strncpy(ifr.ifr_name, interface_name.c_str(), IFNAMSIZ);
  int sock = socket(PF_INET, SOCK_DGRAM, IPPROTO_IP);

	if( ioctl(sock, SIOCGIFHWADDR, &ifr) < 0){
		perror("Could not get interface name");
		return errno;
	}
	unsigned char mac_address[6];
	memcpy(&mac_address[0], &ifr.ifr_hwaddr.sa_data, 6);
  char mac[12];
	sprintf(mac, "%2.2x%2.2x%2.2x%2.2x%2.2x%2.2x",mac_address[0], mac_address[1], mac_address[2], mac_address[3], mac_address[4], mac_address[5]);
  close(sock);
  *result = std::string(mac);
  return 0;
}