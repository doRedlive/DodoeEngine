// do@Redlive

#include "uuid.h"

#include <charconv>
#include <random>
#include <system_error>

namespace dodoe {

    static std::random_device random_device;
    static std::mt19937_64 engine(random_device());
    static std::uniform_int_distribution<uint64_t> distribution;

    UUID::UUID() : m_uuid(distribution(engine)) {

    }

    UUID::UUID(const uint64_t value) : m_uuid(value) {

    }

    UUID UUID::Generate() {
        return UUID();
    }

    UUID UUID::FromString(const String& str) {
        int base = 10;
        const char* first = str.data();
        const char* last = first + str.size();
        if (str.size() > 2 && str[0] == '0' && (str[1] == 'x' || str[1] == 'X')) {
            base = 16;
            first += 2;
        }
        unsigned long long value = 0;
        const auto res = std::from_chars(first, last, value, base);
        if (res.ec != std::errc{} || res.ptr != last) {
            return UUID(0);
        }
        return UUID(static_cast<uint64_t>(value));
    }

    UUID::operator uint64_t() const {
        return m_uuid;
    }
} // dodoe