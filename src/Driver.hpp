#ifndef IODRIVERS_BASE_DRIVER_HH
#define IODRIVERS_BASE_DRIVER_HH

#include <stdexcept>
#include <stdint.h>
#include <stdio.h>
#include <unistd.h>
#include <set>
#include <vector>
#include <iodrivers_base/Exceptions.hpp>
#include <iodrivers_base/SerialConfiguration.hpp>
#include <iodrivers_base/Status.hpp>
#include <iodrivers_base/URI.hpp>

struct addrinfo;

namespace iodrivers_base {

class IOStream;
class IOListener;

class FileGuard
{
    int fd;
public:
    explicit FileGuard(int fd = -1)
        : fd(fd) { }
    ~FileGuard() { reset(); };

    void reset(int new_fd = -1)
    {
        if (fd != -1) close(fd);
        fd = new_fd;
    }

    int get() const { return fd; }
    int release()
    {
        int ret = fd;
        fd = -1;
        return ret;
    }
};

/** A generic implementation of a packet extraction algorithm on an I/O device.
 *
 * This class provides the basic service or reading an I/O device until a full
 * packet has been read, and returning that packet. It does so while maintaining
 * a proper read and write timeout.
 *
 * To use this class:
 * <ul>
 *   <li> subclass it
 *   <li> give to the Driver constructor the maximum packet size that it can expect
 *   <li> implement extractPacket (see below)
 * </ul>
 *
 * Then, you can freely use writePacket and readPacket to write/read data from
 * the device.
 *
 * The issue that this class is trying to solve in a generic way is that, when
 * reading on I/O, one will seldom read a full packet at once. What this class
 * does is to accumulate data in readPacket, until the subclass-provided
 * extractPacket implementation finds a packet in the buffer. When a packet is
 * found, it is copied into the buffer given to readPacket and the packet size
 * is returned.
 *
 * See extractPacket for more information on how to implement this method.
 */
class Driver
{
public:
    /** For backward compatibility only */
    typedef iodrivers_base::Status Statistics;

    static const int INVALID_FD = -1;

private:
    /** Internal buffer used for reading packets */
    uint8_t* internal_buffer;
    /** The current count of bytes left in \c internal_buffer */
    size_t internal_buffer_size;

public:
    int const MAX_PACKET_SIZE;

protected:
    /** The underlying object that gives us access to the actual I/O stream
     */
    IOStream* m_stream;

    /** Set of listener that are passed the data that goes through this driver
     */
    std::set<IOListener*> m_listeners;

    /** True if \c fd should be closed on exit
     *
     * @see setFileDescriptor
     */
    bool m_auto_close;

    /** True if readPacket should return the last packet found
     * in the buffer
     *
     * @see getExtractLastPacket
     */
    bool m_extract_last;

    /** Default read timeout for readPacket
     *
     * @see getReadTimeout setReadTimeout readPacket
     */
    base::Time m_read_timeout;

    /** Default write timeout for writePacket
     *
     * @see getWriteTimeout setWriteTimeout writePacket
     */
    base::Time m_write_timeout;

    /** Internal helper method for readPacket. This one is purely
     * non-blocking.
     *
     * The first element of the pair is -1 on error, 0 if no data is available
     * and >0 if a packet has been read
     *
     * The second element of the pair is true if data has actually been read
     * on the file descriptor, and false otherwise.
     */
    std::pair<int, bool> readPacketInternal(uint8_t* buffer, int bufsize);

    /** Internal helper which extracts the packet to be returned by
     * readPacketInternal (and therefore readPacket) in the provided
     * buffer. This method takes into account the negative values that
     * can be returned by extractPacket() and the m_extract_last flag.
     *
     * The first element of the returned pair is the start of either a full
     * packet, if one has been found, or of the start of a packet if a partial
     * packet is in buffer. This pointer is buffer + buffer_size (i.e.
     * end-of-buffer) if no packet is present at all.
     *
     * The second element of the returned pair is the packet size if a full
     * packet has been found, and 0 in all other cases.
     */
    std::pair<uint8_t const*, int> findPacket(uint8_t const* buffer, int buffer_size) const;

    /** Internal helper method which reads packets only from the internal buffer
     * (does not access any file descriptor)
     */
    std::pair<int, bool> extractPacketFromInternalBuffer(uint8_t* buffer, int out_buffer_size);

    /** Internal helper method which copies in buffer the appropriate packet
     * found in the internal buffer, and returns its size. It returns 0 if no
     * packet has been found.
     */
    int doPacketExtraction(uint8_t* buffer);

    mutable Status m_stats;

    void openIPClient(std::string const& hostname, int port, addrinfo const& hints);

    /** Pull bytes out of the internal buffer into the given buffer
     *
     * @param skip offset in the internal buffer where the copy starts. These
     *             bytes are removed from the internal buffer.
     * @param size number of bytes copied to the output buffer
    */
    void pullBytesFromInternal(uint8_t* buffer, int skip, int size);

    /** Helper for openURI to handle UDP streams
     *
     * They're rather complex to open because of backward compatibility reasons
     */
    void openURI_UDP(URI const& uri);

public:
    /** Creates an Driver class for a packet-based protocol
     *
     * @arg max_packet_size the maximum packet size in bytes
     * @arg extract_last if true, readPacket will return only the latest packet
     *   found in the buffer, discarding oldest packets. This flag can be
     *   changed with setExtractLastPacket
     */
    Driver(int max_packet_size, bool extract_last = false);

    virtual ~Driver();

    /** Sets the default read timeout in milliseconds. Used in readPacket calls
     * without timeout parameters
     */
    void setReadTimeout(base::Time const& t);

    /** Get the default read timeout */
    base::Time getReadTimeout() const;

    /** Sets the default write timeout in milliseconds. Used in writePacket calls
     * without timeout parameters
     */
    void setWriteTimeout(base::Time const& t);

    /** Get the default read timeout */
    base::Time getWriteTimeout() const;

    /** Removes all data that is pending on the file descriptor */
    void clear();

    /** Returns the I/O statistics
     *
     * Use resetStats() to set them back to 0
     */
    Status getStatus() const;

    /** Reset the I/O statistics to 0
     */
    void resetStatus();

    /** @deprecated
     *
     * Use getStatus() instead
     */
    Status getStats() const { return getStatus(); }

    /** @deprecated
     */
    void resetStats() { return resetStatus(); }

    /** Changes the packet extraction mode
     *
     * @see getExtractLastPacket
     */
    void setExtractLastPacket(bool flag);

    /** Returns the current packet extraction mode. If true, readPacket will
     * only return the last packet found in the buffer. Otherwise, always
     * returns the first packet found
     */
    bool getExtractLastPacket() const;

    /** Opens an URI to a device
     *
     * The following formats are recognized:
     *
     * * serial://path/to/device:baudrate
     * * tcp://hostname:port
     * * udp://hostname:remote_port[:local_port]
     * * udpserver://port
     */
    virtual void openURI(std::string const& uri);

    /**
    * @deprecated
    *
    * Use openTCP
    */
    bool openInet(const char *hostname, int port);

    /**
    * Opens a TCP connection to foreign host,
    */
    void openTCP(std::string const& hostname, int port);

    /**
    * Opens a TCP connection for one client,
    */
    void openTCPServer(int port);

    /**
    * Opens a UDP connection
    *
    * @param hostname the remote host to send datagrams to
    * @param remote_port the port on the remote host
    *
    * A deprecated behavior is to give an empty hostname, which opens a UDP
    * socket listening to the given port. The server will write to the host
    * whose UDP packet was received last (or skips sending altogether until
    * such a datagram was received). Use openUDPServer instead.
    */
    void openUDP(std::string const& hostname, int remote_port);

    /**
    * Opens a listening UDP socket
    *
    * @param port the port to bind to, set to zero to use a random port
    */
    void openUDPServer(int port);

    /**
    * Opens a UDP connection
    *
    * All parameters are required. The driver will be available to
    * write data to a specified host and output port. Data will be read
    * from the input port.
    */
    void openUDPBidirectional(
        std::string const& hostname, int remote_port, int local_port,
        bool ignore_connrefused,
        bool connected,
        bool ignore_hostunreach = true,
        bool ignore_netunreach = true
    );

    /** Opens a serial port and sets it up to a sane configuration.  Use
     * then setSerialBaudrate() to change the actual baudrate of the
     * connection on this side.
     *
     * Throws UnixError on error
     *
     * The return value is kept here for backward compatibility only.
     */
    bool openSerial(
        std::string const& port, int baudrate,
        SerialConfiguration const& serial_config = SerialConfiguration()
    );

    /** @overload
     */
    bool openSerial(URI const& uri);

    /** Opens a file from a path. It can be used for read-only tests of a
     * driver, or to connect to a named FIFO or an already-created Unix socket
     */
    void openFile(std::string const& path);

    /** Opens a serial port and sets it up to a sane configuration
     *
     * Returns INVALID_FD on failure, or the file descriptor on success
     */
    static int openSerialIO(std::string const& port, int baudrate);

    /** Sets the O_NONBLOCK flag on a file descriptor
     *
     * Returns true if the flag was not already set and false otherwise
     *
     * Throws UnixError if the flag could not be set
     */
    static bool setNonBlockingFlag(int fd);

    /** Initializes the file descriptor with the given value. If auto_close
     * is true (the default), then the file descriptor will be
     * automatically closed on exit.
     *
     * The provided file descriptor must be non-blocking for the timeout
     * functionality to work.
     */
    void setFileDescriptor(int fd, bool auto_close = true, bool has_eof = true);

    /** Returns the file descriptor associated with this object. If no file
     * descriptor is assigned, returns INVALID_FD
     */
    int getFileDescriptor() const;

    /** True if a valid file descriptor is assigned to this object */
    bool isValid() const;

    // In most Linux systems also baudrates up to 1152000 are defined.
    enum SERIAL_RATES
    {
        SERIAL_1200 = 1200,
        SERIAL_2400 = 2400,
        SERIAL_4800 = 4800,
        SERIAL_9600 = 9600,
        SERIAL_19200 = 19200,
        SERIAL_38400 = 38400,
        SERIAL_57600 = 57600,
        SERIAL_115200 = 115200,
        SERIAL_230400 = 230400,
        SERIAL_460800 = 460800,
        SERIAL_576000 = 576000,
        SERIAL_921600 = 921600,
        SERIAL_1152000 = 1152000
    };

    /** Sets the baud rate value for the serial connection
     *
     * @arg the baud rate. It can be one of the values in SERIAL_RATES
     * @return true on success, false on failure
     */
    bool setSerialBaudrate(int rate);

    /** Sets the baud rate value for the given file descriptor
     *
     * @arg the baud rate. It can be one of the values in SERIAL_RATES
     * @return true on success, false on failure
     */
    static bool setSerialBaudrate(int fd, int rate);

    /** Closes the file descriptor */
    virtual void close();

    /** True if a packet is already present in the internal buffer */
    bool hasPacket() const;

    /** @overload
     *
     * Calls readRaw using the default timeout as packet timeout
     */
    int readRaw(uint8_t* buffer, int bufsize);

    /** @overload
     *
     * Use the same timeout for first byte and packet
     */
    int readRaw(uint8_t* buffer, int bufsize, base::Time const& timeout);

    /** Read raw bytes from the underlying I/O
     *
     * Reads as many bytes as possible within the time boundaries specified
     * by its timeout parameters, not attempting to extract packets
     *
     * @arg packet_timeout the overall timeout. The method will return at most
     *   after that much time has elapsed
     * @arg first_byte_timeout return if no bytes are received within that
     *   much time
     * @arg inter_byte_timeout return if no new bytes have been received after
     *   that much time has elapsed since the last received byte
     *
     * This never throws
     */
    int readRaw(uint8_t* buffer, int bufsize,
                base::Time const& packet_timeout,
                base::Time const& first_byte_timeout,
                base::Time const& inter_byte_timeout = base::Time());

    /** @overload
     *
     * Calls readPacket using the default timeout as packet timeout, and no
     * first byte timeout
     */
    int readPacket(uint8_t* buffer, int bufsize);

    /** @overload
     *
     * Calls readPacket without a first byte timeout
     */
    int readPacket(uint8_t* buffer, int bufsize, base::Time const& packet_timeout);

    /** @overload @deprecated
     *
     * @arg packet_timeout in milliseconds, see readPacket for semantics
     * @arg first_byte_timeout in milliseconds, see readPacket for semantics
     */
    int readPacket(uint8_t* buffer, int bufsize, int packet_timeout, int first_byte_timeout = -1);

    /** Tries to read a packet from the file descriptor and to save it in the
     * provided buffer. +packet_timeout+ is the timeout to receive a complete
     * packet. There is no infinite timeout value, and 0 is non-blocking at all
     * (but might throw without data)
     *
     * first_byte_timeout defines the timeout to receive at least one byte. Set
     * to a value greater than packet_timeout (or call the readPacket variant
     * without fourth argument) to disable. with a packet_timeout of 0, a
     * TimeoutError exception is thrown when there is no data to read.
     *
     * Timeout values are used only if a valid file descriptor has been provided
     * to the class.  Otherwise, if the pushInputData() interface is being used,
     * it will raise TimeoutError if no packets are currently present in the
     * internal buffer.
     *
     * @throws TimeoutError on timeout or no data, and UnixError on reading problems
     * @returns the size of the packet
     */
    int readPacket(uint8_t* buffer, int bufsize,
                   base::Time const& packet_timeout,
                   base::Time const& first_byte_timeout);

    /** @overload
     *
     * Calls writePacket using the default write timeout
     */
    bool writePacket(uint8_t const* buffer, int bufsize);

    /** @overload @deprecated
     */
    bool writePacket(uint8_t const* buffer, int bufsize, int timeout);

    /** Tries to write a packet to the file descriptor. +timeout+ is the
     * timeout in milliseconds. There is not infinite timeout value, and 0
     * is non-blocking at all
     *
     * @throws timeout_error on timeout and unix_error on reading problems
     * @returns always true. The return value is kept for backward compatibility only
     */
    bool writePacket(uint8_t const* buffer, int bufsize, base::Time const& timeout);

    /** Find a packet into the currently accumulated data.
     *
     * This method should be provided by subclasses. The @a buffer argument is
     * the data that has been read until now, and @a buffer_size how many bytes
     * there is in @a buffer.
     *
     * There is four possible cases:
     * - there is no packet in the buffer. In that case, return -buffer_size to
     *   discard all the data that has been gathered until now.
     * - there is the beginning of a packet but it is not starting at the first
     *   byte of \c buffer. In that case, return -position_packet_start, where
     *   position_packet_start is the position of the packet in \c buffer.
     * - a packet begins at the first byte of \c buffer, but the end of the
     *   packet is not in \c buffer yet. Return 0.
     * - there is a full packet in \c buffer, starting at the first buffer byte.
     *   Return the packet size. That data will be copied back to the buffer
     *   given to readPacket.
     */
    virtual int extractPacket(uint8_t const* buffer, size_t buffer_size) const = 0;

    /** Sets the main IO stream
     *
     * The Driver object takes ownership of the stream
     *
     * The current I/O stream will be deleted by this operation
     */
    void setMainStream(IOStream* stream);

    /** Gets the main IO stream
     */
    IOStream* getMainStream() const;

    /** Open the IO in test mode
     *
     * It is mostly equivalent to openURI("test://"), but is not meant to be
     * overloaded, allowing to test openURI:
     *
     * <code>
     * openTestMode()
     * // Feed data to the driver
     * openURI();
     *
     * Moreover, it will always create
     * a new test "channel", while openURI will create a new one only if the
     * current main stream is not a test stream already.
     */
    void openTestMode();

    /** Add a listener stream. The object's ownership is taken by the Driver
     * object.
     */
    void addListener(IOListener* stream);

    /** Removes a listener stream. The object's ownership is passed to the
     * caller
     */
    void removeListener(IOListener* stream);

    /** Whether the current stream is finished (e.g. end-of-file or disconnected)
     */
    bool eof() const;

    /** Sets serial port configuration. Must be called after openURI(...)
     */
    void setSerialConfiguration(SerialConfiguration const& serial_config);

    SerialConfiguration parseSerialConfiguration(std::string const &description);

    static std::string printable_com(std::string const& buffer);
    static std::string printable_com(uint8_t const* buffer, size_t buffer_size);
    static std::string printable_com(char const* buffer, size_t buffer_size);

    static std::string binary_com(std::string const& str);
    static std::string binary_com(uint8_t const* str, size_t str_size);
    static std::string binary_com(char const* str, size_t str_size);
};

}

#endif

