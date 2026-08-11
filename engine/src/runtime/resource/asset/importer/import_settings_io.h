// do@Redlive

#pragma once

#include "dopch.h"

#include "runtime/core/utils/json.h"
#include "runtime/core/utils/uuid.h"
#include "runtime/resource/file/file_id.h"

namespace dodoe {

    struct SpriteMeta {
        String name{};
        UInt32 local_id{0};
        Float ppu{10.0f};
        Float pivot_x{0.5f};
        Float pivot_y{0.5f};
        Float slice_left{0.0f};
        Float slice_bottom{0.0f};
        Float slice_right{0.0f};
        Float slice_top{0.0f};
    };

    struct ImportSettings {
        UUID guid{};
        String importer{};
        Json settings{};
        DynamicArray<SpriteMeta> sprites{};
    };

    class ImportSettingsIO {
    public:
        static constexpr const char* kMetaSuffix = ".meta";

        static Bool Load(const FsPath& absolute_source_path, ImportSettings& out);

        static Bool Save(const FsPath& absolute_source_path, const ImportSettings& settings);

        static ImportSettings LoadOrCreate(const FsPath& absolute_source_path,
                                           const String& source_path,
                                           const String& default_importer,
                                           const Json& default_settings);

        static UInt64 LastWriteTimeSeconds(const FsPath& path);
    };

} // dodoe
