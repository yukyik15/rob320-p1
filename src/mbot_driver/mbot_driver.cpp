#include "mbot_driver/mbot_driver.hpp"

using namespace rix::ipc;
using namespace rix::msg;

MBotDriver::MBotDriver(std::unique_ptr<interfaces::IO> input, std::unique_ptr<MBotBase> mbot)
    : input(std::move(input)), mbot(std::move(mbot)) {}

void MBotDriver::spin(std::unique_ptr<interfaces::Notification> notif) {
    /* TODO */
    auto send_stop = [&]() {
        geometry::Twist2DStamped stop{};
        stop.twist.vx = 0.0;
        stop.twist.vy = 0.0;
        stop.twist.wz = 0.0;
        mbot->drive(stop);
    };

    // Make stdin non-blocking so we can notice SIGINT promptly.
    input->set_nonblocking(true);

    auto sig_received = [&]() -> bool {
        return (notif != nullptr) && notif->wait(rix::util::Duration(0.0));
    };

    auto read_exact = [&](uint8_t *dst, size_t nbytes) -> bool {
        size_t got = 0;
        while (got < nbytes) {
            if (sig_received()) {
                return false;  // caller will stop+return
            }

            ssize_t r = input->read(dst + got, nbytes - got);

            if (r == 0) {
                // EOF
                return false;
            }
            if (r < 0) {
                // Non-blocking: no data yet
                if (errno == EAGAIN || errno == EWOULDBLOCK) {
                    // Wait a bit for stdin to become readable, then retry
                    input->wait_for_readable(rix::util::Duration(0.01));  // 10ms
                    continue;
                }
                // Other error
                return false;
            }

            got += static_cast<size_t>(r);
        }
        return true;
    };

    while (true) {
        if (sig_received()) {
            send_stop();
            return;
        }

        // Wait briefly for data (so we don't busy-spin).
        input->wait_for_readable(rix::util::Duration(0.01));

        // Read 4-byte size prefix
        uint8_t size_buf[4];
        if (!read_exact(size_buf, 4)) {
            send_stop();
            return;
        }

        // Deserialize uint32 length (RIX serialization for numbers is raw bytes)
        uint32_t msg_size = 0;
        size_t off = 0;
        if (!detail::deserialize_number<uint32_t>(msg_size, size_buf, 4, off)) {
            send_stop();
            return;
        }

        // Read the serialized message payload
        std::vector<uint8_t> payload(msg_size);
        if (msg_size > 0) {
            if (!read_exact(payload.data(), msg_size)) {
                send_stop();
                return;
            }
        }

        // Deserialize and drive
        geometry::Twist2DStamped cmd{};
        off = 0;
        if (!cmd.deserialize(payload.data(), payload.size(), off)) {
            // Bad/partial message; ignore and keep going
            continue;
        }

        mbot->drive(cmd);
    }
}