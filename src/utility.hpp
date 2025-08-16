#pragma once

#include <string>

namespace vk {
    struct KVStorageHash {
        using is_transparent = void;

        std::size_t operator()(std::string_view sv) const { return std::hash<std::string_view>{}(sv); }

        std::size_t operator()(const std::string &s) const { return std::hash<std::string>{}(s); }
    };

    struct KVStorageEqual {
        using is_transparent = void;

        bool operator()(std::string_view lhs, std::string_view rhs) const { return lhs == rhs; }
    };

    struct KVStorageLess {
        using is_transparent = void;

        bool operator()(std::string_view a, std::string_view b) const noexcept { return a < b; }
    };

} // namespace vk
