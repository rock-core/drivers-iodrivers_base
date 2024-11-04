#include <iodrivers_base/IOStream.hpp>
#include <iodrivers_base/Exceptions.hpp>
#include <base-logging/Logging.hpp>

#include <errno.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/un.h>
#include <unistd.h>

#include <iostream>
#include <memory>
#include <tuple>

using namespace std;
using namespace iodrivers_base;

IOStream::~IOStream() {}
int IOStream::getFileDescriptor() const { return FDStream::INVALID_FD; }
bool IOStream::eof() const { return false; }
bool IOStream::hasIO(base::Time const& timeout) { return waitRead(timeout); };
bool IOStream::hasIO() { return hasIO(base::Time()); };

FDStream::FDStream(int fd, bool auto_close, bool has_eof)
    : m_auto_close(auto_close)
    , m_has_eof(has_eof)
    , m_eof(false)
    , m_fd(fd)

{
    if (setNonBlockingFlag(fd))
    {
        LOG_WARN_S << "FD given to Driver::setFileDescriptor is set as blocking, setting the NONBLOCK flag";
    }
}
FDStream::~FDStream()
{
    if (m_auto_close)
        ::close(m_fd);
}
void FDStream::setAutoClose(bool flag) {
    m_auto_close = flag;
}

bool FDStream::waitRead(base::Time const& timeout)
{
    fd_set set;
    FD_ZERO(&set);
    FD_SET(m_fd, &set);

    timeval timeout_spec = { static_cast<time_t>(timeout.toSeconds()), suseconds_t(timeout.toMicroseconds() % 1000000)};
    int ret = select(m_fd + 1, &set, NULL, NULL, &timeout_spec);
    if (ret < 0 && errno != EINTR)
        throw UnixError("waitRead(): error in select()");

    return (ret > 0);
}
bool FDStream::waitWrite(base::Time const& timeout)
{
    fd_set set;
    FD_ZERO(&set);
    FD_SET(m_fd, &set);

    timeval timeout_spec = { static_cast<time_t>(timeout.toSeconds()), suseconds_t(timeout.toMicroseconds() % 1000000) };
    int ret = select(m_fd + 1, NULL, &set, NULL, &timeout_spec);
    if (ret < 0 && errno != EINTR)
        throw UnixError("waitWrite(): error in select()");

    return (ret > 0);
}
size_t FDStream::read(uint8_t* buffer, size_t buffer_size)
{
    int c = ::read(m_fd, buffer, buffer_size);
    if (c > 0)
        return c;
    else if (c == 0)
    {
        m_eof = m_has_eof;
        return 0;
    }
    else
    {
        if (errno == EAGAIN)
            return 0;
        throw UnixError("readPacket(): error reading the file descriptor");
    }
}
bool FDStream::eof() const
{
    return m_eof;
}
size_t FDStream::write(uint8_t const* buffer, size_t buffer_size)
{
    int c = ::write(m_fd, buffer, buffer_size);
    if (c == -1 && errno != EAGAIN && errno != ENOBUFS)
        throw UnixError("writePacket(): error during write");
    if (c == -1)
        return 0;
    return c;
}
void FDStream::clear()
{
}
bool FDStream::setNonBlockingFlag(int fd)
{
    long fd_flags = fcntl(fd, F_GETFL);
    if (!(fd_flags & O_NONBLOCK))
    {
        if (fcntl(fd, F_SETFL, fd_flags | O_NONBLOCK) == -1)
            throw UnixError("cannot set the O_NONBLOCK flag");
        return true;
    }
    return false;
}
int FDStream::getFileDescriptor() const { return m_fd; }

UDPServerStream::UDPServerStream(int fd, bool auto_close)
    : FDStream(fd,auto_close)
    , m_s_len(sizeof(m_si_other))
    , m_si_other_dynamic(true)
    , m_has_other(false)
    , m_ignore_econnrefused(true)
    , m_ignore_ehostunreach(true)
    , m_ignore_enetunreach(true)
    , m_wait_read_error(0)
{
}

UDPServerStream::UDPServerStream(int fd, bool auto_close, struct sockaddr *si_other, size_t *s_len)
    : FDStream(fd,auto_close)
    , m_si_other(*si_other)
    , m_s_len(*s_len)
    , m_si_other_dynamic(false)
    , m_has_other(true)
    , m_ignore_econnrefused(true)
    , m_ignore_ehostunreach(true)
    , m_ignore_enetunreach(true)
    , m_wait_read_error(0)
{
}

void UDPServerStream::setIgnoreEnetUnreach(bool enable) {
    m_ignore_enetunreach = enable;
}

void UDPServerStream::setIgnoreEhostUnreach(bool enable) {
    m_ignore_ehostunreach = enable;
}

void UDPServerStream::setIgnoreEconnRefused(bool enable) {
    m_ignore_econnrefused = enable;
}

bool UDPServerStream::waitRead(base::Time const& timeout) {
    if (m_wait_read_error) {
        return false;
    }

    base::Time now = base::Time::now();
    base::Time deadline = now + timeout;
    while (now <= deadline) {
        if (!FDStream::waitRead(deadline - now))
            return false;

        now = base::Time::now();

        // We do a zero-size read to read the error from the socket, and ignore
        // the ones we want to ignore
        uint8_t buf[0];
        ssize_t ret;
        int err;
        tie(ret, err) = recvfrom(buf, 0, MSG_PEEK, NULL, NULL);
        if (ret < 0) {
            if (m_ignore_econnrefused && err == ECONNREFUSED) {
                continue;
            }
            else if (m_ignore_ehostunreach && err == EHOSTUNREACH) {
                continue;
            }
            else if (m_ignore_enetunreach && err == ENETUNREACH) {
                continue;
            }
            m_wait_read_error = err;
        }
        else {
            m_wait_read_error = 0;
        }

        return true;
    }
    return FDStream::waitRead(base::Time());
}

pair<ssize_t, int> UDPServerStream::recvfrom(
    uint8_t* buffer, size_t buffer_size, int flags,
    sockaddr* s_other, socklen_t* s_len
) {
    ssize_t ret = ::recvfrom(m_fd, buffer, buffer_size, flags, s_other, s_len);
    return make_pair(ret, errno);
}

size_t UDPServerStream::read(uint8_t* buffer, size_t buffer_size)
{
    if (m_wait_read_error) {
        int err = m_wait_read_error;
        m_wait_read_error = 0;
        throw UnixError("readPacket(): error reading the file descriptor", err);
    }

    sockaddr si_other;
    unsigned int s_len = sizeof(si_other);

    ssize_t ret;
    int err;
    if (m_si_other_dynamic)
        tie(ret, err) = recvfrom(buffer, buffer_size, 0, &si_other, &s_len);
    else
        tie(ret, err) = recvfrom(buffer, buffer_size, 0, NULL, NULL);

    if (ret >= 0){
        m_has_other = true;
        if (m_si_other_dynamic) {
            m_si_other = si_other;
            m_s_len = s_len;
        }

        if (ret == 0) {
            m_eof = true;
        }
        return ret;
    }
    else
    {
        if (err == EAGAIN) {
            return 0;
        }
        else if (m_ignore_econnrefused && err == ECONNREFUSED) {
            return 0;
        }
        else if (m_ignore_ehostunreach && err == EHOSTUNREACH) {
            return 0;
        }
        else if (m_ignore_enetunreach && err == ENETUNREACH) {
            return 0;
        }
        throw UnixError("readPacket(): error reading the file descriptor", err);
    }
}

pair<ssize_t, int> UDPServerStream::sendto(uint8_t const* buffer, size_t buffer_size) {
    ssize_t ret = ::sendto(m_fd, buffer, buffer_size, 0, &m_si_other, m_s_len);
    return make_pair(ret, errno);
}

size_t UDPServerStream::write(uint8_t const* buffer, size_t buffer_size)
{
    if (! m_has_other)
        return buffer_size;

    ssize_t ret;
    int err;
    tie(ret, err) = sendto(buffer, buffer_size);
    if (ret == -1) {
        if (err == EAGAIN && err == ENOBUFS) {
            return 0;
        }
        else if (m_ignore_econnrefused && err == ECONNREFUSED) {
            return buffer_size;
        }
        else if (m_ignore_ehostunreach && err == EHOSTUNREACH) {
            return buffer_size;
        }
        else if (m_ignore_enetunreach && err == ENETUNREACH) {
            return buffer_size;
        }

        throw UnixError("UDPServerStream: writePacket(): error during write", err);
    }
    return ret;
}

UnixServerStream::UnixServerStream(int fd, bool auto_close)
    : m_server_fd(fd)
    , m_auto_close(auto_close)
    , m_fd_stream(fd, false)
{
}

UnixServerStream::~UnixServerStream()
{
    if (m_auto_close) {
        ::close(m_server_fd);
    }
}

bool UnixServerStream::waitRead(base::Time const& timeout)
{
    if (m_client_stream) {
        return m_client_stream->waitRead(timeout);
    }

    base::Time now = base::Time::now();
    base::Time deadline = now + timeout;
    while (now <= deadline) {
        if (!m_fd_stream.waitRead(deadline - now)) {
            return false;
        }

        accept();
        return true;
    }
    return false;
}

void UnixServerStream::accept()
{
    int fd = ::accept(getFileDescriptor(), nullptr, nullptr);
    if (fd != -1) {
        m_client_stream = std::make_unique<FDStream>(fd, true);
    }
    else {
        std::cerr << strerror(errno) << std::endl;
    }
}

bool UnixServerStream::waitWrite(base::Time const& timeout)
{
    if (m_client_stream) {
        return m_client_stream->waitWrite(timeout);
    }

    base::Time deadline = base::Time::now() + timeout;
    if (!waitRead(timeout)) {
        return false;
    }

    auto now = base::Time::now();
    base::Time new_timeout;
    if (now < deadline) {
        new_timeout = deadline - now;
    }
    return m_client_stream->waitWrite(new_timeout);
}

size_t UnixServerStream::read(uint8_t* buffer, size_t buffer_size)
{
    if (!m_client_stream) {
        if (!waitRead(base::Time())) {
            return 0;
        }
    }

    if (m_client_stream) {
        return m_client_stream->read(buffer, buffer_size);
    }

    return 0;
}

size_t UnixServerStream::write(uint8_t const* buffer, size_t buffer_size)
{
    if (m_client_stream) {
        return m_client_stream->write(buffer, buffer_size);
    }
    return 0;
}

void UnixServerStream::clear()
{
    if (m_client_stream) {
        m_client_stream->clear();
    }
}

bool UnixServerStream::eof() const
{
    if (m_client_stream) {
        return m_client_stream->eof();
    }
    return false;
}

bool UnixServerStream::hasIO(base::Time const& timeout)
{
    if (m_client_stream) {
        return m_client_stream->hasIO(timeout);
    }
    return m_fd_stream.hasIO(timeout);
}

int UnixServerStream::getFileDescriptor() const
{
    if (m_client_stream) {
        return m_client_stream->getFileDescriptor();
    }
    else {
        return m_server_fd;
    }
}