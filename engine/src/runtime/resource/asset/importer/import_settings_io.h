// do@Redlive

#pragma once

#include "dopch.h"

#include "runtime/core/utils/json.h"
#include "runtime/core/utils/uuid.h"
#include "runtime/resource/file/file_id.h"

namespace dodoe {

    struct ImportSettings {
        UUID guid{};
        String importer{};
        Json settings{};
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

        static UUID MakeDeterministicGuid(const String& source_path);

        static UInt64 LastWriteTimeSeconds(const FsPath& path);
    };

} // dodoe
