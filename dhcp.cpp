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
#include <lease.h>


int main(int argc, char **argv){
  int result;
  if( argc < 2){
    exit(EXIT_FAILURE);
  }

  DHCPClient dhcpClient(argv[1]); // interface name

  dhcpClient.initialize();


  std::cout << "\n======== DISCOVER ===============================" << std::endl;
  dhcpClient.do_discover();

  std::cout << "\n\n======== OFFER    ===============================" << std::endl;
  dhcpClient.listen_offer();

  for(auto offer : dhcpClient.get_offers()){

    bool acknowledged = false;
    struct in_addr server_ip = {};
    for(auto option : parse_dhcp_packet(&offer)){
      if(option.type == DHO_DHCP_SERVER_IDENTIFIER ){
        memcpy((void*)&server_ip, option.data, option.length);
      }
    }

    std::cout << "\n\n======== REQUEST  ===============================" << std::endl;
    dhcpClient.do_request(server_ip, offer.yiaddr);

    std::cout << "\n\n======== ACKNOWLEDGEMENT ========================" << std::endl;
    dhcpClient.listen_acknowledgement(server_ip);

    auto ack_packet = dhcpClient.get_acknowledge();
    for(auto option : parse_dhcp_packet(&ack_packet)){
      if(option.type == DHO_DHCP_MESSAGE_TYPE){
        if(*option.data == DHCPACK){

          auto l = process_lease(&ack_packet);
          std::cout << "lease time:         " << l.lease_time << std::endl;
          std::cout << "message type:       " << (int)l.message_type << std::endl;
          std::cout << "address:            " << std::string(inet_ntoa(l.address)) << std::endl;
          std::cout << "rebind time:        " << l.rebind << std::endl;
          std::cout << "renew time:         " << l.renew << std::endl;
          std::cout << "expire time:        " << l.expire << std::endl;
          std::cout << "subnet mask:        " << std::string(inet_ntoa(l.subnet_mask)) << std::endl;
          std::cout << "server identifier:  " << std::string(inet_ntoa(l.server_identifier)) << std::endl;
          std::cout << "router:             " << std::string(inet_ntoa(l.routers)) << std::endl;
          for(auto dns : l.domain_name_servers){
            std::cout << "dns:  " << std::string(inet_ntoa(dns)) << std::endl;
          }

        } else if (*option.data == DHCPNAK){
          std::cout << "\n\nServer: " << std::string(inet_ntoa(server_ip))
                                      << " responded NAK" << std::endl;
        }

      }
    }

    if(acknowledged){
      break;
    }
  }

  dhcpClient.cleanup();
  return 0;
}
