#include <gmock/gmock.h>

#include <mbot/mbot_base.hpp>

class MockMBot : public MBotBase {
   public:
    MockMBot() {
        ON_CALL(*this, ok).WillByDefault([this]() -> bool { return true; });
        ON_CALL(*this, drive).WillByDefault([this](const Twist2DStamped &cmd) -> void { twists.push_back(cmd); });
    }

    MOCK_METHOD(bool, ok, (), (const, override));
    MOCK_METHOD(void, drive, (const Twist2DStamped &cmd), (const, override));

    std::vector<rix::msg::geometry::Twist2DStamped> twists;
};