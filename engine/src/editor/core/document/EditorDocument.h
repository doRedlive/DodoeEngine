// do@Redlive
#pragma once
#include "dopch.h"

#include <cstdint>
#include <string>
#include <vector>

namespace cakery {

struct EditorComponent {
    String typeName;
    Json value = Json::object();
};

struct EditorEntity {
    UInt64 uuid = 0;
    UInt64 parent = 0;
    String name;
    DynamicArray<EditorComponent> nativeComponents;
    DynamicArray<EditorComponent> managedComponents;
};

struct EditorDocument {
    String name;
    DynamicArray<EditorEntity> entities;
};

} // namespace cakery
