#include "mbot_driver/mbot_driver.hpp"

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include "mocks/mock_io.hpp"
#include "mocks/mock_mbot.hpp"
#include "mocks/mock_notification.hpp"

void twist_equal(const rix::msg::geometry::Twist2D &a, const rix::msg::geometry::Twist2D &b) {
    EXPECT_EQ(a.vx, b.vx);
    EXPECT_EQ(a.vy, b.vy);
    EXPECT_EQ(a.wz, b.wz);
}

TEST(MBotDriverTest, ExitsOnNotification) {
    auto input = std::make_unique<testing::NiceMock<MockIO>>();
    
    auto mbot = std::make_unique<testing::NiceMock<MockMBot>>();
    auto *mbot_ptr = mbot.get(); // Need to get raw pointer for inspection

    auto notif = std::make_unique<testing::NiceMock<MockNotification>>();
    auto mbot_driver = std::make_unique<testing::NiceMock<MBotDriver>>(std::move(input), std::move(mbot));

    notif->raise();
    mbot_driver->spin(std::move(notif));

    ASSERT_EQ(mbot_ptr->twists.size(), 1);
    EXPECT_EQ(mbot_ptr->twists[0].twist.vx, 0.0f);
    EXPECT_EQ(mbot_ptr->twists[0].twist.vy, 0.0f);
    EXPECT_EQ(mbot_ptr->twists[0].twist.wz, 0.0f);
}

TEST(MBotDriverTest, TranslatesDriveCommandsAndExitsOnEOF) {
    rix::msg::geometry::Twist2DStamped twist1;
    twist1.header.frame_id = "my_mbot_one";
    twist1.twist.vx = 1.0f;
    twist1.twist.vy = 2.0f;
    twist1.twist.wz = 3.0f;
    rix::msg::geometry::Twist2DStamped twist2;
    twist1.header.frame_id = "mbot_two";
    twist1.twist.vx = 4.0f;
    twist1.twist.vy = 5.0f;
    twist1.twist.wz = 6.0f;

    rix::msg::standard::UInt32 size_msg1, size_msg2;
    size_msg1.data = twist1.size();
    size_msg2.data = twist2.size();

    std::vector<uint8_t> buffer(2 * (size_msg1.size() + size_msg1.data) + size_msg2.size() + size_msg2.data);
    
    size_t offset = 0;
    size_msg1.serialize(buffer.data(), offset);
    twist1.serialize(buffer.data(), offset);
    size_msg2.serialize(buffer.data(), offset);
    twist2.serialize(buffer.data(), offset);
    size_msg1.serialize(buffer.data(), offset);
    twist1.serialize(buffer.data(), offset);

    auto input = std::make_unique<testing::NiceMock<MockIO>>();
    input->write(buffer.data(), buffer.size());
    input->close_write_end();

    auto mbot = std::make_unique<testing::NiceMock<MockMBot>>();
    auto *mbot_ptr = mbot.get(); // Need to get raw pointer for inspection

    auto mbot_driver = std::make_unique<testing::NiceMock<MBotDriver>>(std::move(input), std::move(mbot));
    
    auto notif = std::make_unique<testing::NiceMock<MockNotification>>();

    mbot_driver->spin(std::move(notif));

    ASSERT_EQ(mbot_ptr->twists.size(), 4);
    twist_equal(mbot_ptr->twists[0].twist, twist1.twist);
    twist_equal(mbot_ptr->twists[1].twist, twist2.twist);
    twist_equal(mbot_ptr->twists[2].twist, twist1.twist);
    twist_equal(mbot_ptr->twists[3].twist, {});
}

TEST(MBotDriverTest, TranslatesDriveCommandsAndExitsOnNotification) {
    rix::msg::geometry::Twist2DStamped twist1;
    twist1.header.frame_id = "mbot_one";
    twist1.twist.vx = 1.0f;
    twist1.twist.vy = 2.0f;
    twist1.twist.wz = 3.0f;
    rix::msg::geometry::Twist2DStamped twist2;
    twist1.header.frame_id = "my_mbot_two";
    twist1.twist.vx = 4.0f;
    twist1.twist.vy = 5.0f;
    twist1.twist.wz = 6.0f;

    rix::msg::standard::UInt32 size_msg1, size_msg2;
    size_msg1.data = twist1.size();
    size_msg2.data = twist2.size();

    std::vector<uint8_t> buffer(2 * (size_msg1.size() + size_msg1.data) + size_msg2.size() + size_msg2.data);
    
    size_t offset = 0;
    size_msg1.serialize(buffer.data(), offset);
    twist1.serialize(buffer.data(), offset);
    size_msg2.serialize(buffer.data(), offset);
    twist2.serialize(buffer.data(), offset);
    size_msg1.serialize(buffer.data(), offset);
    twist1.serialize(buffer.data(), offset);

    auto input = std::make_unique<testing::NiceMock<MockIO>>();
    input->write(buffer.data(), buffer.size());
    input->close_write_end();

    auto mbot = std::make_unique<testing::NiceMock<MockMBot>>();
    auto *mbot_ptr = mbot.get(); // Need to get raw pointer for inspection

    auto mbot_driver = std::make_unique<testing::NiceMock<MBotDriver>>(std::move(input), std::move(mbot));
    
    auto notif = std::make_unique<testing::NiceMock<MockNotification>>(2); // Set counter_max to 2 (wait returns true, true, false)

    mbot_driver->spin(std::move(notif));

    ASSERT_EQ(mbot_ptr->twists.size(), 3);
    twist_equal(mbot_ptr->twists[0].twist, twist1.twist);
    twist_equal(mbot_ptr->twists[1].twist, twist2.twist);
    twist_equal(mbot_ptr->twists[2].twist, {});
}