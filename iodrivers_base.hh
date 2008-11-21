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

    /** Internal helper method for readPacket. This one is purely
     * non-blocking. It returns -1 on error, 0 if no data is available and
     * >0 if a packet has been read
     */
    int readPacketInternal(uint8_t* buffer, int bufsize);

public:
    IODriver(int max_packet_size);
    ~IODriver();

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

