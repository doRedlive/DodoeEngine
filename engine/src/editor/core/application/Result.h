// do@Redlive
#pragma once
#include "dopch.h"

namespace cakery {

struct Result {
    bool ok = true;
    String message;
    String detail;

    static Result success() { return Result{}; }

    static Result failure(const String& message, const String& detail = {}) {
        Result r;
        r.ok = false;
        r.message = message;
        r.detail = detail;
        return r;
    }
};

} // namespace cakery
