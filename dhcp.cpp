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
#include <dhcp-client.h>
#include <iostream>


int main(int argc, char **argv){
  int result;
  if( argc < 2){
    exit(EXIT_FAILURE);
  }

  DHCPClient dhcpClient(argv[1]);

  dhcpClient.initialize();


  std::cout << "\n======== DISCOVER ===============================" << std::endl;
  dhcpClient.send_dhcp_discover();

  std::cout << "\n\n======== OFFER    ===============================" << std::endl;
  dhcpClient.get_dhcp_offer();


  dhcpClient.cleanup();
  return 0;

  for(auto i : dhcpClient.getOffers()){

    std::cout << "\n\n======== REQUEST  ===============================" << std::endl;
    dhcpClient.send_dhcp_request(i.siaddr, i.yiaddr);

    std::cout << "\n\n======== ACKNOWLEDGEMENT ========================" << std::endl;
    dhcpClient.get_dhcp_acknowledgement(i.siaddr);

    std::string(inet_ntoa(i.siaddr));
    std::cout << "\n\nGot ip: " << std::string(inet_ntoa(i.yiaddr))
              << " from server: " <<  std::string(inet_ntoa(i.yiaddr))
              << std::endl;
    break;
  }




  return 0;
}

/* gets state and plugin output to return */
