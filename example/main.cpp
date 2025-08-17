#include <chrono>
#include <iostream>

#include "kv_storage.hpp"

using Clock = std::chrono::steady_clock;

int main() {
    // Инициализация с начальными данными
    std::vector<std::tuple<std::string, std::string, uint32_t>> initial_data = {
            {"key1", "value1", 0}, // Никогда не истекает
            {"key2", "value2", 3600}, // Истекает через 1 час
            {"key3", "value3", 60} // Истекает через 1 минуту
    };

    vk::KVStorage<Clock> storage(initial_data);

    // Установка новой пары ключ-значение
    storage.set("new_key", "new_value", 300); // Истекает через 5 минут

    // Получение значения
    auto value = storage.get("key1");
    if (value) {
        std::cout << "Найдено: " << *value << std::endl;
    }

    // Получение диапазона ключей
    auto range = storage.getManySorted("key", 10);
    for (const auto &[key, val]: range) {
        std::cout << key << " = " << val << std::endl;
    }

    // Удаление истекших записей
    while (auto expired = storage.removeOneExpiredEntry()) {
        std::cout << "Удалена истекшая запись: " << expired->first << std::endl;
    }
}
