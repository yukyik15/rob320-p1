#include <teleop_keyboard/teleop_keyboard.hpp>

TeleopKeyboard::TeleopKeyboard(std::unique_ptr<rix::ipc::interfaces::IO> input,
                               std::unique_ptr<rix::ipc::interfaces::IO> output, double linear_speed,
                               double angular_speed)
    : input(std::move(input)), output(std::move(output)), linear_speed(linear_speed), angular_speed(angular_speed) {}

void TeleopKeyboard::spin(std::unique_ptr<rix::ipc::interfaces::Notification> notif) {
    /* TODO */
    // Nonblocking so we can notice SIGINT promptly while waiting for input.
    input->set_nonblocking(true);

    uint32_t seq = 0;

    auto sig_received = [&]() -> bool {
        return (notif != nullptr) && notif->wait(rix::util::Duration(0.0));
    };

    auto write_exact = [&](const uint8_t* src, size_t nbytes) -> bool {
        size_t sent = 0;
        while (sent < nbytes) {
            if (sig_received()) return false;
            ssize_t w = output->write(src + sent, nbytes - sent);
            if (w <= 0) {
                // stdout errors are fatal for this program
                return false;
            }
            sent += static_cast<size_t>(w);
        }
        return true;
    };

    while (true) {
        // Stop immediately on SIGINT -> do not emit any further commands.
        if (sig_received()) return;

        // Wait a bit for input so we don't busy-spin.
        input->wait_for_readable(rix::util::Duration(0.01));  // 10ms

        // Read a single character
        uint8_t ch = 0;
        ssize_t r = input->read(&ch, 1);

        if (r == 0) {
            // EOF on input -> terminate
            return;
        }
        if (r < 0) {
            // Nonblocking: no data yet
            if (errno == EAGAIN || errno == EWOULDBLOCK) {
                continue;
            }
            // Other read error -> terminate
            return;
        }

        // Map key -> twist. Invalid keys => no command.
        bool valid = true;
        double vx = 0.0, vy = 0.0, wz = 0.0;

        switch (static_cast<char>(ch)) {
            case 'W': vx = +linear_speed; break;
            case 'A': vy = +linear_speed; break;
            case 'S': vx = -linear_speed; break;
            case 'D': vy = -linear_speed; break;
            case 'Q': wz = +angular_speed; break;
            case 'E': wz = -angular_speed; break;
            case ' ': vx = 0.0; vy = 0.0; wz = 0.0; break;
            default: valid = false; break;
        }

        if (!valid) {
            continue;  // ignore invalid keys
        }

        // If SIGINT happened after reading the key, we must not write a command
        if (sig_received()) return;

        // Build message
        geometry::Twist2DStamped msg{};
        msg.header.seq = seq++;
        msg.header.frame_id = "mbot";
        msg.header.stamp = rix::util::Time::now().to_msg();;

        msg.twist.vx = vx;
        msg.twist.vy = vy;
        msg.twist.wz = wz;

        // Serialize
        const uint32_t n = msg.size();
        std::vector<uint8_t> payload(n);
        size_t off = 0;
        msg.serialize(payload.data(), off);

        // Write size prefix (UInt32) then payload
        uint8_t size_buf[4];
        size_t off2 = 0;
        ::rix::msg::detail::serialize_number<uint32_t>(size_buf, off2, n);

        if (!write_exact(size_buf, 4)) return;
        if (n > 0 && !write_exact(payload.data(), payload.size())) return;
    }
}