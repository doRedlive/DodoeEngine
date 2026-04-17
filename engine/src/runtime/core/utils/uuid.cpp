//
// Created by GreenMuffin on 2026/1/22.
//

#include "uuid.h"

#include <random>

namespace dodoe {

    static std::random_device random_device;
    static std::mt19937_64 engine(random_device());
    static std::uniform_int_distribution<uint64_t> distribution;

    UUID::UUID() : uuid_(distribution(engine)) {

    }

    UUID::UUID(const uint64_t value) : uuid_(value) {

    }

    UUID::operator uint64_t() const {
        return uuid_;
    }
} // dodoe