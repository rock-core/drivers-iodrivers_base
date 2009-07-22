#ifndef SERIAL_HH
#define SERIAL_HH

#include <stdexcept>

/** Exception raised when a unix error occured in readPacket or writePacket
 */
struct unix_error : std::runtime_error
{
    int const error;
    explicit unix_error(std::string const& desc);

    unix_error(std::string const& desc, int error_code);
};

/** Exception raised when a timeout occured in readPacket or writePacket */
struct timeout_error : std::runtime_error
{
    enum TIMEOUT_TYPE
    { PACKET, FIRST_BYTE };

    TIMEOUT_TYPE const type;

    explicit timeout_error(TIMEOUT_TYPE type, std::string const& desc)
        : std::runtime_error(desc)
        , type(type) {}
};

class file_guard
{
    int fd;
public:
    explicit file_guard(int fd)
        : fd(fd) { }
    ~file_guard() { if (fd != -1) close(fd); };
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
 *   <li> give to the IODriver constructor the maximum packet size that it can expect
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
class IODriver
{
public:
    static const int INVALID_FD      = -1;

private:
    /** Internal buffer used for reading packets */
    uint8_t* internal_buffer;
    /** The current count of bytes left in \c internal_buffer */
    size_t internal_buffer_size;

protected:
    int const MAX_PACKET_SIZE;

    /** The file descriptor we are acting on. It is automatically closed on
     * destruction. -1 means not initialized
     */
    int m_fd;

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
    std::pair<uint8_t const*, int> findPacket(uint8_t const* buffer, int buffer_size);

    /** Internal helper method which copies in buffer the appropriate packet
     * found in the internal buffer, and returns its size. It returns 0 if no
     * packet has been found.
     */
    int doPacketExtraction(uint8_t* buffer);

public:
    /** Creates an IODriver class for a packet-based protocol
     *
     * @arg max_packet_size the maximum packet size in bytes
     * @arg extract_last if true, readPacket will return only the latest packet
     *   found in the buffer, discarding oldest packets. This flag can be
     *   changed with setExtractLastPacket
     */
    IODriver(int max_packet_size, bool extract_last = false);

    ~IODriver();

    /** Removes all data that is pending on the file descriptor */
    void clear();

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

    /** Opens a serial port and sets it up to a sane configuration.  Use
     * then setSerialBaudrate() to change the actual baudrate of the
     * connection on this side.
     */
    bool openSerial(std::string const& port, int baudrate);

    /** Initializes the file descriptor with the given value. If auto_close
     * is true (the default), then the file descriptor will be
     * automatically closed on exit.
     *
     * The provided file descriptor must be non-blocking for the timeout
     * functionality to work.
     */
    void setFileDescriptor(int fd, bool auto_close = true);

    /** Returns the file descriptor associated with this object. If no file
     * descriptor is assigned, returns INVALID_FD
     */
    int getFileDescriptor() const;

    /** True if a valid file descriptor is assigned to this object */
    bool isValid() const;

    enum SERIAL_RATES
    {
        B19200 = 19200,
        B38400 = 38400,
        B57600 = 57600,
        B115200 = 115200
    };

    /** Sets the baud rate value for the serial connection
     *
     * @arg the baud rate. It can be one of the values in SERIAL_RATES
     * @return true on success, false on failure
     */
    bool setSerialBaudrate(int rate);

    /** Closes the file descriptor */
    void close();

    /** Tries to read a packet from the file descriptor and to save it in the
     * provided buffer. +packet_timeout+ is the timeout in milliseconds to
     * receive a complete packet. There is not infinite timeout value, and 0
     * is non-blocking at all
     *
     * first_byte_timeout, if set to a value greater or equal to 0, defines
     * the timeout in milliseconds to receive at least one byte.
     *
     * @throws timeout_error on timeout and unix_error on reading problems
     * @returns the size of the packet
     */
    int readPacket(uint8_t* buffer, int bufsize, int packet_timeout, int first_byte_timeout = -1);

    /** Tries to write a packet to the file descriptor. +timeout+ is the
     * timeout in milliseconds. There is not infinite timeout value, and 0
     * is non-blocking at all
     *
     * @throws timeout_error on timeout and unix_error on reading problems
     * @returns true on success, false on failure
     */
    bool writePacket(uint8_t const* buffer, int bufsize, int timeout);

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

    static std::string printable_com(std::string const& buffer);
    static std::string printable_com(uint8_t const* buffer, size_t buffer_size);
    static std::string printable_com(char const* buffer, size_t buffer_size);
};

#endif

