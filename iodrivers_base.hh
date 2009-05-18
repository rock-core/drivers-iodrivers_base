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
    explicit timeout_error(std::string const& desc)
        : std::runtime_error(desc) {}
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
     * non-blocking. It returns -1 on error, 0 if no data is available and
     * >0 if a packet has been read
     */
    int readPacketInternal(uint8_t* buffer, int bufsize);

    /** Internal helper which extracts the packet to be returned by
     * readPacketInternal (and therefore readPacket) in the provided
     * buffer. This method takes into account the negative values that
     * can be returned by extractPacket() and the m_extract_last flag.
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
     * @arg max_packet_size the maximum packet size in butes
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

    /** Tries to read a packet from the file descriptor and to save it in
     * the provided buffer. +timeout+ is the timeout in milliseconds. There
     * is not infinite timeout value, and 0 is non-blocking at all
     *
     * @throws timeout_error on timeout and unix_error on reading problems
     * @returns the size of the packet
     */
    int readPacket(uint8_t* buffer, int bufsize, int timeout);

    /** Tries to write a packet to the file descriptor. +timeout+ is the
     * timeout in milliseconds. There is not infinite timeout value, and 0
     * is non-blocking at all
     *
     * @throws timeout_error on timeout and unix_error on reading problems
     * @returns true on success, false on failure
     */
    bool writePacket(uint8_t const* buffer, int bufsize, int timeout);

    /** Reimplement that in subclasses to determine if there is currently a
     * full packet in the provided buffer. If a packet is found, the
     * returned value is the offset of the first byte not in the packet. 0 means
     * that no full packet is available. The provided buffer is never empty.
     *
     * Returning a negative value indicates that the respective number of bytes
     * should be discarded as junk, invalid packets or unwanted markers.
     */
    virtual int extractPacket(uint8_t const* buffer, size_t buffer_size) const = 0;

    static std::string printable_com(std::string const& buffer);
    static std::string printable_com(uint8_t const* buffer, size_t buffer_size);
    static std::string printable_com(char const* buffer, size_t buffer_size);
};

#endif

