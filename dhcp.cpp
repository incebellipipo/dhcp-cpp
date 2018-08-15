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

  auto l = DHCPClient::gather_lease(argv[1]);

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
  return 0;
}
