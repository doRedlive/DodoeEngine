// do@Redlive

#pragma once

#include "dopch.h"

#include "runtime/core/object/object.h"
#include "runtime/core/utils/util.h"

namespace dodoe {

    class DODOE_API Tileset : public Object {
    public:
        static constexpr UInt32 kLocalId = 0;

        String name{};
        UInt32 first_gid{1};
        UInt32 tile_width{16};
        UInt32 tile_height{16};
        UInt32 columns{0};
        UInt32 tile_count{0};
        String image_path{};
        Identifier texture_id{0};

        Tileset() = default;
        explicit Tileset(const ObjectID& id)
            : Object(id) {}

        [[nodiscard]] const char* getObjectTypeName() const override { return "Tileset"; }

        void clear() {
            name.clear();
            first_gid = 1;
            tile_width = 16;
            tile_height = 16;
            columns = 0;
            tile_count = 0;
            image_path.clear();
            texture_id = 0;
        }

        [[nodiscard]] Bool loadFromJson(const String& absolute_path);
        [[nodiscard]] Bool saveToJson(const String& absolute_path) const;

        [[nodiscard]] static Tileset* Create(const ObjectID& id, const String& path);
        [[nodiscard]] static Tileset* CreateTransient();
        static void Shutdown();
    };

} // dodoe
