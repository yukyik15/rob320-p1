#include "teleop_keyboard/teleop_keyboard.hpp"

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include "mocks/mock_io.hpp"
#include "mocks/mock_notification.hpp"

std::map<char, rix::msg::geometry::Twist2D> twist_map;

void init_twist_map(double linear, double angular) {
    auto twist = &twist_map['q'];
    twist->vx = 0;
    twist->vy = 0;
    twist->wz = angular;

    twist = &twist_map['w'];
    twist->vx = linear;
    twist->vy = 0;
    twist->wz = 0;
    
    twist = &twist_map['e'];
    twist->vx = 0;
    twist->vy = 0;
    twist->wz = -angular;
    
    twist = &twist_map['a'];
    twist->vx = 0;
    twist->vy = linear;
    twist->wz = 0;
    
    twist = &twist_map['s'];
    twist->vx = -linear;
    twist->vy = 0;
    twist->wz = 0;
    
    twist = &twist_map['d'];
    twist->vx = 0;
    twist->vy = -linear;
    twist->wz = 0;
    
    twist = &twist_map[' '];
    twist->vx = 0;
    twist->vy = 0;
    twist->wz = 0;
}

void convert_buffer_to_twists(const std::vector<uint8_t> &buffer,
                              std::vector<rix::msg::geometry::Twist2DStamped> &twists) {
    const size_t buffer_size = buffer.size();
    size_t offset = 0;
    uint32_t twist_size = 0;
    while (offset < buffer_size) {
        twist_size = *(uint32_t *)&buffer[offset];
        offset += 4;
        rix::msg::geometry::Twist2DStamped twist;
        ASSERT_TRUE(twist.deserialize(buffer.data(), buffer_size, offset));
        ASSERT_EQ(twist.size(), twist_size);
        twists.push_back(twist);
    }
}

void twist_equal(const rix::msg::geometry::Twist2D &a, const rix::msg::geometry::Twist2D &b) {
    EXPECT_EQ(a.vx, b.vx);
    EXPECT_EQ(a.vy, b.vy);
    EXPECT_EQ(a.wz, b.wz);
}

void validate_twists(const char *input, size_t input_size, std::vector<rix::msg::geometry::Twist2DStamped> &twists) {
    size_t j = 0;
    for (size_t i = 0; i < input_size; i++) {
        auto it = twist_map.find(input[i]);
        if (it == twist_map.end()) continue;
        twist_equal(twists[j++].twist, it->second);
    }
}

TEST(TeleopKeyboardTest, ExitsOnNotification) {
    auto input = std::make_unique<testing::NiceMock<MockIO>>();
    auto output = std::make_unique<testing::NiceMock<MockIO>>();
    auto output_ptr = output.get();  // Get raw pointer for inspection

    init_twist_map(0.5, 1.5);
    auto teleop_keyboard = std::make_unique<TeleopKeyboard>(std::move(input), std::move(output), 0.5, 1.5);
    auto notif = std::make_unique<testing::NiceMock<MockNotification>>();

    notif->raise();
    teleop_keyboard->spin(std::move(notif));

    EXPECT_EQ(output_ptr->get_buffer().size(), 0);
}

TEST(TeleopKeyboardTest, TranslatesKeysAndExitsOnEOF) {
    auto input = std::make_unique<testing::NiceMock<MockIO>>();
    const char *data = "qwe asd";
    input->write((uint8_t *)data, 7);
    input->close_write_end();

    auto output = std::make_unique<testing::NiceMock<MockIO>>();
    auto output_ptr = output.get();  // Get raw pointer for inspection

    init_twist_map(0.5, 1.5);
    auto teleop_keyboard = std::make_unique<TeleopKeyboard>(std::move(input), std::move(output), 0.5, 1.5);
    auto notif = std::make_unique<testing::NiceMock<MockNotification>>();

    teleop_keyboard->spin(std::move(notif));

    std::vector<rix::msg::geometry::Twist2DStamped> twists;
    convert_buffer_to_twists(output_ptr->get_buffer(), twists);

    ASSERT_EQ(twists.size(), 7);
    validate_twists(data, 7, twists);
}

TEST(TeleopKeyboardTest, TranslatesInvalidKeysAndExitsOnEOF) {
    auto input = std::make_unique<testing::NiceMock<MockIO>>();
    const char *data = "abcdefghijklmnopqrstuvwxyz ";
    input->write((uint8_t *)data, 27);
    input->close_write_end();

    auto output = std::make_unique<testing::NiceMock<MockIO>>();
    auto output_ptr = output.get();  // Get raw pointer for inspection

    init_twist_map(1.0, 3.14);
    auto teleop_keyboard = std::make_unique<TeleopKeyboard>(std::move(input), std::move(output), 1.0, 3.14);
    auto notif = std::make_unique<testing::NiceMock<MockNotification>>();

    teleop_keyboard->spin(std::move(notif));

    std::vector<rix::msg::geometry::Twist2DStamped> twists;
    convert_buffer_to_twists(output_ptr->get_buffer(), twists);

    ASSERT_EQ(twists.size(), 7);
    validate_twists(data, 7, twists);
}

TEST(TeleopKeyboardTest, TranslatesInvalidKeysAndExitsOnNotification) {
    auto input = std::make_unique<testing::NiceMock<MockIO>>();
    const char *data = "abcdefghijklmnopqrstuvwxyz ";
    input->write((uint8_t *)data, 27);
    input->close_write_end();

    auto output = std::make_unique<testing::NiceMock<MockIO>>();
    auto output_ptr = output.get();  // Get raw pointer for inspection

    init_twist_map(1.0, 3.14);
    auto teleop_keyboard = std::make_unique<TeleopKeyboard>(std::move(input), std::move(output), 1.0, 3.14);
    auto notif = std::make_unique<testing::NiceMock<MockNotification>>(5);

    teleop_keyboard->spin(std::move(notif));

    std::vector<rix::msg::geometry::Twist2DStamped> twists;
    convert_buffer_to_twists(output_ptr->get_buffer(), twists);

    ASSERT_EQ(twists.size(), 3); // a, d, e
    validate_twists(data, 5, twists); // abcde
}