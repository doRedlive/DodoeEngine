// do@Redlive

#include "asset_importer.h"

#include "runtime/core/utils/common.h"
#include "runtime/resource/asset/importer/import_settings_io.h"

namespace dodoe {

    Bool AssetImporter::isDirty(const ImportContext& ctx) const {
        if (!ctx.cached_meta) {
            return true;
        }
        if (ImportSettingsIO::LastWriteTimeSeconds(FsPath(ctx.absolute_source_path.c_str())) != ctx.cached_meta->source_file_mtime) {
            return true;
        }
        return ComputeImportSignature(ctx.settings) != ctx.cached_meta->import_signature;
    }

    ImporterRegistry& ImporterRegistry::Self() {
        static ImporterRegistry s_instance;
        return s_instance;
    }

    void ImporterRegistry::registerImporter(const String& ext, Scope<AssetImporter> importer) {
        if (!importer) {
            return;
        }
        AssetImporter* raw = importer.get();
        m_importers_by_name[String(raw->getName())] = raw;
        if (!ext.empty()) {
            m_importers[ext] = std::move(importer);
        } else {
            m_importers[String("")] = std::move(importer);
        }
    }

    AssetImporter* ImporterRegistry::find(const String& ext) const {
        auto it = m_importers.find(ext);
        if (it != m_importers.end()) {
            return it->second.get();
        }
        return nullptr;
    }

    AssetImporter* ImporterRegistry::findByName(const String& name) const {
        auto it = m_importers_by_name.find(name);
        if (it != m_importers_by_name.end()) {
            return it->second;
        }
        return nullptr;
    }

    UInt64 ComputeImportSignature(const Json& settings) {
        String text(settings.dump().c_str());
        return string2hash(text);
    }

} // dodoe
