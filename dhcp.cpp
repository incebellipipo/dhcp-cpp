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
  dhcpClient.do_discover();

  std::cout << "\n\n======== OFFER    ===============================" << std::endl;
  dhcpClient.listen_offer();

  for(auto i : dhcpClient.get_offers()){

    struct in_addr server_ip = {};
    auto options = parse_dhcp_packet(&i);
    for(auto option : options){
      if(option.type == DHO_DHCP_SERVER_IDENTIFIER ){
        memcpy((void*)&server_ip, option.data, option.length);
      }
    }

    std::cout << "\n\n======== REQUEST  ===============================" << std::endl;
    dhcpClient.do_request(server_ip, i.yiaddr);

    std::cout << "\n\n======== ACKNOWLEDGEMENT ========================" << std::endl;
    dhcpClient.listen_acknowledgement(server_ip);

    std::cout << "\n\nGot ip: " << std::string(inet_ntoa(server_ip))
              << " from server: " <<  std::string(inet_ntoa(i.yiaddr))
              << std::endl;
    break;
  }




  dhcpClient.cleanup();
  return 0;
}

/* gets state and plugin output to return */
