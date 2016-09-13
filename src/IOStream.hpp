#ifndef IODRIVERS_BASE_IOSTREAM_HH
#define IODRIVERS_BASE_IOSTREAM_HH

#include <base/Time.hpp>

#include <unistd.h>
#include <netinet/in.h>
#include <stdio.h>
#include <sys/types.h>
#include <sys/socket.h>

namespace iodrivers_base
{
    /** Generic IO handler that allows to wait, read and write to an IO stream
     *
     * We're not using the default std::iostream interface as it is quite
     * complicated, and the Driver class really needs little
     */
    class IOStream
    {
    public:
        virtual ~IOStream();
        virtual void waitRead(base::Time const& timeout) = 0;
        virtual void waitWrite(base::Time const& timeout) = 0;
        virtual size_t read(uint8_t* buffer, size_t buffer_size) = 0;
        virtual size_t write(uint8_t const* buffer, size_t buffer_size) = 0;
        virtual void clear() = 0;

        /** If this IOStream is attached to a file descriptor, return it. Otherwise,
         * returns INVALID_FD;
         *
         * The default implementation returns INVALID_FD
         */
        virtual int getFileDescriptor() const;
    };

    /** Implementation of IOStream for file descriptors */
    class FDStream : public IOStream
    {
        bool m_auto_close;
	
    protected:  
	int m_fd;

    public:
        static const int INVALID_FD      = -1;

        FDStream(int fd, bool auto_close);
        virtual ~FDStream();
        virtual void waitRead(base::Time const& timeout);
        virtual void waitWrite(base::Time const& timeout);
        virtual size_t read(uint8_t* buffer, size_t buffer_size);
        virtual size_t write(uint8_t const* buffer, size_t buffer_size);
        virtual void clear();

        /** Sets the NONBLOCK flag on the given file descriptor and returns true if
         * the file descriptor was in blocking mode
         */
        bool setNonBlockingFlag(int fd);

        virtual int getFileDescriptor() const;
    };
    
    class UDPServerStream : public FDStream
    {
    public: 
      UDPServerStream(int fd, bool auto_close);
      UDPServerStream(int fd, bool auto_close, struct sockaddr *si_other, size_t *s_len);
      virtual size_t read(uint8_t* buffer, size_t buffer_size);
      virtual size_t write(uint8_t const* buffer, size_t buffer_size);    
    protected:
      struct sockaddr m_si_other;
      bool m_si_other_dynamic;
      unsigned int m_s_len;
    };
}

#endif
