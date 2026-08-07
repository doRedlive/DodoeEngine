// do@Redlive

#pragma once

#include "dopch.h"

#include "runtime/core/utils/json.h"
#include "runtime/resource/asset/asset.h"
#include "runtime/resource/file/file_id.h"

namespace dodoe {

    struct ImportContext {
        const FileID& source_file;
        const String& source_path;
        const String& absolute_source_path;
        const Json& settings;
        const AssetMetaData* cached_meta;
    };

    class AssetImporter {
    public:
        virtual ~AssetImporter() = default;

        virtual const char* getName() const = 0;

        virtual Json getDefaultSettings() const { return Json::object(); }

        virtual Scope<Asset> import(const ImportContext& ctx) = 0;

        virtual Bool isDirty(const ImportContext& ctx) const;
    };

    class ImporterRegistry {
        UnorderedMap<String, Scope<AssetImporter>> m_importers;
        UnorderedMap<String, AssetImporter*> m_importers_by_name;

    public:
        static ImporterRegistry& Self();

        void registerImporter(const String& ext, Scope<AssetImporter> importer);

        AssetImporter* find(const String& ext) const;

        AssetImporter* findByName(const String& name) const;
    };

    UInt64 ComputeImportSignature(const Json& settings);

} // dodoe
