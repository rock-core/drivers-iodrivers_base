#include <iodrivers_base/Driver.hpp>
#include <iodrivers_base/Timeout.hpp>

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
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <netinet/udp.h>
#include <netdb.h>

#include <boost/lexical_cast.hpp>
#include <iodrivers_base/IOStream.hpp>
#include <iodrivers_base/IOListener.hpp>
#include <iodrivers_base/TestStream.hpp>

#ifdef __gnu_linux__
#include <linux/serial.h>
#include <termio.h>
#include <fcntl.h>
#include <err.h>
#endif

#ifdef __APPLE__
#ifndef B460800
#define B460800 460800
#define B576000 576000
#define B921600 921600
#endif
#endif

using namespace std;
using namespace iodrivers_base;

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

string Driver::binary_com(std::string const& str)
{ return binary_com(str.c_str(), str.size()); }
string Driver::binary_com(uint8_t const* str, size_t str_size)
{ return binary_com(reinterpret_cast<char const*>(str), str_size); }
string Driver::binary_com(char const* str, size_t str_size)
{
    std::ostringstream result;
    for (size_t i = 0; i < str_size; ++i)
    {
        unsigned int code = str[i];
        result << hex << ((code & 0xF0) >> 4) << hex << (code & 0xF);
        // result << (code & 0xFF) << std::endl;
    }
    return result.str();
}

Driver::Driver(int max_packet_size, bool extract_last)
    : internal_buffer(new uint8_t[max_packet_size]), internal_buffer_size(0)
    , MAX_PACKET_SIZE(max_packet_size)
    , m_stream(0), m_auto_close(true), m_extract_last(extract_last) 
{
    if(MAX_PACKET_SIZE <= 0)
        std::runtime_error("Driver: max_packet_size cannot be smaller or equal to 0!");
}

Driver::~Driver()
{
    delete[] internal_buffer;
    delete m_stream;
    for (set<IOListener*>::iterator it = m_listeners.begin(); it != m_listeners.end(); ++it)
        delete *it;
}

void Driver::setMainStream(IOStream* stream)
{
    delete m_stream;
    m_stream = stream;
}

IOStream* Driver::getMainStream() const
{
    return m_stream;
}

void Driver::addListener(IOListener* listener)
{
    m_listeners.insert(listener);
}

void Driver::removeListener(IOListener* listener)
{
    m_listeners.erase(listener);
}

void Driver::clear()
{
    if (m_stream)
        m_stream->clear();
    internal_buffer_size = 0;
}

Status Driver::getStatus() const
{
    m_stats.queued_bytes = internal_buffer_size;
    return m_stats;
}
void Driver::resetStatus()
{ m_stats = Status(); }

void Driver::setExtractLastPacket(bool flag) { m_extract_last = flag; }
bool Driver::getExtractLastPacket() const { return m_extract_last; }

void Driver::setFileDescriptor(int fd, bool auto_close)
{
    setMainStream(new FDStream(fd, auto_close));
}

int Driver::getFileDescriptor() const
{
    if (m_stream)
        return m_stream->getFileDescriptor();
    return FDStream::INVALID_FD;
}
bool Driver::isValid() const { return m_stream; }

void Driver::openURI(std::string const& uri)
{
    // Modes:
    //   0 for serial
    //   1 for TCP
    //   2 for UDP
    //   3 for UDP server
    //   4 for file (either Unix sockets or named FIFOs)
    int mode_idx = -1;
    char const* modes[6] = { "serial://", "tcp://", "udp://", "udpserver://", "file://", "test://" };
    for (int i = 0; i < 6; ++i)
    {
        if (uri.compare(0, strlen(modes[i]), modes[i]) == 0)
        {
            mode_idx = i;
            break;
        }
    }

    if (mode_idx == -1)
        throw std::runtime_error("unknown URI " + uri);

    string device = uri.substr(strlen(modes[mode_idx]));

    // Find a :[additional_info] marker
    string::size_type marker = device.find_last_of(":");
    int additional_info = 0;
    if (marker != string::npos)
    {
        additional_info = boost::lexical_cast<int>(device.substr(marker + 1));
        device = device.substr(0, marker);
    }

    if (mode_idx == 0)
    { // serial://DEVICE:baudrate
        if (marker == string::npos)
            throw std::runtime_error("missing baudrate specification in serial:// URI");
        openSerial(device, additional_info);
        return;
    }
    else if (mode_idx == 1)
    { // TCP tcp://hostname:port
        if (marker == string::npos)
            throw std::runtime_error("missing port specification in tcp:// URI");
        return openTCP(device, additional_info);
    }
    else if (mode_idx == 2)
    { // UDP udp://hostname:remoteport
        if (marker == string::npos)
            throw std::runtime_error("missing port specification in udp:// URI");

        string::size_type remote_port_marker = device.find_last_of(":");
        if (remote_port_marker != string::npos)
        {
            int remote_port = boost::lexical_cast<int>(device.substr(remote_port_marker + 1));
            device = device.substr(0, remote_port_marker);

            return openUDPBidirectional(device, remote_port, additional_info);
        }

        return openUDP(device, additional_info);
    }
    else if (mode_idx == 3)
    { // UDP udpserver://localport
        return openUDP("", boost::lexical_cast<int>(device));
    }
    else if (mode_idx == 4)
    { // file file://path
        return openFile(device);
    }
    else if (mode_idx == 5)
    { // test://
        if (!dynamic_cast<TestStream*>(getMainStream()))
            openTestMode();
    }
}

void Driver::openTestMode()
{
    setMainStream(new TestStream);
}

bool Driver::openSerial(std::string const& port, int baud_rate)
{
    setFileDescriptor(Driver::openSerialIO(port, baud_rate));
    return true;
}

bool Driver::openInet(const char *hostname, int port)
{
    openTCP(hostname, port);
    return true;
}

static int createIPServerSocket(int port, addrinfo const& hints)
{
    struct addrinfo *result;
    string port_as_string = boost::lexical_cast<string>(port);
    int ret = getaddrinfo(NULL, port_as_string.c_str(), &hints, &result);
    if (ret != 0)
        throw UnixError("cannot resolve server port " + port_as_string);

    int sfd = -1;
    struct addrinfo *rp;
    for (rp = result; rp != NULL; rp = rp->ai_next) {
        sfd = socket(rp->ai_family, rp->ai_socktype,
                rp->ai_protocol);
        if (sfd == -1)
            continue;

        if (::bind(sfd, rp->ai_addr, rp->ai_addrlen) == 0)
            break;                  /* Success */

        ::close(sfd);
    }
    freeaddrinfo(result);

    if (rp == NULL)
        throw UnixError("cannot open server socket on port " + port_as_string);

    return sfd;
}

static int createIPClientSocket(const char *hostname, const char *port, addrinfo const& hints, struct sockaddr *addr, size_t *addr_len)
{
    struct addrinfo *result;
    int ret = getaddrinfo(hostname, port, &hints, &result);
    if (ret != 0)
        throw UnixError("cannot resolve client port " + string(port));

    int sfd = -1;
    struct addrinfo *rp;
    for (rp = result; rp != NULL; rp = rp->ai_next) {
        sfd = socket(rp->ai_family, rp->ai_socktype,
                rp->ai_protocol);
        if (sfd == -1)
            continue;

        if (connect(sfd, rp->ai_addr, rp->ai_addrlen) == 0)
            break;                  /* Success */

        ::close(sfd);
    }

    if (rp == NULL)
    {
        freeaddrinfo(result);
        throw UnixError("cannot open client socket on port " + string(port));
    }

    if (addr != NULL) *addr = *(rp->ai_addr);
    if (addr_len != NULL) *addr_len = rp->ai_addrlen;

    freeaddrinfo(result);
    
    return sfd;
}

void Driver::openIPClient(std::string const& hostname, int port, addrinfo const& hints)
{
    int sfd = createIPClientSocket(hostname.c_str(), boost::lexical_cast<string>(port).c_str(), hints, NULL, NULL);
    setFileDescriptor(sfd);
}

void Driver::openTCP(std::string const& hostname, int port){
    struct addrinfo hints;
    memset(&hints, 0, sizeof(struct addrinfo));
    hints.ai_family = AF_UNSPEC;    /* Allow IPv4 or IPv6 */
    hints.ai_socktype = SOCK_STREAM; /* Datagram socket */
    openIPClient(hostname, port, hints);

    int fd = m_stream->getFileDescriptor();
    int nodelay_flag = 1;
    int result = setsockopt(fd,
            IPPROTO_TCP, TCP_NODELAY,
            (char *) &nodelay_flag, sizeof(int));
    if (result < 0)
    {
        close();
        throw UnixError("cannot set the TCP_NODELAY flag");
    }
}

void Driver::openUDP(std::string const& hostname, int port)
{
    if (hostname.empty())
    {
        struct addrinfo hints;
        memset(&hints, 0, sizeof(struct addrinfo));
        hints.ai_family = AF_UNSPEC;    /* Allow IPv4 or IPv6 */
        hints.ai_socktype = SOCK_DGRAM; /* Datagram socket */
        hints.ai_flags = AI_PASSIVE;    /* For wildcard IP address */
        
        int sfd = createIPServerSocket(port, hints);
        setMainStream(new UDPServerStream(sfd,true));
    }
    else
    {
        struct addrinfo hints;
        memset(&hints, 0, sizeof(struct addrinfo));
        hints.ai_family = AF_UNSPEC;    /* Allow IPv4 or IPv6 */
        hints.ai_socktype = SOCK_DGRAM; /* Datagram socket */
        openIPClient(hostname, port, hints);
    }
}

void Driver::openUDPBidirectional(std::string const& hostname, int out_port, int in_port)
{
    struct addrinfo in_hints;
    memset(&in_hints, 0, sizeof(struct addrinfo));
    in_hints.ai_family = AF_UNSPEC;    /* Allow IPv4 or IPv6 */
    in_hints.ai_socktype = SOCK_DGRAM; /* Datagram socket */
    in_hints.ai_flags = AI_PASSIVE;    /* For wildcard IP address */

    struct addrinfo out_hints;
    memset(&out_hints, 0, sizeof(struct addrinfo));
    out_hints.ai_family = AF_UNSPEC;    /* Allow IPv4 or IPv6 */
    out_hints.ai_socktype = SOCK_DGRAM; /* Datagram socket */

    struct sockaddr peer;
    size_t peer_len;
    int peerfd = createIPClientSocket(hostname.c_str(), boost::lexical_cast<string>(out_port).c_str(), out_hints, &peer, &peer_len);
    ::close(peerfd);    

    int sfd = createIPServerSocket(in_port, in_hints);
    setMainStream(new UDPServerStream(sfd, true, &peer, &peer_len));
}

int Driver::openSerialIO(std::string const& port, int baud_rate)
{
    int fd = ::open(port.c_str(), O_RDWR | O_NOCTTY | O_SYNC | O_NONBLOCK );
    if (fd == FDStream::INVALID_FD)
        throw UnixError("cannot open device " + port);

    FileGuard guard(fd);

    struct termios tio;
    memset(&tio, 0, sizeof(termios));

    tio.c_cflag = (CS8 | CREAD | CLOCAL);    // data bits = 8bit, enable reading 
					     // and ignore modem status line
    tio.c_iflag = IGNBRK; 		     // don't use breaks by default
    cfsetispeed(&tio, B9600);                // set baud rate to something
    cfsetospeed(&tio, B9600); 		     // otherwise tcsetattr complains on Mac OS

    // Commit
    if (tcsetattr(fd, TCSANOW, &tio)!=0)
        throw UnixError("Driver::openSerial cannot set serial options");

    if (!setSerialBaudrate(fd, baud_rate))
        throw UnixError("Driver::openSerial cannot set baudrate");

    guard.release();
    return fd;
}

void Driver::openFile(std::string const& path)
{
    int fd = ::open(path.c_str(), O_RDWR | O_SYNC | O_NONBLOCK );
    if (fd == FDStream::INVALID_FD)
        throw UnixError("cannot open file " + path);
    setFileDescriptor(fd);
}

bool Driver::setSerialBaudrate(int brate) {
    return setSerialBaudrate(getFileDescriptor(), brate);
}

bool Driver::setSerialBaudrate(int fd, int brate) {
    int tc_rate = 0;
#ifdef __gnu_linux__
    bool custom_rate = false;
#endif
    switch(brate) {
	case(SERIAL_1200):
	    tc_rate = B1200;
	    break;
	case(SERIAL_2400):
	    tc_rate = B2400;
	    break;
	case(SERIAL_4800):
	    tc_rate = B4800;
	    break;
        case(SERIAL_9600):
            tc_rate = B9600;
            break;
        case(SERIAL_19200):
            tc_rate = B19200;
            break;
        case(SERIAL_38400):
            tc_rate = B38400;
            break;
        case(SERIAL_57600):
            tc_rate = B57600;
            break;
        case(SERIAL_115200):
            tc_rate = B115200;
            break;
        case(SERIAL_230400):
            tc_rate = B230400;
            break;
        case(SERIAL_460800):
            tc_rate = B460800;
            break;
        case(SERIAL_576000):
            tc_rate = B576000;
            break;
        case(SERIAL_921600):
            tc_rate = B921600;
            break;
        default:
#ifdef __gnu_linux__
	    tc_rate = B38400;
	    custom_rate = true;
            std::cerr << "Using custom baud rate " << brate << std::endl;
#else
            std::cerr << "Non-standard baud rate selected. This is only supported on linux." << std::endl;
            return false;
#endif
    }

#ifdef __gnu_linux__
    struct serial_struct ss;
    ioctl(fd, TIOCGSERIAL, &ss);
    if( custom_rate )
    {
	ss.flags = (ss.flags & ~ASYNC_SPD_MASK) | ASYNC_SPD_CUST;
	ss.custom_divisor = (ss.baud_base + (brate / 2)) / brate;
	int closestSpeed = ss.baud_base / ss.custom_divisor;

	if (closestSpeed < brate * 98 / 100 || closestSpeed > brate * 102 / 100) 
	{
	    std::cerr << "Cannot set custom serial rate to " << brate 
		<< ". The closest possible value is " << closestSpeed << "."
		<< std::endl;
	}
    }
    else
    {
	ss.flags &= ~ASYNC_SPD_MASK;
    }
    ioctl(fd, TIOCSSERIAL, &ss);
#endif

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
    delete m_stream;
    m_stream = 0;
}

std::pair<uint8_t const*, int> Driver::findPacket(uint8_t const* buffer, int buffer_size) const
{
    int packet_start = 0, packet_size = 0;
    int extract_result = extractPacket(buffer, buffer_size);
    
    // make sure the returned packet size is not longer than
    // the buffer
    if( extract_result > buffer_size )
        throw length_error("extractPacket() returned result size "
                + boost::lexical_cast<string>(extract_result) 
                + ", which is larger than the buffer size " 
                + boost::lexical_cast<string>(buffer_size) + ".");

    if (0 == extract_result)
        return make_pair(buffer, 0);

    if (extract_result < 0)
        packet_start += -extract_result;
    else if (extract_result > 0)
        packet_size = extract_result;

    if (m_extract_last)
    {
        m_stats.stamp = base::Time::now();
        m_stats.bad_rx  += packet_start;
        m_stats.good_rx += packet_size;
    }

    int remaining = buffer_size - (packet_start + packet_size);

    if (remaining == 0)
        return make_pair(buffer + packet_start, packet_size);

    if (!packet_size || (packet_size > 0 && m_extract_last))
    {
        // Recursively call findPacket to find a packet in the current internal
        // buffer. This is used either if the last call to extractPacket
        // returned a negative value (remove bytes at the front of the buffer),
        // or if m_extract_last is true (we are looking for the last packet in
        // buffer)
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
    if (!m_extract_last)
    {
        m_stats.stamp = base::Time::now();
        m_stats.bad_rx  += packet.first - internal_buffer;
        m_stats.good_rx += packet.second;
    }
    // cerr << "found packet " << printable_com(packet.first, packet.second) << " in internal buffer" << endl;

    int buffer_rem = internal_buffer_size - (packet.first + packet.second - internal_buffer);
    memcpy(buffer, packet.first, packet.second);
    memmove(internal_buffer, packet.first + packet.second, buffer_rem);
    internal_buffer_size = buffer_rem;

    return packet.second;
}

pair<int, bool> Driver::extractPacketFromInternalBuffer(uint8_t* buffer, int out_buffer_size)
{
    // How many packet bytes are there currently in +buffer+
    int packet_size = 0;
    int result_size = 0;
    while (internal_buffer_size > 0)
    {
        packet_size = doPacketExtraction(buffer);

        // after doPacketExtraction, if a packet is there it has already been
        // copied in 'buffer'
        if (packet_size)
            result_size = packet_size;

        if (!packet_size || !m_extract_last)
            break;
    }
    return make_pair(result_size, false);
}

pair<int, bool> Driver::readPacketInternal(uint8_t* buffer, int out_buffer_size)
{
    if (out_buffer_size < MAX_PACKET_SIZE)
        throw length_error("readPacket(): provided buffer too small (got " + boost::lexical_cast<string>(out_buffer_size) + ", expected at least " + boost::lexical_cast<string>(MAX_PACKET_SIZE) + ")");

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
        int c = m_stream->read(internal_buffer + internal_buffer_size, MAX_PACKET_SIZE - internal_buffer_size);
        if (c > 0) {
            for (set<IOListener*>::iterator it = m_listeners.begin(); it != m_listeners.end(); ++it)
                (*it)->readData(internal_buffer + internal_buffer_size, c);
	  
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
        else
            return make_pair(packet_size, received_something);

        if (internal_buffer_size == (size_t)MAX_PACKET_SIZE)
            throw length_error("readPacket(): current packet too large for buffer");
    }

    // Never reached
}

bool Driver::hasPacket() const
{
    if (internal_buffer_size == 0)
        return false;

    pair<uint8_t const*, int> packet = findPacket(internal_buffer, internal_buffer_size);
    return (packet.second > 0);
}

void Driver::setReadTimeout(base::Time const& timeout)
{ m_read_timeout = timeout; }
base::Time Driver::getReadTimeout() const
{ return m_read_timeout; }
int Driver::readPacket(uint8_t* buffer, int buffer_size)
{
    return readPacket(buffer, buffer_size, getReadTimeout());
}
int Driver::readPacket(uint8_t* buffer, int buffer_size,
        base::Time const& packet_timeout)
{
    return readPacket(buffer, buffer_size, packet_timeout,
            packet_timeout + base::Time::fromSeconds(1));
}
int Driver::readPacket(uint8_t* buffer, int buffer_size,
        base::Time const& packet_timeout, base::Time const& first_byte_timeout)
{
    return readPacket(buffer, buffer_size, packet_timeout.toMilliseconds(), 
            first_byte_timeout.toMilliseconds());
}
int Driver::readPacket(uint8_t* buffer, int buffer_size, int packet_timeout, int first_byte_timeout)
{
    if (first_byte_timeout > packet_timeout)
        first_byte_timeout = -1;

    if (buffer_size < MAX_PACKET_SIZE)
        throw length_error("readPacket(): provided buffer too small (got "
                + boost::lexical_cast<string>(buffer_size) + ", expected at least "
                + boost::lexical_cast<string>(MAX_PACKET_SIZE) + ")");

    if (!isValid())
    {
        // No valid file descriptor. Assume that the user is using the raw data
        // interface (i.e. that the data is already in the internal read buffer)
        pair<int, bool> result = extractPacketFromInternalBuffer(buffer, buffer_size);
        if (result.first)
            return result.first;
        else
            throw TimeoutError(TimeoutError::PACKET,
                    "readPacket(): no packet in the internal buffer and no FD to read from");
    }

    if(!m_stream)
        throw std::runtime_error("Driver::writePacket : invalid stream, did you forget to call open ?");

    Timeout time_out;
    bool read_something = false;
    while(true) {
	
        pair<int, bool> read_state = readPacketInternal(buffer, buffer_size);
            
        int packet_size = read_state.first;
            
        read_something = read_something || read_state.second;
        
        if (packet_size > 0)
            return packet_size;

        // if there was no data to read _and_ packet_timeout is zero, we'll throw
        if (packet_timeout == 0)
            throw TimeoutError(TimeoutError::FIRST_BYTE,
                    "readPacket(): no data to read while a packet_timeout of 0 was given");

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
        {
            throw TimeoutError(timeout_type,
                "readPacket(): no data after waiting "
                + boost::lexical_cast<string>(timeout) + "ms");
        }

        // we still have time left to wait for arriving data. see how much
        int remaining_timeout = time_out.timeLeft(timeout);
        try {
            // calls select and waits until a new read can be actually performed (in the next
            // while-iteration)
            m_stream->waitRead(base::Time::fromMicroseconds(remaining_timeout * 1000));
        }
        catch(TimeoutError& e)
        {
            throw TimeoutError(timeout_type,
                "readPacket(): no data after retrying with remaining time "
                + boost::lexical_cast<string>(remaining_timeout) + "ms of "
                + boost::lexical_cast<string>(timeout) +"ms timeout");
        }
    }
}

void Driver::setWriteTimeout(base::Time const& timeout)
{ m_write_timeout = timeout; }
base::Time Driver::getWriteTimeout() const
{ return m_write_timeout; }

bool Driver::writePacket(uint8_t const* buffer, int buffer_size)
{
    return writePacket(buffer, buffer_size, getWriteTimeout());
}
bool Driver::writePacket(uint8_t const* buffer, int buffer_size, base::Time const& timeout)
{ return writePacket(buffer, buffer_size, timeout.toMilliseconds()); }
bool Driver::writePacket(uint8_t const* buffer, int buffer_size, int timeout)
{
    if(!m_stream)
        throw std::runtime_error("Driver::writePacket : invalid stream, did you forget to call open ?");
    
    Timeout time_out(timeout);
    int written = 0;
    while(true) {
        int c = m_stream->write(buffer + written, buffer_size - written);
        for (set<IOListener*>::iterator it = m_listeners.begin(); it != m_listeners.end(); ++it)
            (*it)->writeData(buffer + written, c);
        written += c;

        if (written == buffer_size) {
            m_stats.stamp = base::Time::now();
	    m_stats.tx += buffer_size;
            return true;
        }
        
        if (time_out.elapsed())
            throw TimeoutError(TimeoutError::PACKET, "writePacket(): timeout");

        int remaining_timeout = time_out.timeLeft();
        m_stream->waitWrite(base::Time::fromMicroseconds(remaining_timeout * 1000));
    }
}

