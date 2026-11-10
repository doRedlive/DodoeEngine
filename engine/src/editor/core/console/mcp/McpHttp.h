// do@Redlive
#pragma once
#include "dopch.h"

#include <cstddef>
#include <cstdlib>
#include <string>

namespace cakery::mcp {

inline constexpr Size_t kMaxHeaderBytes = 64 * 1024;
inline constexpr Size_t kMaxBodyBytes = 16 * 1024 * 1024;

struct HttpBuffer {
    String data;
    Size_t offset = 0;

    void feed(const char* bytes, Size_t length) {
        data.append(bytes, length);
    }

    enum class ReadResult { Ok, NeedMore, Error };

    ReadResult readLine(String& line) {
        const Size_t end = data.find('\n', offset);
        if (end != String::npos) {
            Size_t length = end - offset;
            if (length > 0 && data[offset + length - 1] == '\r') {
                --length;
            }
            line.assign(data, offset, length);
            offset = end + 1;
            return ReadResult::Ok;
        }
        if (data.size() - offset > kMaxHeaderBytes) {
            return ReadResult::Error;
        }
        return ReadResult::NeedMore;
    }

    ReadResult readBody(Size_t length, String& body) {
        if (length > kMaxBodyBytes) {
            return ReadResult::Error;
        }
        if (data.size() - offset < length) {
            return ReadResult::NeedMore;
        }
        body.assign(data, offset, length);
        offset += length;
        return ReadResult::Ok;
    }
};

struct HttpRequestHead {
    String method;
    String target;
    String session_id;
    Size_t content_length = 0;
    bool valid = false;
};

inline String ToLowerAscii(String text) {
    for (char& c : text) {
        if (c >= 'A' && c <= 'Z') {
            c = static_cast<char>(c - 'A' + 'a');
        }
    }
    return text;
}

template <typename Feed>
inline bool ParseRequestHead(HttpBuffer& buffer, HttpRequestHead& head, Feed&& feed) {
    head = HttpRequestHead{};
    String line;
    bool haveRequestLine = false;
    while (true) {
        const HttpBuffer::ReadResult result = buffer.readLine(line);
        if (result == HttpBuffer::ReadResult::Error) {
            return false;
        }
        if (result == HttpBuffer::ReadResult::NeedMore) {
            if (!feed(buffer)) {
                return false;
            }
            continue;
        }
        if (line.empty()) {
            return haveRequestLine && head.valid;
        }
        if (!haveRequestLine) {
            haveRequestLine = true;
            const Size_t first = line.find(' ');
            const Size_t second =
                first == String::npos ? String::npos : line.find(' ', first + 1);
            if (first == String::npos || second == String::npos) {
                head.valid = false;
                continue;
            }
            head.method = line.substr(0, first);
            head.target = line.substr(first + 1, second - first - 1);
            const auto query = head.target.find('?');
            if (query != String::npos) {
                head.target = head.target.substr(0, query);
            }
            head.valid = !head.method.empty() && !head.target.empty();
            continue;
        }
        const auto colon = line.find(':');
        if (colon == String::npos) {
            continue;
        }
        const String name = ToLowerAscii(line.substr(0, colon));
        Size_t valueStart = colon + 1;
        while (valueStart < line.size() && (line[valueStart] == ' ' || line[valueStart] == '\t')) {
            ++valueStart;
        }
        const String value = line.substr(valueStart);
        if (name == "content-length") {
            head.content_length =
                static_cast<Size_t>(std::strtoull(value.c_str(), nullptr, 10));
        } else if (name == "mcp-session-id") {
            head.session_id = value;
        }
    }
}

template <typename Feed>
inline bool ReadBody(HttpBuffer& buffer, Size_t length, String& body, Feed&& feed) {
    if (length > kMaxBodyBytes) {
        return false;
    }
    while (true) {
        const HttpBuffer::ReadResult result = buffer.readBody(length, body);
        if (result == HttpBuffer::ReadResult::Ok) {
            return true;
        }
        if (result == HttpBuffer::ReadResult::Error) {
            return false;
        }
        if (!feed(buffer)) {
            return false;
        }
    }
}

} // namespace cakery::mcp
