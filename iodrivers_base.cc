#include "iodrivers_base.hh"
#include <errno.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

#include <termios.h>
#include <unistd.h>

#include <sys/time.h>
#include <time.h>

#include <cstring>
#include <sstream>
#include <iostream>

using namespace std;

unix_error::unix_error(std::string const& desc)
    : std::runtime_error(desc + ": " + strerror(errno)), error(errno) {}


static string printable_com(string const& buffer)
{
    char const* str = buffer.c_str();
    size_t str_size = buffer.size();
    ostringstream result;
    for (size_t i = 0; i < str_size; ++i)
    {
        if (str[i] == '\n')
            result << "\\n";
        else if (str[i] == '\r')
            result << "\\r";
        else
            result << str[i];
    }
    return result.str();
}


IODriver::IODriver(int max_packet_size)
    : internal_buffer(new uint8_t[max_packet_size]), internal_buffer_size(0)
    , MAX_PACKET_SIZE(max_packet_size)
    , m_fd(INVALID_FD), m_auto_close(true) {}

IODriver::~IODriver()
{
    delete[] internal_buffer;
    if (isValid() && m_auto_close)
        close();
}

void IODriver::setFileDescriptor(int fd, bool auto_close)
{
    if (isValid() && m_auto_close)
        close();

    m_auto_close = auto_close;
    m_fd = fd;
}

int IODriver::getFileDescriptor() const { return m_fd; }
bool IODriver::isValid() const { return m_fd != INVALID_FD; }

bool IODriver::openSerial(std::string const& port, int baud_rate)
{
    m_fd = ::open(port.c_str(), O_RDWR | O_NOCTTY | O_SYNC | O_NONBLOCK );
    if (m_fd == INVALID_FD)
        return false;

    file_guard guard(m_fd);

    struct termios tio;
    tcgetattr(m_fd,&tio);

    tio.c_cflag=(tio.c_cflag & ~CSIZE) | CS8; // data bits = 8bit

    tio.c_iflag&= ~( BRKINT | ICRNL | ISTRIP );
    tio.c_iflag&= ~ IXON;    // no XON/XOFF
    tio.c_cflag&= ~PARENB;   // no parity
#ifndef LINUX
    tio.c_cflag&= ~CRTSCTS;  // no CTS/RTS
    tio.c_cflag&= ~CSTOPB;   // stop bit = 1bit
#endif

#ifdef CYGWIN
    tio.c_cc[VMIN] = 1;
    tio.c_cc[VTIME] = 1;
#endif

    // Other
    tio.c_lflag &= ~( ISIG | ICANON | ECHO );
    
    // Commit
    if (tcsetattr(m_fd,TCSADRAIN,&tio)!=0)
        return false;

    if (!setSerialBaudrate(baud_rate))
        return false;

    guard.release();
    return true;
}

bool IODriver::setSerialBaudrate(int brate) {
    int tc_rate = 0;
    switch(brate) {
        case(19200):
            tc_rate = B19200;
            break;
        case(38400):
            tc_rate = B38400;
            break;
        case(57600):
            tc_rate = B57600;
            break;
        case(115200):
            tc_rate = B115200;
            break;
        default:
            std::cerr << "invalid baud rate " << brate << std::endl;
            return false;
    }

    struct termios termios_p;
    if(tcgetattr(m_fd, &termios_p)){
        perror("Failed to get terminal info \n");    
        return false;
    }

    if(cfsetispeed(&termios_p, tc_rate)){
        perror("Failed to set terminal input speed \n");    
        return false;
    }

    if(cfsetospeed(&termios_p, tc_rate)){
        perror("Failed to set terminal output speed \n");    
        return false;
    }

    if(tcsetattr(m_fd, TCSANOW, &termios_p)) {
        perror("Failed to set speed \n");    
        return false;
    }
    return true;
}

void IODriver::close()
{
    ::close(m_fd);
    m_fd = INVALID_FD;
}

int IODriver::readPacketInternal(uint8_t* buffer, int buffer_size)
{
    if (buffer_size < MAX_PACKET_SIZE)
        throw length_error("readPacket(): provided buffer too small");

    if (internal_buffer_size > 0)
    {
        //cerr << internal_buffer_size << " bytes remaining in internal buffer" << endl;

        // Search for the end of packet in the internal buffer
        int packet_size = extractPacket(internal_buffer, internal_buffer_size);
        if (packet_size > 0)
        {
            memcpy(buffer, internal_buffer, packet_size);
            internal_buffer_size -= packet_size;
            memmove(internal_buffer, internal_buffer + packet_size, internal_buffer_size);
            //cerr << "got packet " << printable_com(string(buffer, buffer + packet_size)) << endl;
            return packet_size;
        }

        memcpy(buffer, internal_buffer, internal_buffer_size);
    }

    uint8_t* buffer_end = buffer + internal_buffer_size;
    internal_buffer_size = 0;

    while (true) {
        int c = ::read(m_fd, buffer_end, MAX_PACKET_SIZE - (buffer_end - buffer));
        if (c > 0) {
            //cerr << "received: " << printable_com(string(buffer_end, buffer_end + c)) << " (" << c << ")" << endl;
            buffer_end += c;

            //cerr << "buffer:   "  << printable_com(string(buffer, buffer_end)) << " (" << buffer_end - buffer << ")" << endl;
            
            int packet_size = extractPacket(buffer, buffer_end - buffer);
            if (packet_size > 0)
            {
                int buffer_size = buffer_end - buffer;
                memcpy(internal_buffer, buffer + packet_size, buffer_size - packet_size);
                internal_buffer_size = buffer_size - packet_size;
                //cerr << "got packet " << printable_com(string(buffer, buffer + packet_size)) << endl;
                return packet_size;
            }
        }
        else if (c < 0)
        {
            if (errno == EAGAIN)
            {
                internal_buffer_size = buffer_end - buffer;
                memcpy(internal_buffer, buffer, internal_buffer_size);
                return 0;
            }

            throw unix_error("readPacket(): error reading the file descriptor");
        }

        if (buffer_end == buffer + MAX_PACKET_SIZE)
            throw length_error("readPacket(): current packet too large for buffer");
    }

    // Never reached
}

int IODriver::readPacket(uint8_t* buffer, int buffer_size, int timeout)
{
    timeval start_time;
    gettimeofday(&start_time, 0);
    while(true) {
        int packet_size = readPacketInternal(buffer, buffer_size);
        if (packet_size > 0)
            return packet_size;
        
        timeval current_time;
        gettimeofday(&current_time, 0);

        int elapsed = 
            (current_time.tv_sec - start_time.tv_sec) * 1000
            + (static_cast<int>(current_time.tv_usec) -
                    static_cast<int>(start_time.tv_usec)) / 1000;
        if (elapsed > timeout)
            throw timeout_error("readPacket(): timeout");

        int remaining_timeout = timeout - elapsed;

        fd_set set;
        FD_ZERO(&set);
        FD_SET(m_fd, &set);

        timeval timeout_spec = { remaining_timeout / 1000, (remaining_timeout % 1000) * 1000 };
        int ret = select(m_fd + 1, &set, NULL, NULL, &timeout_spec);
        if (ret < 0)
            throw unix_error("readPacket(): error in select()");
        else if (ret == 0)
            throw timeout_error("readPacket(): timeout");
    }
}

bool IODriver::writePacket(uint8_t const* buffer, int buffer_size, int timeout)
{
    timeval start_time;
    gettimeofday(&start_time, 0);
    int written = 0;
    while(true) {
        int c = write(m_fd, buffer + written, buffer_size - written);
        if (c == -1 && errno != EAGAIN)
            throw unix_error("writePacket(): error during write");
        written += c;
        if (written == buffer_size)
            return true;
        
        timeval current_time;
        gettimeofday(&current_time, 0);

        int elapsed = 
            (current_time.tv_sec - start_time.tv_sec) * 1000
            + (static_cast<int>(current_time.tv_usec) -
                    static_cast<int>(start_time.tv_usec)) / 1000;
        if (elapsed > timeout)
            throw timeout_error("writePacket(): timeout");

        int remaining_timeout = timeout - elapsed;

        fd_set set;
        FD_ZERO(&set);
        FD_SET(m_fd, &set);

        timeval timeout_spec = { remaining_timeout / 1000, (remaining_timeout % 1000) * 1000 };
        int ret = select(m_fd + 1, NULL, &set, NULL, &timeout_spec);
        if (ret < 0)
            throw unix_error("writePacket(): error in select()");
        else if (ret == 0)
            throw timeout_error("writePacket(): timeout");
    }
}

