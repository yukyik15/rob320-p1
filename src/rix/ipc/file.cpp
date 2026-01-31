#include "rix/ipc/file.hpp"

namespace rix {
namespace ipc {

/**< TODO */
bool File::remove(const std::string &pathname) {
    return (::unlink(pathname.c_str()) == 0);
}

File::File() : fd_(-1) {}

File::File(int fd) : fd_(fd) {}

/**< TODO */
File::File(std::string pathname, int creation_flags, mode_t mode) {
    // open() ignores mode unless O_CREAT is set, so it's safe to always pass it.
    fd_ = ::open(pathname.c_str(), creation_flags, mode);
}

/**< TODO */
File::File(const File &src) {
    if (src.fd_ >= 0) {
        fd_ = ::dup(src.fd_);
    }
}

/**< TODO */
File &File::operator=(const File &src) { 
    if (this == &src) {
        return *this;
    }

    // Close current fd if we own a "real" fd (avoid closing stdin/stdout/stderr).
    if (fd_ > 0) {
        ::close(fd_);
        fd_ = -1;
    }

    if (src.fd_ >= 0) {
        fd_ = ::dup(src.fd_);
    } else {
        fd_ = -1;
    }

    return *this;
}

File::File(File &&src) : fd_(-1) { std::swap(fd_, src.fd_); }

File &File::operator=(File &&src) {
    if (fd_ > 0) {
        ::close(fd_);
        fd_ = -1;
    }
    std::swap(fd_, src.fd_);
    return *this;
}

/**< TODO */
File::~File() {
    // Avoid closing stdin/stdout/stderr by convention (fd 0,1,2).
    if (fd_ > 0) {
        ::close(fd_);
        fd_ = -1;
    }
}

/**< TODO */
ssize_t File::read(uint8_t *buffer, size_t size) const {
    if (fd_ < 0) {
        return -1;
    }
    return ::read(fd_, buffer, size);
}

/**< TODO */
ssize_t File::write(const uint8_t *buffer, size_t size) const {
    if (fd_ < 0) {
        return -1;
    }
    return ::write(fd_, buffer, size);
}

int File::fd() const { return fd_; }

/**< TODO */
bool File::ok() const {
    return fd_ >= 0;
}

/**< TODO */
void File::set_nonblocking(bool status) {
    if (fd_ < 0) {
        return;
    }

    int flags = ::fcntl(fd_, F_GETFL);
    if (flags < 0) {
        return;
    }

    if (status) {
        flags |= O_NONBLOCK;
    } else {
        flags &= ~O_NONBLOCK;
    }

    (void)::fcntl(fd_, F_SETFL, flags);
}

/**< TODO */
bool File::is_nonblocking() const {
    if (fd_ < 0) {
        return false;
    }

    int flags = ::fcntl(fd_, F_GETFL);
    if (flags < 0) {
        return false;
    }

    return (flags & O_NONBLOCK) != 0;
}

/**< TODO */
bool File::wait_for_writable(const util::Duration &duration) const {
    if (fd_ < 0) {
        return false;
    }

    int timeout_ms = 0;
    auto ns = duration.to_nanoseconds();
    if (ns < 0) {
        timeout_ms = 0;
    } else {
        // poll() takes milliseconds; rounding up avoids timing out "too early"
        // for small non-zero durations.
        int64_t ms = duration.to_milliseconds(util::Time::RoundType::CEIL);
        if (ms > static_cast<int64_t>(std::numeric_limits<int>::max())) {
            timeout_ms = std::numeric_limits<int>::max();
        } else {
            timeout_ms = static_cast<int>(ms);
        }
    }

    struct pollfd pfd;
    pfd.fd = fd_;
    pfd.events = POLLOUT;
    pfd.revents = 0;

    int ret = ::poll(&pfd, 1, timeout_ms);
    if (ret <= 0) {
        return false;
    }
    return (pfd.revents & POLLOUT) != 0;
}

/**< TODO */
bool File::wait_for_readable(const util::Duration &duration) const {
    if (fd_ < 0) {
        return false;
    }

    int timeout_ms = 0;
    auto ns = duration.to_nanoseconds();
    if (ns < 0) {
        timeout_ms = 0;
    } else {
        int64_t ms = duration.to_milliseconds(util::Time::RoundType::CEIL);
        if (ms > static_cast<int64_t>(std::numeric_limits<int>::max())) {
            timeout_ms = std::numeric_limits<int>::max();
        } else {
            timeout_ms = static_cast<int>(ms);
        }
    }

    struct pollfd pfd;
    pfd.fd = fd_;
    pfd.events = POLLIN;
    pfd.revents = 0;

    int ret = ::poll(&pfd, 1, timeout_ms);
    if (ret <= 0) {
        return false;
    }
    return (pfd.revents & POLLIN) != 0;
}

}  // namespace ipc
}  // namespace rix