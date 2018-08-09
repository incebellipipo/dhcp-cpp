//
// Created by cem on 23.07.2018.
//

#include <dhcp-packet.h>
#include <cstdio>
#include <cstring>
#include <sys/types.h>
#include <cerrno>
#include <arpa/inet.h>
#include <dhcpc.h>
#include <iostream>


void print_packet(const u_int8_t *data, int len) {
#ifdef DEBUG
  for (int i = 0; i < len; i++) {
    if (i % 0x10 == 0) {
      printf("\n %04x :: ", i);
    }
    printf("%02x ", data[i]);
  }
#endif
}

/* sends a DHCP packet */
int send_dhcp_packet(void *buffer, int buffer_size, int sock, struct sockaddr_in *destination){
	struct sockaddr_in myname;
	int result;

	result = (int) sendto(sock,(char *)buffer, (size_t)buffer_size,0, (struct sockaddr *)destination,sizeof(*destination));

	if (verbose) {
    printf("[verbose] send_dhcp_packet result: %d\n", result);
  }

	if(result<0) {
	  perror("Send dhcp packet resulting with an error");
    return EXIT_FAILURE;
  }

	return EXIT_SUCCESS;
}

/* receives a DHCP packet */
int receive_dhcp_packet(void *buffer, int buffer_size, int sock, int timeout, struct sockaddr_in *address){
  struct timeval tv;
  fd_set readfds;
  int recv_result;
  socklen_t address_size;
  struct sockaddr_in source_address;

  /* wait for data to arrive (up time timeout) */
  tv.tv_sec=timeout;
  tv.tv_usec=0;
  FD_ZERO(&readfds);
  FD_SET(sock,&readfds);
  select(sock+1,&readfds, nullptr, nullptr,&tv);

  /* make sure some data has arrived */
  if(!FD_ISSET(sock,&readfds)){
    if (verbose) {
      printf("[verbose] No (more) data received\n");
    }
    return -1;
  }

  else{

    /* why do we need to peek first?  i don't know, its a hack.  without it, the source address of the first packet received was
    not being interpreted correctly.  sigh... */
    bzero(&source_address,sizeof(source_address));
    address_size=sizeof(source_address);

    recv_result=(int)recvfrom(sock,(char *)buffer,(size_t)buffer_size,MSG_PEEK,(struct sockaddr *)&source_address,&address_size);

    if (verbose) {
      printf("[verbose] recv_result_1: %d\n", recv_result);
    }

    recv_result=(int)recvfrom(sock,(char *)buffer,(size_t)buffer_size,0,(struct sockaddr *)&source_address,&address_size);

    if (verbose) {
      printf("[verbose] recv_result_2: %d\n", recv_result);
    }

    if(recv_result==-1){
      if (verbose) {
				perror("[verbose] recvfrom() failed, ");
			}
      return errno;
    }
		else{
			memcpy(address,&source_address,sizeof(source_address));
			return EXIT_SUCCESS;
		}
  }
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