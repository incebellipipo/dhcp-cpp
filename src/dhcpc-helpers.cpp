
#include <dhcp.h>
#include <dhcpc-helpers.h>


#include <unistd.h>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <netinet/in.h>
#include <net/if.h>
#include <sys/ioctl.h>
#include <iostream>
#include <dhcpc.h>

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

	if (verbose) {
    printf("DHCP socket: %d\n", sock);
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
