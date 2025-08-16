#pragma once

#include <cstdint>
#include <optional>
#include <span>
#include <string>
#include <tuple>
#include <vector>

#include <boost/multi_index/hashed_index.hpp>
#include <boost/multi_index/member.hpp>
#include <boost/multi_index/ordered_index.hpp>
#include <boost/multi_index_container.hpp>
#include <boost/smart_ptr/detail/atomic_count.hpp>
#include <boost/smart_ptr/intrusive_ptr.hpp>
#include <boost/smart_ptr/intrusive_ref_counter.hpp>

#include <utility.hpp>

using namespace boost::multi_index;

namespace vk {
    struct Entry : boost::intrusive_ref_counter<Entry> {
        std::string key;
        std::string value;
        uint64_t expiration_time;

        Entry(std::string k, std::string v, uint64_t exp_time) :
            key(std::move(k)), value(std::move(v)), expiration_time(exp_time) {}
    };

    template<typename Clock>
    class KVStorage {
    private:
        using EntryPtr = boost::intrusive_ptr<Entry>;
        using Container = multi_index_container<
                EntryPtr, indexed_by<hashed_unique<tag<struct by_key>, member<Entry, std::string, &Entry::key>,
                                                   KVStorageHash, KVStorageEqual>,
                                     ordered_unique<tag<struct by_key_sorted>, member<Entry, std::string, &Entry::key>,
                                                    KVStorageLess>,
                                     ordered_non_unique<tag<struct by_expiration>,
                                                        member<Entry, uint64_t, &Entry::expiration_time>>>>;

    private:
        Container entries_;
        Clock clock_;

    public:
        // Инициализирует хранилище переданным множеством записей. Размер span может быть очень большим.
        // Также принимает абстракцию часов (Clock) для возможности управления временем в тестах.
        explicit KVStorage(std::span<std::tuple<std::string /*key*/, std::string /*value*/, uint32_t /*ttl*/>> entries,
                           Clock clock = Clock()) : clock_(std::move(clock)) {
            uint64_t current_time =
                    std::chrono::duration_cast<std::chrono::seconds>(clock_.now().time_since_epoch()).count();

            for (const auto &[key, value, ttl]: entries) {
                uint64_t expiration_time = ttl ? current_time + ttl : 0;

                entries_.emplace(EntryPtr(new Entry(key, value, expiration_time)));
            }
        }

        ~KVStorage() = default;

        // Присваивает по ключу key значение value.
        // Если ttl == 0, то время жизни записи - бесконечность, иначе запись должна перестать быть доступной через ttl
        // секунд. Безусловно обновляет ttl записи.
        void set(std::string key, std::string value, uint32_t ttl) {
            uint64_t current_time =
                    std::chrono::duration_cast<std::chrono::seconds>(clock_.now().time_since_epoch()).count();
            uint64_t expiration_time = ttl ? current_time + ttl : 0;

            auto &key_index = entries_.get<by_key>();

            if (auto it = key_index.find(key); it != key_index.end()) {
                entries_.modify(it, [&value, expiration_time](EntryPtr &p) {
                    p->value = std::move(value);
                    p->expiration_time = expiration_time;
                });
            } else {
                entries_.emplace(EntryPtr(new Entry(std::move(key), std::move(value), expiration_time)));
            }
        }

        // Удаляет запись по ключу key.
        // Возвращает true, если запись была удалена. Если ключа не было до удаления, то вернет false.
        bool remove(std::string_view key) {
            auto &key_index = entries_.get<by_key>();

            if (auto it = key_index.find(key); it != key_index.end()) {
                key_index.erase(it);
                return true;
            }

            return false;
        }

        // Получает значение по ключу key. Если данного ключа нет, то вернет std::nullopt.
        std::optional<std::string> get(std::string_view key) const {
            auto &key_index = entries_.get<by_key>();

            if (auto it = key_index.find(key); it != key_index.end()) {
                const auto &entry = *it;

                if (not isExpired(entry)) {
                    return entry->value;
                }
            }

            return std::nullopt;
        }

        // Возвращает следующие count записей начиная с key в порядке лексикографической сортировки ключей.
        // Пример: ("a", "val1"), ("b", "val2"), ("d", "val3"), ("e", "val4")
        // getManySorted("c", 2) -> ("d", "val3"), ("e", "val4")
        std::vector<std::pair<std::string, std::string>> getManySorted(std::string_view key, uint32_t count) const {
            std::vector<std::pair<std::string, std::string>> result;
            result.reserve(count);

            auto &sorted_index = entries_.get<by_key_sorted>();

            auto it = sorted_index.lower_bound(key);
            while (result.size() < count && it != sorted_index.end()) {
                const auto &entry = *it;

                if (not isExpired(entry)) {
                    result.emplace_back(entry->key, entry->value);
                }

                ++it;
            }

            return result;
        }

        // Удаляет протухшую запись из структуры и возвращает ее. Если удалять нечего, то вернет std::nullopt.
        // Если на момент вызова метода протухло несколько записей, то можно удалить любую.
        std::optional<std::pair<std::string, std::string>> removeOneExpiredEntry() {
            uint64_t current_time =
                    std::chrono::duration_cast<std::chrono::seconds>(clock_.now().time_since_epoch()).count();

            auto &exp_index = entries_.get<by_expiration>();

            for (auto it = exp_index.upper_bound(0); it != exp_index.end(); ++it) {
                const auto &entry = *it;

                if (isExpired(entry)) {
                    std::string key = entry->key;
                    std::string value = entry->value;

                    exp_index.erase(it);

                    return std::make_pair(std::move(key), std::move(value));
                }
            }

            return std::nullopt;
        }

    private:
        bool isExpired(const EntryPtr &entryPtr) const {
            if (entryPtr->expiration_time == 0) {
                return false;
            }

            uint64_t current_time =
                    std::chrono::duration_cast<std::chrono::seconds>(clock_.now().time_since_epoch()).count();

            return current_time >= entryPtr->expiration_time;
        }
    };

} // namespace vk
