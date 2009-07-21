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

unix_error::unix_error(std::string const& desc, int error_code)
    : std::runtime_error(desc + ": " + strerror(error_code)), error(error_code) {}


string IODriver::printable_com(std::string const& str)
{ return printable_com(str.c_str(), str.size()); }
string IODriver::printable_com(uint8_t const* str, size_t str_size)
{ return printable_com(reinterpret_cast<char const*>(str), str_size); }
string IODriver::printable_com(char const* str, size_t str_size)
{
    ostringstream result;
    result << "\"";
    for (size_t i = 0; i < str_size; ++i)
    {
        if (str[i] == 0)
            result << "\\x00";
        else if (str[i] == '\n')
            result << "\\n";
        else if (str[i] == '\r')
            result << "\\r";
        else
            result << str[i];
    }
    result << "\"";
    return result.str();
}


IODriver::IODriver(int max_packet_size, bool extract_last)
    : internal_buffer(new uint8_t[max_packet_size]), internal_buffer_size(0)
    , MAX_PACKET_SIZE(max_packet_size)
    , m_fd(INVALID_FD), m_auto_close(true), m_extract_last(extract_last) {}

IODriver::~IODriver()
{
    delete[] internal_buffer;
    if (isValid() && m_auto_close)
        close();
}

void IODriver::clear()
{
    uint8_t buffer[1024];
    while (read(m_fd, buffer, 1024) > 0);
}

void IODriver::setExtractLastPacket(bool flag) { m_extract_last = flag; }
bool IODriver::getExtractLastPacket() const { return m_extract_last; }

void IODriver::setFileDescriptor(int fd, bool auto_close)
{
    if (isValid() && m_auto_close)
        close();

    long fd_flags = fcntl(fd, F_GETFL);
    if (!(fd_flags & O_NONBLOCK))
    {
        cerr << "WARN: FD given to IODriver::setFileDescriptor is set as blocking, setting the NONBLOCK flag" << endl;
        if (fcntl(fd, F_SETFL, fd_flags | O_NONBLOCK) == -1)
            throw unix_error("cannot set the O_NONBLOCK flag");
    }

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
        case(9600):
            tc_rate = B9600;
            break;
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

std::pair<uint8_t const*, int> IODriver::findPacket(uint8_t const* buffer, int buffer_size)
{
    int packet_start = 0, packet_size = 0;
    int extract_result = extractPacket(buffer, buffer_size);

    if (0 == extract_result)
        return make_pair(buffer, 0);

    if (extract_result < 0)
        packet_start += -extract_result;
    else if (extract_result > 0)
        packet_size = extract_result;

    int remaining = buffer_size - (packet_start + packet_size);

    if (remaining == 0)
        return make_pair(buffer + packet_start, packet_size);

    if (!packet_size || (packet_size > 0 && m_extract_last))
    {
        std::pair<uint8_t const*, int> next_packet;
        next_packet = findPacket(buffer + packet_start + packet_size, remaining);

        if (m_extract_last)
        {
            if (next_packet.second == 0)
                return make_pair(buffer + packet_start, packet_size);
            else
                return next_packet;
        }
        else
        {
            return next_packet;
        }
    }
    return make_pair(buffer + packet_start, packet_size);
}

int IODriver::doPacketExtraction(uint8_t* buffer)
{
    pair<uint8_t const*, int> packet = findPacket(internal_buffer, internal_buffer_size);
    // cerr << "found packet " << printable_com(packet.first, packet.second) << " in internal buffer" << endl;

    int buffer_rem = internal_buffer_size - (packet.first + packet.second - internal_buffer);
    memcpy(buffer, packet.first, packet.second);
    memmove(internal_buffer, packet.first + packet.second, buffer_rem);
    internal_buffer_size = buffer_rem;

    return packet.second;
}

int IODriver::readPacketInternal(uint8_t* buffer, int out_buffer_size)
{
    if (out_buffer_size < MAX_PACKET_SIZE)
        throw length_error("readPacket(): provided buffer too small");

    // How many packet bytes are there currently in +buffer+
    int current_buffer_state = 0;
    if (internal_buffer_size > 0)
    {
        current_buffer_state = doPacketExtraction(buffer);
        if (current_buffer_state && !m_extract_last)
            return current_buffer_state;
    }

    while (true) {
        // cerr << "reading with " << printable_com(buffer, buffer_size) << " as buffer" << endl;
        int c = ::read(m_fd, internal_buffer + internal_buffer_size, MAX_PACKET_SIZE - internal_buffer_size);
        if (c > 0) {
            // cerr << "received: " << printable_com(buffer + buffer_size, c) << endl;
            internal_buffer_size += c;

            int new_packet = doPacketExtraction(buffer);
            if (new_packet)
            {
                if (!m_extract_last)
                    return new_packet;
                else
                    current_buffer_state = new_packet;
            }
        }
        else if (c < 0)
        {
            if (errno == EAGAIN)
                return current_buffer_state;

            throw unix_error("readPacket(): error reading the file descriptor");
        }

        if (internal_buffer_size == (size_t)MAX_PACKET_SIZE)
            throw length_error("readPacket(): current packet too large for buffer");
    }

    // Never reached
}

int IODriver::readPacket(uint8_t* buffer, int buffer_size, int timeout)
{
    timeval start_time;
    gettimeofday(&start_time, 0);
    while(true) {
        // cerr << endl;
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
        else if (c != -1)
            written += c;

        if (written == buffer_size) {
            return true;
        }
        
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

