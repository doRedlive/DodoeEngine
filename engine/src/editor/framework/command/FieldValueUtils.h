#pragma once

#include "runtime/core/utils/json.h"

namespace dodoe {
    class FieldAccessor;
    struct ObjectID;
}

namespace cakery {

bool IsIntegerJson(const dodoe::Json& value);
dodoe::Json CaptureFieldValue(const char* typeName, void* value);
bool ApplyFieldValue(const char* typeName, dodoe::FieldAccessor& field, void* objPtr, const dodoe::Json& value);

} // namespace cakery
