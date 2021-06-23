#include <base-logging/Logging.hpp>
#include <iodrivers_base/Driver.hpp>
#include <iodrivers_base/Timeout.hpp>
#include <iodrivers_base/URI.hpp>

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
#include <stdexcept>

#include <sys/socket.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <netinet/udp.h>
#include <netdb.h>

#include <boost/regex.hpp>
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
using base::Time;
using namespace iodrivers_base;
using boost::lexical_cast;

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

void Driver::setFileDescriptor(int fd, bool auto_close, bool has_eof)
{
    setMainStream(new FDStream(fd, auto_close, has_eof));
}

int Driver::getFileDescriptor() const
{
    if (m_stream)
        return m_stream->getFileDescriptor();
    return FDStream::INVALID_FD;
}
bool Driver::isValid() const { return m_stream; }

static void validateURIScheme(std::string const& scheme) {
    char const* knownSchemes[7] = { "serial", "tcp", "tcpserver", "udp", "udpserver", "file", "test" };
    for (int i = 0; i < 7; ++i) {
        if (scheme == knownSchemes[i]) {
            return;
        }
    }

    throw std::runtime_error("unknown scheme " + scheme);
}

/** Backward-compatibility code to handle the old syntax udp://host:remote_port:local_port
 *
 * It transforms it into the new udp://host:remote_port?local_port=PORT URI
 */
static URI backwardParseBidirectionalUDP(string const& uri_string) {
    size_t new_style = uri_string.find_first_of("?&=");
    if (new_style != string::npos) {
        return URI::parse(uri_string);
    }

    size_t first_colon = uri_string.find_first_of(":", 6);
    size_t last_colon = uri_string.find_last_of(":");

    if (first_colon == last_colon) {
        return URI::parse(uri_string);
    }

    string local_port = uri_string.substr(last_colon + 1, string::npos);
    string new_uri = uri_string.substr(0, last_colon) + "?local_port=" + local_port;
    return URI::parse(new_uri);
}

void Driver::openURI(std::string const& uri_string) {
    URI uri;
    if (uri_string.substr(0, 6) == "udp://") {
        uri = backwardParseBidirectionalUDP(uri_string);
    }
    else {
        uri = URI::parse(uri_string);
    }
    string scheme = uri.getScheme();
    validateURIScheme(scheme);

    if (scheme == "serial") { // serial://DEVICE:baudrate
        if (uri.getPort() == 0) {
            throw std::invalid_argument("missing baud rate specification in serial URI");
        }
        openSerial(uri.getHost(), uri.getPort(), SerialConfiguration::fromURI(uri));
    }
    else if (scheme == "tcp") { // TCP tcp://hostname:port
        if (uri.getPort() == 0) {
            throw std::invalid_argument("missing port specification in tcp URI");
        }
        openTCP(uri.getHost(), uri.getPort());
    }
    else if (scheme == "tcpserver") {
        if (uri.getPort() == 0) {
            throw std::invalid_argument("missing port specification in tcp server URI");
        }
        openTCPServer(uri.getPort());
    }
    else if (scheme == "udp") { // UDP udp://hostname:remoteport
        openURI_UDP(uri);
    }
    else if (scheme == "udpserver") { // UDP udpserver://localport
        openUDPServer(stoi(uri.getHost()));
    }
    else if (scheme == "file") { // file file://path
        return openFile(uri.getHost());
    }
    else if (scheme == "test") { // test://
        if (!dynamic_cast<TestStream*>(getMainStream()))
            openTestMode();
    }
}

void Driver::openURI_UDP(URI const& uri) {
    if (uri.getPort() == 0) {
        throw std::invalid_argument("missing port specification in udp URI");
    }

    string local_port = uri.getOption("local_port");
    string ignore_connrefused = uri.getOption("ignore_connrefused");
    string ignore_hostunreach = uri.getOption("ignore_hostunreach");
    string ignore_netunreach = uri.getOption("ignore_netunreach");
    string connected = uri.getOption("connected");

    if (local_port.empty() && ignore_connrefused.empty()) {
        LOG_WARN_S << "udp://host:port streams historically would report connection "
                        "refused errors. This default behavior will change in the "
                        "future." << endl;
        LOG_WARN_S << "Set the ignore_connrefused option to 1 to update to the new "
                        "behavior and remove this warning, or set it to 0 to ensure "
                        "that the behavior will be retained when the default changes"
                    << endl;
    }
    if (!local_port.empty() && connected.empty()) {
        LOG_WARN_S << "udp://host:remote_port?local_port=PORT historically was not "
                        "connecting the socket, which means that any remote host could "
                        "send messages to the local socket." << endl;
        LOG_WARN_S << "This default behavior will change in the future. Set the "
                        "connected option to 1 to update to the new behavior, that is "
                        "allowing only the specified remote host to send packets."
                    << endl;
        LOG_WARN_S << "Set to 0 to keep the current behavior even after the "
                        "default is changed " << endl;
    }

    if (connected.empty()) {
        connected = local_port.empty() ? "1" : "0";
    }

    bool is_connected = (connected == "1");

    if (ignore_connrefused.empty()) {
        ignore_connrefused = is_connected ? (local_port.empty() ? "0" : "1") : "1";
    }
    if (ignore_hostunreach.empty()) {
        ignore_hostunreach = "0";
    }
    if (ignore_netunreach.empty()) {
        ignore_netunreach = "0";
    }
    if (local_port.empty()) {
        local_port = "0";
    }
    if (ignore_connrefused == "0" && !is_connected) {
        throw std::invalid_argument(
            "cannot set ignore_connrefused=0 on an unconnected UDP stream"
        );
    }

    openUDPBidirectional(uri.getHost(), uri.getPort(), stoi(local_port),
                         ignore_connrefused == "1",
                         connected == "1",
                         ignore_hostunreach == "1",
                         ignore_netunreach == "1");
}

void Driver::openTestMode()
{
    setMainStream(new TestStream);
}



bool Driver::openSerial(std::string const& device, int baudrate,
                        SerialConfiguration const& configuration) {

    setFileDescriptor(Driver::openSerialIO(device, baudrate), true, false);
    setSerialConfiguration(configuration);
    return true;
}

bool Driver::openInet(const char *hostname, int port)
{
    openTCP(hostname, port);
    return true;
}

struct AddrinfoGuard {
    addrinfo* ptr;
    AddrinfoGuard(addrinfo* ptr)
        : ptr(ptr) {}
    ~AddrinfoGuard() {
        freeaddrinfo(ptr);
    }
};

static int createIPServerSocket(const char* port, addrinfo const& hints)
{
    addrinfo *candidates;
    int ret = getaddrinfo(NULL, port, &hints, &candidates);
    if (ret != 0)
        throw UnixError("cannot resolve server port " + string(port));

    AddrinfoGuard guard(candidates);

    for (addrinfo* rp = candidates; rp != NULL; rp = rp->ai_next) {
        int sfd = socket(rp->ai_family, rp->ai_socktype, rp->ai_protocol);
        if (sfd == -1)
            continue;

        int option = 1;
        setsockopt(sfd, SOL_SOCKET, SO_REUSEADDR, &option, sizeof(option));    

        if (::bind(sfd, rp->ai_addr, rp->ai_addrlen) == 0) {
            return sfd;
        }

        ::close(sfd);
    }

    throw UnixError("cannot open server socket on port " + string(port));
}

static int createIPClientSocket(
    const char *hostname, const char *port, addrinfo const& hints,
    sockaddr_storage* sockaddr, size_t* sockaddr_len
) {
    addrinfo *candidates;
    int ret = getaddrinfo(hostname, port, &hints, &candidates);
    if (ret != 0) {
        throw UnixError("cannot resolve client port " + string(port));
    }

    AddrinfoGuard guard(candidates);

    for (addrinfo* rp = candidates; rp != NULL; rp = rp->ai_next) {
        int sfd = socket(rp->ai_family, rp->ai_socktype, rp->ai_protocol);
        if (sfd == -1)
            continue;

        if (connect(sfd, rp->ai_addr, rp->ai_addrlen) != 0) {
            ::close(sfd);
            continue;
        }

        if (sockaddr) {
            memcpy(sockaddr, rp->ai_addr, rp->ai_addrlen);
            *sockaddr_len = rp->ai_addrlen;
        }
        return sfd;
    }

    throw UnixError("cannot open client socket on port " + string(port));
}

static pair<sockaddr_storage, size_t> connectIPSocket(
    int fd, const char* hostname, const char* port, addrinfo const& hints
) {
    addrinfo *candidates;
    int ret = getaddrinfo(hostname, port, &hints, &candidates);
    if (ret != 0) {
        throw UnixError("cannot resolve client port " + string(port));
    }

    AddrinfoGuard guard(candidates);

    for (addrinfo* rp = candidates; rp != NULL; rp = rp->ai_next) {
        if (connect(fd, rp->ai_addr, rp->ai_addrlen) != 0) {
            continue;
        }

        pair<sockaddr_storage, size_t> result;
        memcpy(&result.first, rp->ai_addr, rp->ai_addrlen);
        result.second = rp->ai_addrlen;
        return result;
    }

    throw UnixError("cannot connect client socket on port " + string(port));
}

void Driver::openIPClient(std::string const& hostname, int port, addrinfo const& hints)
{
    int sfd = createIPClientSocket(
        hostname.c_str(), lexical_cast<string>(port).c_str(), hints,
        nullptr, nullptr
    );
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

void Driver::openTCPServer(int port) {
    struct addrinfo hints;
    memset(&hints, 0, sizeof(struct addrinfo));
    hints.ai_family = AF_INET;    /* Allow IPv4 or IPv6 */
    hints.ai_socktype = SOCK_STREAM; /* Datagram socket */
    hints.ai_flags = AI_PASSIVE;    /* For wildcard IP address */

    int sfd = createIPServerSocket(lexical_cast<string>(port).c_str(), hints);
    setMainStream(new TCPServerStream(sfd));

    listen(sfd,5);
    fcntl(sfd,F_SETFL,O_NONBLOCK);    
}

void Driver::openUDP(std::string const& hostname, int port)
{
    if (hostname.empty())
    {
        LOG_WARN_S << "openUDP: providing an empty hostname is "
                      "deprecated, use openUDPServer instead" << endl;
        return openUDPServer(port);
    }

    struct addrinfo hints;
    memset(&hints, 0, sizeof(struct addrinfo));
    hints.ai_family = AF_UNSPEC;    /* Allow IPv4 or IPv6 */
    hints.ai_socktype = SOCK_DGRAM; /* Datagram socket */
    openIPClient(hostname, port, hints);
}

void Driver::openUDPServer(int port)
{
    struct addrinfo hints;
    memset(&hints, 0, sizeof(struct addrinfo));
    hints.ai_family = AF_UNSPEC;    /* Allow IPv4 or IPv6 */
    hints.ai_socktype = SOCK_DGRAM; /* Datagram socket */
    hints.ai_flags = AI_PASSIVE;    /* For wildcard IP address */

    int sfd = createIPServerSocket(lexical_cast<string>(port).c_str(), hints);
    setMainStream(new UDPServerStream(sfd, true));
}

void Driver::openUDPBidirectional(
    std::string const& hostname, int remote_port, int local_port,
    bool ignore_connrefused,
    bool connected,
    bool ignore_hostunreach,
    bool ignore_netunreach
) {
    struct addrinfo local_hints;
    memset(&local_hints, 0, sizeof(struct addrinfo));
    local_hints.ai_family = AF_UNSPEC;    /* Allow IPv4 or IPv6 */
    local_hints.ai_socktype = SOCK_DGRAM; /* Datagram socket */
    local_hints.ai_flags = AI_PASSIVE;    /* For wildcard IP address */

    int sfd = createIPServerSocket(lexical_cast<string>(local_port).c_str(), local_hints);

    struct addrinfo remote_hints;
    memset(&remote_hints, 0, sizeof(struct addrinfo));
    remote_hints.ai_family = AF_UNSPEC;    /* Allow IPv4 or IPv6 */
    remote_hints.ai_socktype = SOCK_DGRAM; /* Datagram socket */

    pair<sockaddr_storage, size_t> peer;
    if (connected) {
        peer = connectIPSocket(
            sfd, hostname.c_str(), lexical_cast<string>(remote_port).c_str(),
            remote_hints
        );
    }
    else {
        int fd = createIPClientSocket(
            hostname.c_str(), lexical_cast<string>(remote_port).c_str(),
            remote_hints, &peer.first, &peer.second
        );
        ::close(fd);
    }
    auto stream = new UDPServerStream(
        sfd, true,
        reinterpret_cast<sockaddr*>(&peer.first), &peer.second
    );
    stream->setIgnoreEconnRefused(ignore_connrefused);
    stream->setIgnoreEhostUnreach(ignore_hostunreach);
    stream->setIgnoreEnetUnreach(ignore_netunreach);
    setMainStream(stream);
}

SerialConfiguration Driver::parseSerialConfiguration(std::string const &description) {
    boost::regex ex = boost::regex(
        "^([5-8])([neo])([12])$",
        boost::regex_constants::icase
    );

    boost::smatch sm;
    if (!boost::regex_match (description, sm, ex)) {
        throw invalid_argument("Invalid serial configuration");
    }

    SerialConfiguration serial_config;
    serial_config.byte_size = static_cast<ByteSize>(atoi(sm[1].str().c_str()));
    serial_config.parity = static_cast<ParityChecking>(toupper(sm[2].str().at(0)));
    serial_config.stop_bits = static_cast<StopBits>(atoi(sm[3].str().c_str()));

    return serial_config;
}


void Driver::setSerialConfiguration(SerialConfiguration const& serial_config)
{
    struct termios tio;
    int fd = getFileDescriptor();

    if (tcgetattr(fd, &tio)) {
        throw UnixError("Driver::setSerialConfiguration: Failed to get terminal info\n");
    }

    if (serial_config.parity == PARITY_NONE) {
        tio.c_cflag &= ~PARENB;
    } else {
        tio.c_cflag |= PARENB;
        if (serial_config.parity == PARITY_EVEN) {
            tio.c_cflag &= ~PARODD;
        } else {
            tio.c_cflag |= PARODD;
        }
    }

    tio.c_cflag &= ~CSIZE;
    switch (serial_config.byte_size) {
        case BITS_5:
            tio.c_cflag |= CS5;
            break;
        case BITS_6:
            tio.c_cflag |= CS6;
            break;
        case BITS_7:
            tio.c_cflag |= CS7;
            break;
        case BITS_8:
            tio.c_cflag |= CS8;
            break;
    }

    if (serial_config.stop_bits == STOP_BITS_ONE) {
        tio.c_cflag &= ~CSTOPB;
    } else {
        tio.c_cflag |= CSTOPB;
    }

    if (tcsetattr(fd, TCSANOW, &tio) != 0) {
        throw UnixError("Driver::setSerialConfiguration: Failed to set terminal info\n");
    }
}

int Driver::openSerialIO(std::string const& port, int baud_rate)
{
    int fd = ::open(port.c_str(), O_RDWR | O_NOCTTY | O_SYNC | O_NONBLOCK );
    if (fd == FDStream::INVALID_FD)
        throw UnixError("cannot open device " + port);

    FileGuard guard(fd);

    struct termios tio;
    memset(&tio, 0, sizeof(termios));
    tio.c_cflag = CS8 | CREAD;    // data bits = 8bit and enable receiver
    tio.c_iflag = IGNBRK; // don't use breaks by default

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
                + lexical_cast<string>(extract_result)
                + ", which is larger than the buffer size "
                + lexical_cast<string>(buffer_size) + ".");

    if (0 == extract_result)
        return make_pair(buffer, 0);

    if (extract_result < 0)
        packet_start += -extract_result;
    else if (extract_result > 0)
        packet_size = extract_result;

    if (m_extract_last)
    {
        m_stats.stamp = Time::now();
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
        m_stats.stamp = Time::now();
        m_stats.bad_rx  += packet.first - internal_buffer;
        m_stats.good_rx += packet.second;
    }

    pullBytesFromInternal(buffer, packet.first - internal_buffer, packet.second);
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

void Driver::pullBytesFromInternal(uint8_t* buffer, int skip, int size) {
    int total_size = skip + size;
    int new_internal_size = internal_buffer_size - total_size;

    memcpy(buffer, internal_buffer + skip, size);
    memmove(internal_buffer,
            internal_buffer + total_size,
            new_internal_size);
    internal_buffer_size = new_internal_size;
}

int Driver::readRaw(uint8_t* buffer, int out_buffer_size)
{
    return readRaw(buffer, out_buffer_size, getReadTimeout());
}

int Driver::readRaw(uint8_t* buffer, int out_buffer_size, Time const& timeout)
{
    return readRaw(buffer, out_buffer_size, timeout, timeout);
}

int Driver::readRaw(uint8_t* buffer, int out_buffer_size,
                    Time const& packet_timeout,
                    Time const& first_byte_timeout_,
                    Time const& inter_byte_timeout_) {
    if (!isValid()) {
        throw std::runtime_error("attempting to call readRaw on a closed driver");
    }

    int buffer_fill = std::min<int>(internal_buffer_size, out_buffer_size);
    pullBytesFromInternal(buffer, 0, buffer_fill);

    auto first_byte_timeout = min(packet_timeout, first_byte_timeout_);
    auto inter_byte_timeout = inter_byte_timeout_.isNull() ?
                              packet_timeout : inter_byte_timeout_;

    auto now = Time::now();
    auto last_char = now + packet_timeout;
    bool received_bytes = false;
    Time global_deadline = now + first_byte_timeout;
    while (buffer_fill < out_buffer_size && now <= global_deadline)
    {
        auto deadline = min(global_deadline, last_char + inter_byte_timeout);
        try {
            m_stream->waitRead(deadline - now);
        }
        catch(TimeoutError&) {
            break;
        }
        int c = m_stream->read(buffer + buffer_fill,
                               out_buffer_size - buffer_fill);
        now = Time::now();

        if (c > 0) {
            last_char = now;
            if (!received_bytes) {
                global_deadline = now + packet_timeout;
                received_bytes = true;
            }
            for (IOListener* it: m_listeners)
                it->readData(buffer + buffer_fill, c);
        }
        buffer_fill += c;
    }

    return buffer_fill;
}

pair<int, bool> Driver::readPacketInternal(uint8_t* buffer, int out_buffer_size)
{
    if (out_buffer_size < MAX_PACKET_SIZE)
        throw length_error("readPacket(): provided buffer too small (got " + lexical_cast<string>(out_buffer_size) + ", expected at least " + lexical_cast<string>(MAX_PACKET_SIZE) + ")");

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

void Driver::setReadTimeout(Time const& timeout)
{ m_read_timeout = timeout; }
Time Driver::getReadTimeout() const
{ return m_read_timeout; }
int Driver::readPacket(uint8_t* buffer, int buffer_size)
{
    return readPacket(buffer, buffer_size,
                      getReadTimeout(), getReadTimeout());
}
int Driver::readPacket(uint8_t* buffer, int buffer_size, Time const& packet_timeout)
{
    return readPacket(buffer, buffer_size,
                      packet_timeout, packet_timeout);
}
int Driver::readPacket(uint8_t* buffer, int buffer_size,
                       int packet_timeout, int first_byte_timeout)
{
    if (first_byte_timeout == -1) {
        first_byte_timeout = packet_timeout;
    }

    return readPacket(buffer, buffer_size,
                      Time::fromMilliseconds(packet_timeout),
                      Time::fromMilliseconds(first_byte_timeout));
}
int Driver::readPacket(uint8_t* buffer, int buffer_size,
                       Time const& packet_timeout, Time const& first_byte_timeout_)
{
    if (buffer_size < MAX_PACKET_SIZE) {
        throw length_error("readPacket(): provided buffer too small (got "
                + lexical_cast<string>(buffer_size) + ", expected at least "
                + lexical_cast<string>(MAX_PACKET_SIZE) + ")");
    }

    if (!isValid()) {
        // No valid file descriptor. Assume that the user is using the raw data
        // interface (i.e. that the data is already in the internal read buffer)
        pair<int, bool> result = extractPacketFromInternalBuffer(buffer, buffer_size);
        if (result.first) {
            return result.first;
        }
        else {
            throw TimeoutError(
                TimeoutError::PACKET, "readPacket(): no packet in the internal "\
                                      "buffer and no stream to read from");
        }
    }

    TimeoutError::TIMEOUT_TYPE timeout_type = TimeoutError::FIRST_BYTE;
    Time first_byte_timeout = min(packet_timeout, first_byte_timeout_);
    Time start_time = Time::now();
    Time deadline = start_time + first_byte_timeout;

    while (true) {
        pair<int, bool> read_state = readPacketInternal(buffer, buffer_size);
        int packet_size = read_state.first;

        if (packet_size > 0) {
            return packet_size;
        }

        // if there was no data to read _and_ packet_timeout is zero, we'll throw
        if (packet_timeout.isNull()) {
            throw TimeoutError(
                TimeoutError::FIRST_BYTE,
                "readPacket(): no data to read while a packet_timeout of 0 was given");
        }

        bool read_something = read_state.second;
        if (timeout_type == TimeoutError::FIRST_BYTE && read_something) {
            deadline = start_time + packet_timeout;
            timeout_type = TimeoutError::PACKET;
        }

        Time now = Time::now();
        if (now > deadline)
        {
            throw TimeoutError(
                timeout_type,
                "readPacket(): no data after waiting "
                + lexical_cast<string>((now - start_time).toMilliseconds()) + "ms");
        }

        // we still have time left to wait for arriving data. see how much
        Time remaining = deadline - now;
        try {
            // calls select and waits until a new read can be actually performed (in the next
            // while-iteration)
            m_stream->waitRead(remaining);
        }
        catch (TimeoutError& e)
        {
            auto total_wait = Time::now() - start_time;
            throw TimeoutError(timeout_type,
                "readPacket(): no data waiting for data. Last wait lasted "
                + lexical_cast<string>(remaining.toMilliseconds()) + "ms, "
                + "out of a total wait of "
                + lexical_cast<string>(total_wait.toMilliseconds()) + "ms");
        }
    }
}

void Driver::setWriteTimeout(Time const& timeout)
{ m_write_timeout = timeout; }
Time Driver::getWriteTimeout() const
{ return m_write_timeout; }

bool Driver::writePacket(uint8_t const* buffer, int buffer_size)
{
    return writePacket(buffer, buffer_size, getWriteTimeout());
}
bool Driver::writePacket(uint8_t const* buffer, int buffer_size, Time const& timeout)
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
            m_stats.stamp = Time::now();
            m_stats.tx += buffer_size;
            return true;
        }

        if (time_out.elapsed())
            throw TimeoutError(TimeoutError::PACKET, "writePacket(): timeout");

        int remaining_timeout = time_out.timeLeft();
        m_stream->waitWrite(Time::fromMicroseconds(remaining_timeout * 1000));
    }
}

bool Driver::eof() const
{
    if (!m_stream)
        throw std::runtime_error("eof(): invalid stream");
    return m_stream->eof();
}