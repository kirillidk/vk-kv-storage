# vk-kv-storage

Высокопроизводительная реализация Key-Value хранилища, оптимизированная для рабочих нагрузок с преобладанием операций чтения и поддержкой TTL.

## Обзор

Данный проект представляет собой реализацию in-memory движка Key-Value хранилища, предназначенного для сценариев, где операции чтения значительно превышают операции записи (95% чтений). Хранилище поддерживает:

- Отображение строки в строку
- Функциональность TTL (time-to-live) для автоматического истечения срока действия записей
- Эффективные диапазонные запросы с лексикографическим упорядочиванием
- Экономное использование памяти с intrusive reference counting

## Возможности

- **Высокопроизводительное чтение**: Оптимизировано для нагрузок с преобладанием чтения
- **Поддержка TTL**: Автоматическое истечение записей с настраиваемым временем жизни
- **Диапазонные запросы**: Получение записей в лексикографическом порядке
- **Экономное использование памяти**: Использует intrusive указатели и boost::multi_index для оптимального использования памяти
- **Абстракция времени**: Тестируемое управление временем через dependency injection

## Архитектура

Хранилище использует `boost::multi_index_container` с тремя индексами:
1. **Хеш-индекс по ключу**
2. **Упорядоченный индекс по ключу**
3. **Упорядоченный индекс по времени истечения**
   
Записи управляются через `boost::intrusive_ptr` для эффективного управления памятью и автоматической очистки.

## Справочник API

### Конструктор
```cpp
explicit KVStorage(std::span<std::tuple<std::string, std::string, uint32_t>> entries, Clock clock = Clock())
```
Инициализирует хранилище со span записей. Каждая запись содержит ключ, значение и TTL.

### Основные операции

#### `void set(std::string key, std::string value, uint32_t ttl)`
Устанавливает пару ключ-значение с опциональным TTL.
- `ttl = 0`: Запись никогда не истекает
- `ttl > 0`: Запись истекает через указанное количество секунд

#### `std::optional<std::string> get(std::string_view key) const`
Получает значение по ключу. Возвращает `std::nullopt`, если ключ не существует или истек.

#### `bool remove(std::string_view key)`
Удаляет запись по ключу. Возвращает `true`, если запись была удалена, `false`, если ключ не существовал.

#### `std::vector<std::pair<std::string, std::string>> getManySorted(std::string_view key, uint32_t count) const`
Возвращает до `count` записей, начиная с данного ключа в лексикографическом порядке.

#### `std::optional<std::pair<std::string, std::string>> removeOneExpiredEntry()`
Удаляет и возвращает одну истекшую запись. Возвращает `std::nullopt`, если истекших записей нет.

## Временная сложность

| Операция | Сложность |
|----------|-----------|
| `set()` | O(log n) |
| `get()` | O(1) |
| `remove()` | O(log n) |
| `getManySorted()` | O(log n + count) |
| `removeOneExpiredEntry()` | O(log n) |

Где:
- `n` = общее количество записей

## Накладные расходы памяти

**Накладные расходы на запись**: ~80 байт

```cpp
// Условная структура узла multi_index для одной записи
struct MultiIndexNode {
    // Для hashed_index
    struct hash_node;              // 16 байт (2 указателя)

    // Для ordered_index by key
    struct {
        void* left;           // 8 байт
        void* right;          // 8 байт  
        void* parent;         // 8 байт
    } ordered_node_key;       // = 24 байта

    // Для ordered_index by expiration
    struct {
        void* left;           // 8 байт
        void* right;          // 8 байт
        void* parent;         // 8 байт
    } ordered_node_exp;       // = 24 байта

    boost::intrusive_ptr<Entry> entry_ptr;  // 8 байт
};
// Итого узел: 16 + 24 + 24 + 8 = 72 байта
```

**Итого overhead на запись**: 72 (узел) + 8 (счетчик для указателя) = **80 байт**  

## Требования для сборки

- Компилятор с поддержкой C++20
- CMake 3.17+
- Conan 2.0+

## Сборка

1. Установка зависимостей:
```bash
conan install . --output-folder=build --build=missing
```

2. Конфигурация и сборка:
```bash
cd build
cmake .. \
  -DCMAKE_TOOLCHAIN_FILE=conan_toolchain.cmake \
  -DCMAKE_BUILD_TYPE=Release
cmake --build .
```

3. Запуск тестов:
```bash
ctest
```

## Пример использования

```cpp
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
```

## Проектные решения

1. **boost::multi_index_container**: Обеспечивает несколько эффективных паттернов доступа в одном контейнере
2. **boost::intrusive_ptr**: Снижает накладные расходы памяти по сравнению с std::shared_ptr
3. **Прозрачные hash/compare функторы**: Позволяют эффективные поиски с string_view без создания временных строк
4. **Абстракция Clock**: Обеспечивает детерминистическое тестирование и гибкость в источниках времени
