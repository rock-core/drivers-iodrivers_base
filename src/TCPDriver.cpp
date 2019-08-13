#include "TCPDriver.hpp"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <fcntl.h>
#include <errno.h>
#include <stdlib.h>
#include <sys/ioctl.h>
#include <errno.h>
#include <signal.h>


namespace iodrivers_base{

TCPDriver::TCPDriver(int max_packet_size, bool extract_last):
    Driver(max_packet_size,extract_last)
{
    if(signal(SIGPIPE, SIG_IGN) == SIG_ERR){
        throw iodrivers_base::UnixError("TCPDriver: Could not deactivate signals");
    }

}

TCPDriver::~TCPDriver(){
    ::close(socked_fd);
    if(client_fd)
        ::close(client_fd);

}

void TCPDriver::tcp_server_init(int port){
    struct sockaddr_in serv_addr;
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_addr.s_addr = INADDR_ANY;
    serv_addr.sin_port = htons(port);
    socked_fd = socket(AF_INET, SOCK_STREAM, 0);

    if (socked_fd < 0)
        throw iodrivers_base::UnixError("TCPDriver: Could not create socked");

    if (bind(socked_fd, (struct sockaddr *) &serv_addr,sizeof(serv_addr)) < 0){
        throw iodrivers_base::UnixError("TCPDriver: Could bind to socked");
    }

    listen(socked_fd,5);
    clilen = sizeof(cli_addr);
    client_fd = 0;
    fcntl(socked_fd,F_SETFL,O_NONBLOCK);
}


void TCPDriver::checkClientConnection(){
  int new_client= accept(socked_fd, (struct sockaddr *) &cli_addr, &clilen);
  if(new_client > 0){
      if(client_fd)
          ::close(client_fd);
      client_fd = new_client;
      setFileDescriptor(new_client,false);
  }
}


int TCPDriver::readPacket(uint8_t* buffer, int bufsize){
    checkClientConnection();
    if(client_fd)
        return iodrivers_base::Driver::readPacket(buffer,bufsize);
    else
        return 0;
}

int TCPDriver::readPacket(uint8_t* buffer, int bufsize, base::Time const& packet_timeout, base::Time const& first_byte_timeout){
    checkClientConnection();
    if(client_fd){
        int res = 0;
        try{
            res =  iodrivers_base::Driver::readPacket(buffer,bufsize,packet_timeout, first_byte_timeout);
            return res;
        }catch(iodrivers_base::UnixError e){
            if(e.error == 32){ //Broken PIPE
                ::close(client_fd);
                client_fd = 0;
                return 0;
            }else{
                printf("Error is: %i\n",e.error);
                throw e;
            }
        }
    }else
        return 0;
}
bool TCPDriver::writePacket(uint8_t const* buffer, int bufsize, base::Time const& timeout){
    checkClientConnection();
    if(client_fd){
        bool res = false;
        try{
            res =  iodrivers_base::Driver::writePacket(buffer,bufsize,timeout);
            return res;
        }catch(iodrivers_base::UnixError e){
            if(e.error == 32){ //Broken PIPE
                ::close(client_fd);
                client_fd = 0;
                return false;
            }else{
                printf("Error is: %i\n",e.error);
                throw e;
            }
        }
    }else
        return false;
}


};
