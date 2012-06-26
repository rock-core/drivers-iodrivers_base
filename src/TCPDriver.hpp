#include <iodrivers_base/Driver.hpp>
#include <netinet/in.h>

namespace iodrivers_base{

/**
 * This class implements an TCP Server for single Client
 * If more thn one client tryes to connect, the old one is disconnected
 * see checkClientConnection for more details or if you want to implement support
 * for more than one client
 */

class TCPDriver : public iodrivers_base::Driver {
    public:

        TCPDriver(int max_packet_size, bool extract_last = false);
        virtual ~TCPDriver();
        
        
        /**
         * Initialized an new soked so that an TCP client can connect to the given port,
         * connection is not established by this mehtod, you need to call read or write packed
         * only one time to get the first connection.
         */
        void tcp_server_init(int port);

        /**
         * Overloaded method from iodriver_base::Driver, additionally calls checkClientConntion();
         */
        virtual int readPacket(uint8_t* buffer, int bufsize);
        virtual int readPacket(uint8_t* buffer, int bufsize, base::Time const& packet_timeout, base::Time const& first_byte_timeout);
        
        /**
         * Overloaded method from iodriver_base::Driver, additionally calls checkClientConntion();
         */
        virtual bool writePacket(uint8_t const* buffer, int bufsize, base::Time const& timeout);

        bool hasOpenSocked(){
            return socked_fd;
        }

    protected:
        /**
         * This Method cheks for an new waiting client that tryes to connect to the current port.
         * If an new clienet is discoverd, the old one will be disconnected and an connection
         * to the new one is established
         * The new fiile descriptor is passed to the iodriver to now handle the connection to the new one
         * the old one is closed so that the old client will be disconnected
         */
        virtual void checkClientConnection();

        /**
         * Corresponding file descriptor to the socked
         */
        int socked_fd;

        /*
         * this member could be also handled by the Driver class itsel, but not sure what the Driver do internally, 
         * so keep this member for know inside of this class
         */
        int client_fd;

        /**
         * Internal members to handle the connection
         */
        struct sockaddr_in cli_addr;
        
        /**
         * Internal members to handle the connection
         */
        socklen_t clilen;

};



};
