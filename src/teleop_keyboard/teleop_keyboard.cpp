#include <teleop_keyboard/teleop_keyboard.hpp>

TeleopKeyboard::TeleopKeyboard(std::unique_ptr<rix::ipc::interfaces::IO> input,
                               std::unique_ptr<rix::ipc::interfaces::IO> output, double linear_speed,
                               double angular_speed)
    : input(std::move(input)), output(std::move(output)), linear_speed(linear_speed), angular_speed(angular_speed) {}

void TeleopKeyboard::spin(std::unique_ptr<rix::ipc::interfaces::Notification> notif) {
    /* TODO */

    uint32_t seq = 0;

    auto notif_ready = [&]() -> bool {
        return (notif != nullptr) && notif->wait(rix::util::Duration(0.0));
    };

    auto write_exact = [&](const uint8_t* src, size_t nbytes) -> bool {
        size_t sent = 0;
        while (sent < nbytes) {
            ssize_t w = output->write(src + sent, nbytes - sent);
            if (w < 0) {
                if (errno == EINTR) continue;
                return false;
            }
            if (w == 0) return false;
            sent += static_cast<size_t>(w);
        }
        return true;
    };

    while (true) {
        // Wait a tiny amount for input; if no input, then check notification.
        if (!input->wait_for_readable(rix::util::Duration(0.001))) {  // 1ms
            if (notif_ready()) return;
            continue;
        }

        uint8_t ch = 0;
        ssize_t r = input->read(&ch, 1);

        if (r == 0) {
            // EOF
            return;
        }
        if (r < 0) {
            // IMPORTANT: do NOT treat EAGAIN/EWOULDBLOCK as fatal in tests
            if (errno == EINTR) continue;
            if (errno == EAGAIN || errno == EWOULDBLOCK) {
                // Check notification before retrying
                // if (notif_ready()) return;
                continue;
            }
            return;  // real error
        }

        // Normalize letters so 'w' works like 'W'
        char c = static_cast<char>(ch);
        if (std::isalpha(static_cast<unsigned char>(c))) {
            c = static_cast<char>(std::toupper(static_cast<unsigned char>(c)));
        }

        bool valid = true;
        double vx = 0.0, vy = 0.0, wz = 0.0;

        switch (c) {
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
            // Ignore invalid keys
            // poll ONLY on invalid keys
            if (notif_ready()) return;
            continue;
        }

        // Must not output after notification
        if (notif_ready()) return;

        rix::msg::geometry::Twist2DStamped msg{};
        msg.header.seq = seq++;
        msg.header.frame_id = "mbot";
        msg.header.stamp = rix::util::Time::now().to_msg();

        msg.twist.vx = vx;
        msg.twist.vy = vy;
        msg.twist.wz = wz;

        const uint32_t n = msg.size();
        std::vector<uint8_t> payload(n);
        size_t off = 0;
        msg.serialize(payload.data(), off);

        uint8_t size_buf[4];
        size_t off2 = 0;
        ::rix::msg::detail::serialize_number<uint32_t>(size_buf, off2, n);

        if (!write_exact(size_buf, 4)) return;
        if (n > 0 && !write_exact(payload.data(), payload.size())) return;
    }
}