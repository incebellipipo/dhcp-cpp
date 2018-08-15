//
// Created by cem on 23.07.2018.
//

#include <cstdio>
#include <cstring>
#include <sys/types.h>
#include <cerrno>
#include <arpa/inet.h>
#include <iostream>
#include <map>

#include <unistd.h>

#include <netinet/ip.h>
#include <netinet/udp.h>
#include <linux/if_ether.h>
#include <linux/if_packet.h>

#include <dhcp-packet.h>
#include <dhcp-client.h>
#include <net/if.h>
#include <sys/ioctl.h>

inline void print_packet(const u_int8_t *data, int len) {
#if defined(DEBUG_PACKET)
  for (int i = 0; i < len; i++) {
    if (i % 0x10 == 0) {
      printf("\n%04x ::\t", i);
    }
    if( i % 0x08 == 0){
      printf("   ");
    }
    printf("%02x ", data[i]);
  }
  printf("\n");
#endif
}

/* sends a DHCP packet */
int send_dhcp_packet(void *buffer, int buffer_size, char *ifname) {

  auto udpchecksum = [](char* ip, char* udp, u_int16_t length) -> u_int32_t {
    udp[6] = udp[7] = 0;
    struct udp_psedoheader header = {};
    u_int32_t checksum = 0x0;
    memcpy((char*)&header.srcaddr, &ip[12], 4);
    memcpy((char*)&header.dstaddr, &ip[16], 4);
    header.zero = 0;
    header.protocol = IPPROTO_UDP;
    header.length = htons(length);
    auto hptr = (u_int16_t*)&header;
    for(int hlen = sizeof(header) ; hlen > 0; hlen -= 2) {
      checksum += *(hptr++);
    }
    auto uptr = (u_int16_t*)udp;
    for( ; length > 1 ; length -= 2){
      checksum += *(uptr++);
    }
    if( length ){
      checksum += *((u_int8_t*)uptr);
    }
    do {
      checksum = (checksum >> 16u) + (checksum & 0xFFFFu);
    } while( checksum != (checksum & 0xFFFFu));
    auto ans = (u_int16_t) checksum;
    return (ans == 0xFF) ? 0xFF : ntohs(~ans);
  };

  auto checksum = [](u_int16_t *addr, int length) -> u_int32_t {
    int nleft = length;
    u_int32_t sum = 0;
    u_int16_t *w = addr;
    u_int32_t answer = 0;
    for( ; nleft > 1 ; nleft -= sizeof(u_int16_t)){
      sum += *(w++);
    }
    if( nleft == 1 ){
      *(u_int8_t *)(&answer) = *(u_int8_t *)w;
      sum += answer;
    }
    sum = (sum >> 16u) + (sum & 0xFFFFu);
    sum += (sum >> 16u);
    return ~sum;
  };

  int result = -1;
  auto buf = (char*) malloc(
          sizeof(struct ethhdr) +     // 14
          sizeof(struct iphdr) +      // 20
          sizeof(struct udphdr) +     // 8
          sizeof(struct dhcp_packet)   // 548
          );

  memcpy(&buf[sizeof(struct ethhdr) + sizeof(struct iphdr) + sizeof(udphdr)],
          buffer,
         (size_t)buffer_size);

  // Construction of eth header
  auto ethh = (struct ethhdr*)(buf);

  struct ifreq ifr = {};
  memset(&ifr, 0, sizeof(ifreq));
  strncpy(ifr.ifr_name, ifname, IFNAMSIZ );
  int ifreq_sock = socket(AF_INET, SOCK_DGRAM, IPPROTO_IP);
  ioctl(ifreq_sock, SIOCGIFHWADDR, &ifr);
  close(ifreq_sock);

  memset(ethh->h_dest, 0xff, IFHWADDRLEN);
  memcpy((void*)&ethh->h_source, &ifr.ifr_hwaddr.sa_data, IFHWADDRLEN);
  ethh->h_proto = htons(ETH_P_IP);

  // Construction of udp header
  auto udph = (struct udphdr *)(buf + sizeof(struct ethhdr) + sizeof(struct iphdr));
  udph->source = htons(DHCP_CLIENT_PORT);
  udph->dest = htons(DHCP_SERVER_PORT);
  udph->len = htons(buffer_size + sizeof(struct udphdr));
  udph->check = 0x0;

  // Construction of ip header https://www.inetdaemon.com/tutorials/internet/ip/datagram_structure.shtml
  auto iph = (struct iphdr*)(buf + sizeof(struct ethhdr));
  iph->version = IPVERSION;
  iph->ihl = 5;
  iph->tos = 0x10;
  iph->tot_len = htons(sizeof(struct iphdr) + sizeof(struct udphdr) + buffer_size);
  iph->id = 0;
  iph->frag_off = 0;
  iph->ttl = 0x80;
  iph->protocol = IPPROTO_UDP;
  iph->check = 0x0;
  inet_aton("0.0.0.0", (struct in_addr*)&iph->saddr);
  inet_aton("255.255.255.255", (struct in_addr*)&iph->daddr);

  udph->check = htons((u_int16_t)
          udpchecksum((char*)iph, (char*)udph, buffer_size + sizeof(struct udphdr)));
  iph->check = (u_int16_t)
          checksum((u_int16_t*)iph, sizeof(struct iphdr));

  int total_len = sizeof(struct ethhdr) + sizeof(struct iphdr) + sizeof(struct udphdr) + buffer_size;

  struct sockaddr_ll device = {};
  if((device.sll_ifindex = if_nametoindex(ifname)) == 0){
    perror("Failed to resolve interface index");
    exit(EXIT_FAILURE);
  }

  int sendv4_sock = socket(PF_PACKET, SOCK_RAW, htons(ETH_P_ALL));

  result = (int)sendto(sendv4_sock, buf, (size_t)total_len, 0, (struct sockaddr*)&device, sizeof(device));

  close(sendv4_sock);

  print_packet((u_int8_t *)buf, result);

  free(buf);
	if(result<0) {
	  perror("Send dhcp packet resulting with an error");
    exit(EXIT_FAILURE);
  }

	return result;
}

/* receives a DHCP packet */
bool receive_dhcp_packet(int sock, void *packet, int packet_size, int timeout) {

  size_t max_length = sizeof(struct ethhdr) + sizeof(struct iphdr) + sizeof(struct udphdr) + packet_size;

  struct dhcp_packet incoming_packet = {};
  memset(&incoming_packet, 0 , sizeof(incoming_packet));
  bzero(&incoming_packet, sizeof(incoming_packet));

  u_int8_t * buf;
  buf = (u_int8_t *)malloc(max_length * 2);
  bzero(buf, max_length);

  int read_count = 0;
  for(bool valid = false; not valid and read_count < 4 ; valid = validate_packet(&incoming_packet)){
    auto len = (int) recv(sock, buf, max_length, 0);
    read_count++;
    if(len < 0){
      perror("Error recv(): ");
      return false;
    }
    memcpy(&incoming_packet,
          &buf[sizeof(struct ethhdr) + sizeof(struct iphdr) + sizeof(struct udphdr)],
          len -(sizeof(struct ethhdr) + sizeof(struct iphdr) + sizeof(struct udphdr))
    );
  }

  print_packet(buf,(int)max_length);
  memcpy(packet, &incoming_packet, sizeof(struct dhcp_packet));
  free(buf);
  return true;
}

int add_dhcp_option(struct dhcp_packet* packet, u_int8_t code, u_int8_t* data, int offset, u_int8_t len) {

  packet->options[offset] = code;
  packet->options[offset + 1] = len;

  memcpy(&packet->options[offset + 2], data, len);
  return len + (sizeof(u_int8_t) * 2);
}

void end_dhcp_option(struct dhcp_packet* packet, int offset){
  u_int8_t option = DHO_PAD;
  add_dhcp_option(packet, DHO_END, &option, offset, sizeof(option));
}

std::vector<dhcp_option> parse_dhcp_packet(struct dhcp_packet *packet){

  std::vector<dhcp_option> options;

  if(packet==nullptr)
    return options;
  if(packet->options[0] != 0x63){
    return options;
  } else if (packet->options[1] != 0x82){
    return options;
  } else if (packet->options[2] != 0x53){
    return options;
  } else if (packet->options[3] != 0x63){
    return options;
  }

  /* process all DHCP options present in the packet */
  for(int s, x = 4 ; x <  DHCP_MAX_OPTION_LEN; x += s){

    /* end of options (0 is really just a pad, but bail out anyway) */
    if((int)packet->options[x]==-1 || (int)packet->options[x]==0) {
      break;
    }

    dhcp_option option = {};

    /* get option type */
    option.type = (u_int8_t)packet->options[x++];

    /* get option length */
    option.length = (u_int8_t)packet->options[x++];

    s = option.length;

    /* get option data */
    option.data = (u_int8_t*)malloc(option.length * sizeof(u_int8_t));

    /* Copy data to option struct */
    memcpy(option.data, (void*)&packet->options[x], option.length);

    /* Push it in the vector */
    options.push_back(option);

    if(option.type == DHO_END){
      break;
    }
  }

  for(auto option : options){
    printf("Option, Type: %3u, Lenght: %02X, Data: ", option.type, option.length);
    for(int i = 0; i < option.length; i ++){
      printf("%02X ", option.data[i]);
    }
    printf("\n");
  }

}

bool validate_packet(struct dhcp_packet* packet){
  if (packet->op != BOOTREPLY) {
    return false;
  }

  if (packet->options[0] != 0x63)
    return false;
  if (packet->options[1] != 0x82)
    return false;
  if (packet->options[2] != 0x53)
    return false;
  if (packet->options[3] != 0x63)
    return false;


  return true;
}