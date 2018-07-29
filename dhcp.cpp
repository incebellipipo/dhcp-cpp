#include <cstdio>
#include <cstdlib>
#include <clocale>
#include <cstring>
#include <cerrno>
#include <ctime>

//#include <sys/time.h>

#include <unistd.h>

#include <sys/ioctl.h>
#include <fcntl.h>
#include <getopt.h>
#include <sys/socket.h>
#include <sys/types.h>

#include <netdb.h>
#include <netinet/in.h>
#include <net/if.h>

#include <arpa/inet.h>
#include <linux/if_ether.h>
#include <features.h>


#include <dhcp-packet.h>
#include <dhcpc-helpers.h>
#include <dhcpc.h>
#include <iostream>


int main(int argc, char **argv){
  int dhcp_socket;
  int result;

  DHCPClient dhcpClient;

  std::string interface_name("wlp8s0");

  /* create socket for DHCP communications */
  dhcp_socket=create_dhcp_socket(interface_name);

  std::string client_hardware_address;
  get_hardware_address(interface_name, &client_hardware_address);

  dhcpClient.set_packet_xid((u_int32_t) random());
  dhcpClient.set_client_hardware_address(client_hardware_address);

  dhcpClient.send_dhcp_discover(dhcp_socket);

  auto packets = dhcpClient.get_dhcp_offer(dhcp_socket);

  std::cout << "OUR PART" << std::endl;

//  for(auto packet : packets) {
//    dhcpClient.send_dhcp_request(dhcp_socket, packet, in_addr());
//  }
//
//  std::cout << "OUR PART #2" << std::endl;
//
//  dhcpClient.get_dhcp_acknowledgement(dhcp_socket);

  close_dhcp_socket(dhcp_socket);



  return result;
}

/* gets state and plugin output to return */
