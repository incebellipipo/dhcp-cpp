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
#include <linux/if_ether.h>
#include <netinet/ip.h>
#include <netinet/udp.h>

DHCPClient::DHCPClient(char *interface_name) {
  strncpy(ifname_, interface_name, IFNAMSIZ);
  struct ifreq ifr = {};
  memset(&ifr, 0, sizeof(ifreq));
  strncpy(ifr.ifr_name, ifname_, IFNAMSIZ );
  int ifreq_sock = socket(AF_INET, SOCK_DGRAM, IPPROTO_IP);
  if(ioctl(ifreq_sock, SIOCGIFHWADDR, &ifr) < 0){
    perror("Can not gather hwaddr");
    exit(EXIT_FAILURE);
  }
  close(ifreq_sock);

  memcpy((void*)&hwaddr_, &ifr.ifr_hwaddr.sa_data, IFHWADDRLEN);

  std::srand((u_int32_t)std::time(nullptr));
  packet_xid_ = (u_int32_t) random();
}

void DHCPClient::initialize() {

  listen_raw_sock_fd_ = socket(PF_PACKET, SOCK_RAW, htons(ETH_P_ALL));

  if(listen_raw_sock_fd_ < 0){
    perror("Error socket(): ");
    exit(EXIT_FAILURE);
  }


  struct timeval timeout = {};
  timeout.tv_sec = 3;
  timeout.tv_usec = 0;

  setsockopt(listen_raw_sock_fd_, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(timeout));

/*
  struct ifreq ifr = {};
  memset((void*)&ifr, 0, sizeof(struct ifreq));
  strncpy(ifr.ifr_name, ifname_, IFNAMSIZ);
  setsockopt(listen_raw_sock_fd_, SOL_SOCKET, SO_BINDTODEVICE, (void*)&ifr, sizeof(ifr));
*/
  std::srand((u_int32_t)std::time(nullptr));
  packet_xid_ = (u_int32_t) random();


}

void DHCPClient::cleanup() {
  close(listen_raw_sock_fd_);
}

struct dhcp_packet DHCPClient::dhcp_packet_with_headers_set() {

  struct dhcp_packet packet = {};

  // clear the packet data structure
  bzero(&packet, sizeof(packet));

  // hardware address type
  packet.htype = HTYPE_ETHER;

  // length of our hardware address
  packet.hlen = IFHWADDRLEN;

  packet.hops = 0;
  packet.xid = htonl(packet_xid_);
  packet.secs = htons(USHRT_MAX);
  packet.flags = htons(BOOTP_UNICAST);

  memcpy(packet.chaddr, hwaddr_, IFHWADDRLEN);
  return packet;
}

/* sends a DHCPDISCOVER broadcast message in an attempt to find DHCP servers */
int DHCPClient::send_dhcp_discover() {
  discovery_ = dhcp_packet_with_headers_set();

  int offset = 0;
  discovery_.options[offset++] = 0x63;
  discovery_.options[offset++] = 0x82;
  discovery_.options[offset++] = 0x53;
  discovery_.options[offset++] = 0x63;

  u_int8_t option;

  discovery_.op = BOOTREQUEST;
  offset += add_dhcp_option(&discovery_,
                            DHO_DHCP_MESSAGE_TYPE,
                            &(option = DHCPDISCOVER),
                            offset,
                            sizeof(option)
                            );

  /* the IP address we're requesting */
  if(request_specific_address_){
    offset += add_dhcp_option(&discovery_,
                              DHO_DHCP_REQUESTED_ADDRESS,
                              (u_int8_t*)&requested_address_,
                              offset,
                              sizeof(requested_address_)
                              );
  }

  char hostname[1024];
  hostname[1023] = 0x00;
  gethostname(hostname, 1023);
  offset += add_dhcp_option(&discovery_,
                            DHO_HOST_NAME,
                            (u_int8_t*)&hostname,
                            offset,
                            (u_int8_t) strlen(hostname)
                            );

  u_int8_t parameter_requst_list[] = {
          DHO_SUBNET_MASK,
          DHO_BROADCAST_ADDRESS,
          DHO_TIME_OFFSET,
          DHO_ROUTERS,
          DHO_DOMAIN_NAME,
          DHO_DOMAIN_NAME_SERVERS,
          DHO_HOST_NAME,
          DHO_NETBIOS_NAME_SERVERS,
          DHO_INTERFACE_MTU,
          DHO_STATIC_ROUTES,
          DHO_NTP_SERVERS
  };

  offset += add_dhcp_option(&discovery_,
                            DHO_DHCP_PARAMETER_REQUEST_LIST,
                            (u_int8_t*)&parameter_requst_list,
                            offset,
                            sizeof(parameter_requst_list)
                            );

  end_dhcp_option(&discovery_, offset);

  /* send the DHCPDISCOVER packet out */
  return send_dhcp_packet(&discovery_, sizeof(discovery_), ifname_);
}


/* waits for a DHCPOFFER message from one or more DHCP servers */
void DHCPClient::get_dhcp_offer() {

  /* receive as many responses as we can */
  auto offer_packet = (struct dhcp_packet*)malloc(sizeof(dhcp_packet));

  memset(offer_packet, 0 , sizeof(struct dhcp_packet));
  bzero(offer_packet, sizeof(struct dhcp_packet));

  for(int count = 0;
          count <= sizeof(ethhdr) + sizeof(iphdr) + sizeof(udphdr) ; ){
    ioctl(listen_raw_sock_fd_, FIONREAD, &count);
    usleep(1000);
  }

  auto result = receive_dhcp_packet(listen_raw_sock_fd_, offer_packet, sizeof(struct dhcp_packet), DHCP_OFFER_TIMEOUT);

  /* check packet xid to see if its the same as the one we used in the discover packet */
  if(ntohl(offer_packet->xid)!=packet_xid_){
    return;
  }

  /* check hardware address */
  if(strcmp((char*)&offer_packet->chaddr, (char*)&hwaddr_) == 0) {
    offers_.push_back(*offer_packet);
  }
}


int DHCPClient::send_dhcp_request(struct in_addr server, struct in_addr requested) {
  request_ = dhcp_packet_with_headers_set();
  struct sockaddr_in sockaddr_broadcast;

  request_.siaddr = server;

  int offset = 0;
  u_int8_t option;

  request_.options[offset++] = 0x63;
  request_.options[offset++] = 0x82;
  request_.options[offset++] = 0x53;
  request_.options[offset++] = 0x63;

  request_.op = BOOTREQUEST;

  offset += add_dhcp_option(&request_,
                            DHO_DHCP_MESSAGE_TYPE,
                            &(option = DHCPREQUEST),
                            offset,
                            sizeof(option)
  );

  /* the IP address we're requesting */
  offset += add_dhcp_option(&request_,
                            DHO_DHCP_REQUESTED_ADDRESS,
                            (u_int8_t*)&requested,
                            offset,
                            sizeof(requested)
  );

  /* the IP address of the server */
  offset += add_dhcp_option(&request_,
                            DHO_DHCP_SERVER_IDENTIFIER,
                            (u_int8_t*)&server,
                            offset,
                            sizeof(requested)
  );

  char hostname[1024];
  hostname[1023] = 0x00;
  gethostname(hostname, 1023);
  offset += add_dhcp_option(&request_,
                            DHO_HOST_NAME,
                            (u_int8_t*)&hostname,
                            offset,
                            (u_int8_t) strlen(hostname)
  );

  end_dhcp_option(&request_, offset);


  /* send the DHCPREQUEST packet to broadcast address */
  sockaddr_broadcast.sin_family=AF_INET;
  sockaddr_broadcast.sin_port=htons(DHCP_SERVER_PORT);
  sockaddr_broadcast.sin_addr.s_addr=INADDR_BROADCAST;
  bzero(&sockaddr_broadcast.sin_zero,sizeof(sockaddr_broadcast.sin_zero));

  /* send the DHCPREQUEST packet out */
  send_dhcp_packet(&request_, sizeof(dhcp_packet), ifname_);

  selected_server_ = request_.siaddr;

  parse_dhcp_packet(&request_);
  return 0;
}

int DHCPClient::get_dhcp_acknowledgement(struct in_addr server) {

  struct sockaddr_in selected_server;
  selected_server.sin_addr = server;
  receive_dhcp_packet(listen_raw_sock_fd_, &acknowledge_, sizeof(dhcp_packet), DHCP_OFFER_TIMEOUT);


  parse_dhcp_packet(&acknowledge_);

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

  responding_servers_.push_back(source);
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
