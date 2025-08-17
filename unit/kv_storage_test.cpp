#include <chrono>
#include <memory>
#include <thread>

#include <gtest/gtest.h>
#include <kv_storage.hpp>

namespace vk {
    class MockClock {
    public:
        using time_point = std::chrono::system_clock::time_point;

        MockClock(time_point initial_time = std::chrono::system_clock::now()) :
            current_time_(std::make_shared<time_point>(initial_time)) {}

        time_point now() const { return *current_time_; }

        void advance_time(std::chrono::seconds duration) { *current_time_ += duration; }

    private:
        std::shared_ptr<time_point> current_time_;
    };

    class KVStorageTest : public ::testing::Test {
    protected:
        void SetUp() override { clock_ = MockClock(); }

        MockClock clock_;
    };

    TEST_F(KVStorageTest, ConstructorWithInitialData) {
        std::vector<std::tuple<std::string, std::string, uint32_t>> initial_data = {
                {"key1", "value1", 0}, {"key2", "value2", 10}, {"key3", "value3", 0}};

        KVStorage<MockClock> storage(initial_data, clock_);

        EXPECT_EQ(storage.get("key1"), "value1");
        EXPECT_EQ(storage.get("key2"), "value2");
        EXPECT_EQ(storage.get("key3"), "value3");
        EXPECT_EQ(storage.get("nonexistent"), std::nullopt);
    }

    TEST_F(KVStorageTest, SetAndGetOperations) {
        std::vector<std::tuple<std::string, std::string, uint32_t>> empty_data;
        KVStorage<MockClock> storage(empty_data, clock_);

        storage.set("key1", "value1", 0);
        storage.set("key2", "value2", 100);

        EXPECT_EQ(storage.get("key1"), "value1");
        EXPECT_EQ(storage.get("key2"), "value2");
        EXPECT_EQ(storage.get("nonexistent"), std::nullopt);
    }

    TEST_F(KVStorageTest, UpdateExistingKey) {
        std::vector<std::tuple<std::string, std::string, uint32_t>> initial_data = {{"key1", "old_value", 0}};
        KVStorage<MockClock> storage(initial_data, clock_);

        storage.set("key1", "new_value", 50);

        EXPECT_EQ(storage.get("key1"), "new_value");
    }

    TEST_F(KVStorageTest, RemoveOperations) {
        std::vector<std::tuple<std::string, std::string, uint32_t>> initial_data = {{"key1", "value1", 0},
                                                                                    {"key2", "value2", 0}};
        KVStorage<MockClock> storage(initial_data, clock_);

        EXPECT_TRUE(storage.remove("key1"));
        EXPECT_FALSE(storage.remove("key1")); // Already removed
        EXPECT_FALSE(storage.remove("nonexistent"));

        EXPECT_EQ(storage.get("key1"), std::nullopt);
        EXPECT_EQ(storage.get("key2"), "value2");
    }

    TEST_F(KVStorageTest, TTLExpiration) {
        std::vector<std::tuple<std::string, std::string, uint32_t>> initial_data = {{"permanent", "value", 0},
                                                                                    {"temporary", "value", 5}};
        KVStorage<MockClock> storage(initial_data, clock_);

        EXPECT_EQ(storage.get("permanent"), "value");
        EXPECT_EQ(storage.get("temporary"), "value");

        clock_.advance_time(std::chrono::seconds(6));

        EXPECT_EQ(storage.get("permanent"), "value");
        EXPECT_EQ(storage.get("temporary"), std::nullopt);
    }

    TEST_F(KVStorageTest, GetManySorted) {
        std::vector<std::tuple<std::string, std::string, uint32_t>> initial_data = {
                {"a", "val_a", 0}, {"c", "val_c", 0}, {"e", "val_e", 0}, {"b", "val_b", 0}, {"d", "val_d", 0}};
        KVStorage<MockClock> storage(initial_data, clock_);

        auto result = storage.getManySorted("b", 3);

        ASSERT_EQ(result.size(), 3);
        EXPECT_EQ(result[0], std::make_pair("b", "val_b"));
        EXPECT_EQ(result[1], std::make_pair("c", "val_c"));
        EXPECT_EQ(result[2], std::make_pair("d", "val_d"));
    }

    TEST_F(KVStorageTest, GetManySortedSkipsExpiredEntries) {
        std::vector<std::tuple<std::string, std::string, uint32_t>> initial_data = {{"a", "val_a", 0},
                                                                                    {"b", "val_b", 5}, // Will expire
                                                                                    {"c", "val_c", 0},
                                                                                    {"d", "val_d", 0}};
        KVStorage<MockClock> storage(initial_data, clock_);

        clock_.advance_time(std::chrono::seconds(6));

        auto result = storage.getManySorted("a", 3);

        ASSERT_EQ(result.size(), 3);
        EXPECT_EQ(result[0], std::make_pair("a", "val_a"));
        EXPECT_EQ(result[1], std::make_pair("c", "val_c"));
        EXPECT_EQ(result[2], std::make_pair("d", "val_d"));
    }

    TEST_F(KVStorageTest, RemoveOneExpiredEntry) {
        std::vector<std::tuple<std::string, std::string, uint32_t>> initial_data = {
                {"permanent", "value", 0}, {"expires1", "value1", 5}, {"expires2", "value2", 10}};
        KVStorage<MockClock> storage(initial_data, clock_);

        EXPECT_EQ(storage.removeOneExpiredEntry(), std::nullopt);

        clock_.advance_time(std::chrono::seconds(6));

        auto removed = storage.removeOneExpiredEntry();
        ASSERT_TRUE(removed.has_value());
        EXPECT_EQ(removed->first, "expires1");
        EXPECT_EQ(removed->second, "value1");

        EXPECT_EQ(storage.get("expires1"), std::nullopt);
        EXPECT_EQ(storage.get("expires2"), "value2");
        EXPECT_EQ(storage.get("permanent"), "value");

        clock_.advance_time(std::chrono::seconds(5));

        removed = storage.removeOneExpiredEntry();
        ASSERT_TRUE(removed.has_value());
        EXPECT_EQ(removed->first, "expires2");
        EXPECT_EQ(removed->second, "value2");

        EXPECT_EQ(storage.removeOneExpiredEntry(), std::nullopt);
    }

    TEST_F(KVStorageTest, EdgeCases) {
        std::vector<std::tuple<std::string, std::string, uint32_t>> empty_data;
        KVStorage<MockClock> storage(empty_data, clock_);

        // Empty key and value
        storage.set("", "", 0);
        EXPECT_EQ(storage.get(""), "");

        // Large strings (basic test)
        std::string large_key(1000, 'k');
        std::string large_value(1000, 'v');
        storage.set(large_key, large_value, 0);
        EXPECT_EQ(storage.get(large_key), large_value);

        // GetMany with count 0
        auto result = storage.getManySorted("", 0);
        EXPECT_TRUE(result.empty());
    }

} // namespace vk
