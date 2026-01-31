#include "rix/ipc/signal.hpp"

#include <iostream>

namespace rix {
namespace ipc {

std::array<Signal::SignalNotifier, 32> Signal::notifier = {};

/**< TODO */
Signal::Signal(int signum) : signum_(signum - 1) {
    if (signum < 1 || signum > 32) {
        throw std::invalid_argument("Signal: signum must be in [1, 32]");
    }

    const int idx = signum - 1;

    // Only one Signal object may manage a given signum at a time.
    if (notifier[idx].is_init) {
        throw std::invalid_argument("Signal: handler for this signum already exists");
    }

    // Initialize the pipe pair for this signal number
    notifier[idx].pipe = Pipe::create();
    notifier[idx].is_init = true;

    // Store index internally (0..31)
    signum_ = idx;

    // Register our handler for this signal number
    ::signal(signum, Signal::handler);
}

/**< TODO */
Signal::~Signal() {
    if (signum_ >= 0 && signum_ < 32) {
        if (notifier[signum_].is_init) {
            notifier[signum_].is_init = false;
            notifier[signum_].pipe = {};
            ::signal(signum_ + 1, SIG_DFL);
        }
    }
    signum_ = -1;
}

Signal::Signal(Signal &&other) : signum_(-1) { std::swap(signum_, other.signum_); }

Signal &Signal::operator=(Signal &&other) {
    if (signum_ >= 0 && signum_ < 32) {
        if (notifier[signum_].is_init) {
            notifier[signum_].is_init = false;
            notifier[signum_].pipe = {};
            ::signal(signum_+1, SIG_DFL);
        }
    }
    signum_ = -1;
    std::swap(signum_, other.signum_);
    return *this;
}

/**< TODO */
bool Signal::raise() const { 
    if (signum_ < 0 || signum_ >= 32) {
        return false;
    }
    return (::raise(signum_ + 1) == 0);
}

/**< TODO */
bool Signal::kill(pid_t pid) const {
    if (signum_ < 0 || signum_ >= 32) {
        return false;
    }
    return (::kill(pid, signum_ + 1) == 0);
}

// also changing this??
int Signal::signum() const { 
    if (signum_ < 0 || signum_ >= 32) return -1;
    return signum_ + 1;
}

/**< TODO */
bool Signal::wait(const rix::util::Duration &d) const {
    if (signum_ < 0 || signum_ >= 32) {
        return false;
    }
    if (!notifier[signum_].is_init) {
        return false;
    }

    // Wait for the read-end of the pipe to become readable
    Pipe &read_end = notifier[signum_].pipe[0];

    if (!read_end.wait_for_readable(d)) {
        return false;
    }

    // Drain the pipe so future waits don't immediately succeed
    const bool was_nonblocking = read_end.is_nonblocking();
    read_end.set_nonblocking(true);

    uint8_t buf[64];
    while (true) {
        ssize_t n = read_end.read(buf, sizeof(buf));
        if (n > 0) {
            continue;  // keep draining
        }
        if (n == 0) {
            break;     // pipe closed (shouldn't happen normally)
        }
        // n < 0
        if (errno == EAGAIN || errno == EWOULDBLOCK) {
            break;     // drained
        }
        break;         // other error; stop draining
    }

    read_end.set_nonblocking(was_nonblocking);
    return true;
}

/**< TODO */
void Signal::handler(int signum) {
    // signum is 1..32; notifier index is 0..31
    if (signum < 1 || signum > 32) {
        return;
    }
    const int idx = signum - 1;

    if (!notifier[idx].is_init) {
        return;
    }

    // Async-signal-safe: write a single byte to the write end of the pipe.
    const int fd = notifier[idx].pipe[1].fd();
    if (fd < 0) {
        return;
    }

    uint8_t b = 1;
    (void)::write(fd, &b, 1);
}

}  // namespace ipc
}  // namespace rix