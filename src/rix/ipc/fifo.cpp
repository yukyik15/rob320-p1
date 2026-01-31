#include "rix/ipc/fifo.hpp"

namespace rix {
namespace ipc {

/**< TODO */
Fifo::Fifo(const std::string &pathname, Mode mode, bool nonblocking) 
    : File(), mode_(mode), pathname_(pathname) {
    // Create fifo special file if it doesn't exist
    // Use a common default permission (rw for everyone) cuz umask may further restrict
    if (::mkfifo(pathname.c_str(), 0666) != 0) {
        if (errno != EEXIST) {
            // Creation doesn't work for a real reason -> leave fd_ invalid
            fd_ = -1;
            return;
        }
    }

    // Spec says: "opened for both reading and writing"
    int flags = O_RDWR;
    if (nonblocking) {
        flags |= O_NONBLOCK;
    }

    fd_ = ::open(pathname.c_str(), flags);
}

Fifo::Fifo() {}

/**< TODO */
Fifo::Fifo(const Fifo &other) 
    : File(), mode_(other.mode_), pathname_(other.pathname_) {
    if (other.fd_ >= 0) {
        fd_ = ::dup(other.fd_);
    } else {
        fd_ = -1;
    }
}

/**< TODO */
Fifo &Fifo::operator=(const Fifo &other) {
    if (this == &other) {
        return *this;
    }

    // Close current fd if valid (avoid stdin/stdout/stderr)
    if (fd_ > 0) {
        ::close(fd_);
        fd_ = -1;
    }

    pathname_ = other.pathname_;
    mode_ = other.mode_;

    if (other.fd_ >= 0) {
        fd_ = ::dup(other.fd_);
    } else {
        fd_ = -1;
    }

    return *this;
}

Fifo::Fifo(Fifo &&other)
    : File(std::move(other)),
      pathname_(std::move(other.pathname_)),
      mode_(std::move(other.mode_)) {
}

Fifo &Fifo::operator=(Fifo &&other) {
    if (fd_ > 0) {
        ::close(fd_);
        fd_ = -1;
        pathname_ = "";
    }
    std::swap(fd_, other.fd_);
    pathname_ = std::move(other.pathname_);
    mode_ = std::move(other.mode_);
    return *this;
}

Fifo::~Fifo() {}

std::string Fifo::pathname() const { return pathname_; }

Fifo::Mode Fifo::mode() const { return mode_; }

}  // namespace ipc
}  // namespace rix