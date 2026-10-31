#pragma once

#include <memory>
#include <string_view>
#include <vector>

#include "scale/Driver.hpp"

namespace scale {

/** Factory that picks a driver for an advertising scale. First match wins. */
class DriverRegistry {
public:
    struct Entry {
        std::string_view name;
        bool (*matches)(const Advertisement&);
        std::unique_ptr<Driver> (*create)();
    };

    template <class D>
    void add()
    {
        entries_.push_back({D::kName, &D::matches, [] () -> std::unique_ptr<Driver> { return std::make_unique<D>(); }});
    }

    const Entry* find(const Advertisement& advertisement) const
    {
        for (const auto& entry : entries_) {
            if (entry.matches(advertisement)) {
                return &entry;
            }
        }
        return nullptr;
    }

    const std::vector<Entry>& entries() const { return entries_; }

private:
    std::vector<Entry> entries_;
};

} // namespace scale
