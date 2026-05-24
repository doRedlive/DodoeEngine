// do@Redlive

#include "uuid.h"

#include <random>

namespace dodoe {

    static std::random_device random_device;
    static std::mt19937_64 engine(random_device());
    static std::uniform_int_distribution<uint64_t> distribution;

    UUID::UUID() : m_uuid(distribution(engine)) {

    }

    UUID::UUID(const uint64_t value) : m_uuid(value) {

    }

    UUID::operator uint64_t() const {
        return m_uuid;
    }
} // dodoe