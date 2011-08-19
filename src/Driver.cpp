#include <iodrivers_base/Driver.hpp>
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

#include <sys/socket.h>
#include <netdb.h>

using namespace std;
using namespace iodrivers_base;

UnixError::UnixError(std::string const& desc)
    : std::runtime_error(desc + ": " + strerror(errno)), error(errno) {}

UnixError::UnixError(std::string const& desc, int error_code)
    : std::runtime_error(desc + ": " + strerror(error_code)), error(error_code) {}


Timeout::Timeout(unsigned int timeout)
    : timeout(timeout) {
    gettimeofday(&start_time, 0);
}

void Timeout::restart() {
    gettimeofday(&start_time, 0);
}

bool Timeout::elapsed() const
{
    return elapsed(timeout);
}

bool Timeout::elapsed(unsigned int timeout) const
{
    timeval current_time;
    gettimeofday(&current_time, 0);
    unsigned int elapsed = 
	(current_time.tv_sec - start_time.tv_sec) * 1000
	+ (static_cast<int>(current_time.tv_usec) -
	   static_cast<int>(start_time.tv_usec)) / 1000;
    return timeout < elapsed;
}

unsigned int Timeout::timeLeft() const
{
    return timeLeft(timeout);
}

unsigned int Timeout::timeLeft(unsigned int timeout) const
{
    timeval current_time;
    gettimeofday(&current_time, 0);
    int elapsed = 
	(current_time.tv_sec - start_time.tv_sec) * 1000
	+ (static_cast<int>(current_time.tv_usec) -
	   static_cast<int>(start_time.tv_usec)) / 1000;
    if ((int)timeout < elapsed)
	return 0;
    return timeout - elapsed;
}


string Driver::printable_com(std::string const& str)
{ return printable_com(str.c_str(), str.size()); }
string Driver::printable_com(uint8_t const* str, size_t str_size)
{ return printable_com(reinterpret_cast<char const*>(str), str_size); }
string Driver::printable_com(char const* str, size_t str_size)
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


Driver::Driver(int max_packet_size, bool extract_last)
    : internal_buffer(new uint8_t[max_packet_size]), internal_buffer_size(0)
    , MAX_PACKET_SIZE(max_packet_size)
    , m_fd(INVALID_FD), m_auto_close(true), m_extract_last(extract_last) {}

Driver::~Driver()
{
    delete[] internal_buffer;
    if (isValid() && m_auto_close)
        close();
}

void Driver::clear()
{
    uint8_t buffer[1024];
    while (read(m_fd, buffer, 1024) > 0);
}

Driver::Statistics Driver::getStats() const
{ return m_stats; }
void Driver::resetStats()
{ m_stats = Statistics(); }

void Driver::setExtractLastPacket(bool flag) { m_extract_last = flag; }
bool Driver::getExtractLastPacket() const { return m_extract_last; }

void Driver::setFileDescriptor(int fd, bool auto_close)
{
    if (isValid() && m_auto_close)
        close();

    long fd_flags = fcntl(fd, F_GETFL);
    if (!(fd_flags & O_NONBLOCK))
    {
        cerr << "WARN: FD given to Driver::setFileDescriptor is set as blocking, setting the NONBLOCK flag" << endl;
        if (fcntl(fd, F_SETFL, fd_flags | O_NONBLOCK) == -1)
            throw UnixError("cannot set the O_NONBLOCK flag");
    }

    m_auto_close = auto_close;
    m_fd = fd;
}

int Driver::getFileDescriptor() const { return m_fd; }
bool Driver::isValid() const { return m_fd != INVALID_FD; }

bool Driver::openSerial(std::string const& port, int baud_rate)
{
    m_fd = Driver::openSerialIO(port, baud_rate);
    return m_fd != INVALID_FD;
}

bool Driver::openInet(const char *hostname, int port){
	m_fd = socket(AF_INET, SOCK_STREAM ,0 );
	if(m_fd == 0){
		m_fd = INVALID_FD;
		return false;
	}



	struct hostent *server = gethostbyname(hostname);
	if(server == 0){
		shutdown(m_fd,SHUT_RDWR);
		m_fd = INVALID_FD;
		return false;
	}
	struct sockaddr_in serv_addr;
	bzero((char *) &serv_addr, sizeof(serv_addr));
	serv_addr.sin_family = AF_INET;
	bcopy((char *)server->h_addr, (char *)&serv_addr.sin_addr.s_addr,server->h_length);
	serv_addr.sin_port = htons(port);
	if (connect(m_fd,(struct sockaddr*)&serv_addr,sizeof(serv_addr)) < 0){
		shutdown(m_fd,SHUT_RDWR);
		m_fd = INVALID_FD;
		return false;
	}


	//Need to set this after connecting, otherwise we are not now that we are sucsesfully connected
	long fd_flags = fcntl(m_fd, F_GETFL);
	if (!(fd_flags & O_NONBLOCK))
	{
	    if (fcntl(m_fd, F_SETFL, fd_flags | O_NONBLOCK) == -1){
	    	cerr << "Canot set nonblock" << std::endl;
	        throw UnixError("cannot set the O_NONBLOCK flag\n");
	    }
	}

	return true;
}

int Driver::openSerialIO(std::string const& port, int baud_rate)
{
    int fd = ::open(port.c_str(), O_RDWR | O_NOCTTY | O_SYNC | O_NONBLOCK );
    if (fd == INVALID_FD)
        return INVALID_FD;

    FileGuard guard(fd);

    struct termios tio;
    memset(&tio, 0, sizeof(termios));
    tio.c_cflag = CS8 | CREAD;    // data bits = 8bit and enable receiver
    tio.c_iflag = IGNBRK; // don't use breaks by default

    // Commit
    if (tcsetattr(fd, TCSANOW, &tio)!=0)
    {
        cerr << "Driver::openSerial cannot set serial options" << endl;
        return INVALID_FD;
    }

    if (!setSerialBaudrate(fd, baud_rate))
    {
        cerr << "Driver::openSerial cannot set baud rate" << endl;
        return INVALID_FD;
    }

    guard.release();
    return fd;
}

bool Driver::setSerialBaudrate(int brate) {
    return setSerialBaudrate(m_fd, brate);
}

bool Driver::setSerialBaudrate(int fd, int brate) {
    int tc_rate = 0;
    switch(brate) {
	case(1200): 
	    tc_rate = B1200; 
	    break;
	case(2400): 
	    tc_rate = B2400; 
	    break;
	case(4800): 
	    tc_rate = B4800; 
	    break;
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
    if(tcgetattr(fd, &termios_p)){
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

    if(tcsetattr(fd, TCSANOW, &termios_p)) {
        perror("Failed to set speed \n");    
        return false;
    }
    return true;
}

void Driver::close()
{
    ::close(m_fd);
    m_fd = INVALID_FD;
}

std::pair<uint8_t const*, int> Driver::findPacket(uint8_t const* buffer, int buffer_size)
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

int Driver::doPacketExtraction(uint8_t* buffer)
{
    pair<uint8_t const*, int> packet = findPacket(internal_buffer, internal_buffer_size);
    m_stats.bad_rx  += packet.first - internal_buffer;
    m_stats.good_rx += packet.second;
    // cerr << "found packet " << printable_com(packet.first, packet.second) << " in internal buffer" << endl;

    int buffer_rem = internal_buffer_size - (packet.first + packet.second - internal_buffer);
    memcpy(buffer, packet.first, packet.second);
    memmove(internal_buffer, packet.first + packet.second, buffer_rem);
    internal_buffer_size = buffer_rem;

    return packet.second;
}

pair<int, bool> Driver::readPacketInternal(uint8_t* buffer, int out_buffer_size)
{
    if (out_buffer_size < MAX_PACKET_SIZE)
        throw length_error("readPacket(): provided buffer too small");

    // How many packet bytes are there currently in +buffer+
    int packet_size = 0;
    if (internal_buffer_size > 0)
    {
        packet_size = doPacketExtraction(buffer);
        // after doPacketExtraction, if a packet is there it has already been
        // copied in 'buffer'
        if (packet_size && !m_extract_last)
            return make_pair(packet_size, false);
    }

    bool received_something = false;
    while (true) {
        // cerr << "reading with " << printable_com(buffer, buffer_size) << " as buffer" << endl;
        int c = ::read(m_fd, internal_buffer + internal_buffer_size, MAX_PACKET_SIZE - internal_buffer_size);
        if (c > 0) {
	  
            received_something = true;

            // cerr << "received: " << printable_com(buffer + buffer_size, c) << endl;
            internal_buffer_size += c;

            int new_packet = doPacketExtraction(buffer);
            if (new_packet)
            {
                if (!m_extract_last)
                    return make_pair(new_packet, true);
                else
                    packet_size = new_packet;
            }
        }
        else if (c == 0)
        {
            // this is EOF, but some serial-to-USB drivers use it to indicate
            // a blocking call. Anyway, select() in readPacket() will
            // discriminate and raise a timeout if needed.
            return make_pair(packet_size, received_something);
        }
        else if (c < 0)
        {
            if (errno == EAGAIN)
                return make_pair(packet_size, received_something);

            throw UnixError("readPacket(): error reading the file descriptor");
        }

        if (internal_buffer_size == (size_t)MAX_PACKET_SIZE)
            throw length_error("readPacket(): current packet too large for buffer");
    }

    // Never reached
}

int Driver::readPacket(uint8_t* buffer, int buffer_size, int packet_timeout, int first_byte_timeout)
{
  
    Timeout time_out;
    bool read_something = false;
    while(true) {
        // cerr << endl;
	
	pair<int, bool> read_state = readPacketInternal(buffer, buffer_size);
        
	int packet_size     = read_state.first;
        
	read_something = read_something || read_state.second;
	
	if (packet_size > 0)
            return packet_size;
        
        int timeout;
        TimeoutError::TIMEOUT_TYPE timeout_type;
        if (first_byte_timeout != -1 && !read_something)
        {
            timeout = first_byte_timeout;
            timeout_type = TimeoutError::FIRST_BYTE;
        }
        else
        {
            timeout = packet_timeout;
            timeout_type = TimeoutError::PACKET;
        }

        if (time_out.elapsed(timeout))
            throw TimeoutError(timeout_type, "readPacket(): timeout");

        int remaining_timeout = time_out.timeLeft(timeout);

        fd_set set;
        FD_ZERO(&set);
        FD_SET(m_fd, &set);

        timeval timeout_spec = { remaining_timeout / 1000, (remaining_timeout % 1000) * 1000 };
        int ret = select(m_fd + 1, &set, NULL, NULL, &timeout_spec);
        if (ret < 0)
            throw UnixError("readPacket(): error in select()");
        else if (ret == 0)
            throw TimeoutError(timeout_type, "readPacket(): timeout");
    }
}

bool Driver::writePacket(uint8_t const* buffer, int buffer_size, int timeout)
{
    Timeout time_out(timeout);
    int written = 0;
    while(true) {
        int c = write(m_fd, buffer + written, buffer_size - written);
        if (c == -1 && errno != EAGAIN && errno != ENOBUFS)
            throw UnixError("writePacket(): error during write");
        else if (c != -1)
            written += c;

        if (written == buffer_size) {
	    m_stats.tx += buffer_size;
            return true;
        }
        
        if (time_out.elapsed())
            throw TimeoutError(TimeoutError::PACKET, "writePacket(): timeout");

        int remaining_timeout = time_out.timeLeft();

        fd_set set;
        FD_ZERO(&set);
        FD_SET(m_fd, &set);

        timeval timeout_spec = { remaining_timeout / 1000, (remaining_timeout % 1000) * 1000 };
        int ret = select(m_fd + 1, NULL, &set, NULL, &timeout_spec);
        if (ret < 0)
            throw UnixError("writePacket(): error in select()");
        else if (ret == 0)
            throw TimeoutError(TimeoutError::PACKET, "writePacket(): timeout");
    }
}

