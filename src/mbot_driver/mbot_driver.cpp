#include "mbot_driver/mbot_driver.hpp"

using namespace rix::ipc;
using namespace rix::msg;

MBotDriver::MBotDriver(std::unique_ptr<interfaces::IO> input, std::unique_ptr<MBotBase> mbot)
    : input(std::move(input)), mbot(std::move(mbot)) {}

void MBotDriver::spin(std::unique_ptr<interfaces::Notification> notif) {
    /* TODO */
    auto send_stop = [&]() {
        rix::msg::geometry::Twist2DStamped stop{};
        stop.twist.vx = 0.0;
        stop.twist.vy = 0.0;
        stop.twist.wz = 0.0;
        mbot->drive(stop);
    };

    auto notif_ready = [&]() -> bool {
        return (notif != nullptr) && notif->wait(rix::util::Duration(0.0));
    };

    // Read exactly n bytes (blocking). Returns false on EOF/error.
    auto read_exact = [&](uint8_t* dst, size_t n) -> bool {
        size_t got = 0;
        while (got < n) {
            ssize_t r = input->read(dst + got, n - got);
            if (r == 0) return false;                // EOF
            if (r < 0) {
                if (errno == EINTR) continue;        // interrupted by signal, retry
                return false;                        // real error
            }
            got += static_cast<size_t>(r);
        }
        return true;
    };

    while (true) {
        // Check notification between full messages (not in a tight loop)
        if (notif_ready()) {
            send_stop();
            return;
        }

        // Read 4-byte size prefix
        uint8_t size_buf[4];
        if (!read_exact(size_buf, 4)) {
            send_stop(); // EOF or error
            return;
        }

        uint32_t msg_size = 0;
        size_t off = 0;
        if (!::rix::msg::detail::deserialize_number<uint32_t>(msg_size, size_buf, 4, off)) {
            send_stop();
            return;
        }

        // Read payload
        std::vector<uint8_t> payload(msg_size);
        if (msg_size > 0) {
            if (!read_exact(payload.data(), msg_size)) {
                send_stop();
                return;
            }
        }

        // Deserialize + drive
        rix::msg::geometry::Twist2DStamped cmd{};
        off = 0;
        if (!cmd.deserialize(payload.data(), payload.size(), off)) {
            // Bad message: ignore and continue
            continue;
        }

        mbot->drive(cmd);
    }
}