#include "rix/ipc/pipe.hpp"

namespace rix {
namespace ipc {

/**< TODO */
std::array<Pipe, 2> Pipe::create() {
    int fds[2] = {-1, -1};
    if (::pipe(fds) != 0) {
        // Failed to create; return invalid pipes
        return {Pipe(), Pipe()};
    }

    // fds[0] = read end, fds[1] = write end
    return {Pipe(fds[0], true), Pipe(fds[1], false)};
}

Pipe::Pipe() : File(), read_end_(false) {}

/**< TODO */
Pipe::Pipe(const Pipe &other) : File(), read_end_(other.read_end_) {
    if (other.fd_ >= 0) {
        fd_ = ::dup(other.fd_);
    } else {
        fd_ = -1;
    }
}

/**< TODO */
Pipe &Pipe::operator=(const Pipe &other) {
    if (this == &other) {
        return *this;
    }

    // Close current fd if valid (avoid closing stdin/stdout/stderr)
    if (fd_ > 0) {
        ::close(fd_);
        fd_ = -1;
    }

    read_end_ = other.read_end_;

    if (other.fd_ >= 0) {
        fd_ = ::dup(other.fd_);
    } else {
        fd_ = -1;
    }

    return *this;
}

Pipe::Pipe(Pipe &&other) : File(std::move(other)), read_end_(other.read_end_) {}

Pipe &Pipe::operator=(Pipe &&other) {
    if (fd_ > 0) {
        ::close(fd_);
        fd_ = -1;
        read_end_ = false;
    }
    std::swap(fd_, other.fd_);
    read_end_ = std::move(other.read_end_);
    return *this;
}

Pipe::~Pipe() {}

bool Pipe::is_read_end() const { return read_end_; }

bool Pipe::is_write_end() const { return !read_end_; }

Pipe::Pipe(int fd, bool read_end) : File(fd), read_end_(read_end) {}

}  // namespace ipc
}  // namespace rix