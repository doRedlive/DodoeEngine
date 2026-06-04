// do@Redlive

#pragma once

#include "dopch.h"

#include "runtime/core/utils/json.h"
#include "runtime/resource/file/file_id.h"

namespace dodoe {

    enum class AssetType : UInt16 {
        Unknown = 0,
        Texture,
        Mesh,
        Material,
        AnimationClip,
        Scene,
        Shader,
        Script,
        Tileset,
        Prefab,
        Audio,
        Count
    };

    enum class AssetLoadState : UInt8 {
        Unloaded,
        Loading,
        Loaded,
        Failed,
        Reloading
    };

    struct AssetMetaData {
        FileID file_id{};
        AssetType type{AssetType::Unknown};
        String name{};
        String source_path{};
        String asset_path{};
        UInt64 source_file_mtime{0};
        UInt64 asset_file_mtime{0};
        UnorderedMap<String, String> import_settings{};
        DynamicArray<String> tags{};
        DynamicArray<FileID> dependencies{};
        Bool is_builtin{false};
    };

    class Asset {
    protected:
        AssetMetaData m_meta;
        AssetLoadState m_load_state{AssetLoadState::Unloaded};
        std::atomic<UInt32> m_ref_count{0};
        Asset() = default;

    public:
        static constexpr AssetType kStaticType = AssetType::Unknown;

        virtual ~Asset() = default;

        [[nodiscard]] const FileID& getFileID() const { return m_meta.file_id; }
        [[nodiscard]] AssetType getType() const { return m_meta.type; }
        [[nodiscard]] const String& getName() const { return m_meta.name; }
        [[nodiscard]] const String& getSourcePath() const { return m_meta.source_path; }

        void setFileID(const FileID& file_id) { m_meta.file_id = file_id; }
        void setName(const String& name) { m_meta.name = name; }

        [[nodiscard]] const AssetMetaData& getMetaData() const { return m_meta; }
        AssetMetaData& getMetaDataMutable() { return m_meta; }
        void setMetaData(const AssetMetaData& meta) { m_meta = meta; }

        [[nodiscard]] AssetLoadState getLoadState() const { return m_load_state; }
        [[nodiscard]] Bool isLoaded() const { return m_load_state == AssetLoadState::Loaded; }
        void setLoadState(AssetLoadState state) { m_load_state = state; }

        void addRef() { m_ref_count.fetch_add(1, std::memory_order_relaxed); }
        void releaseRef() { m_ref_count.fetch_sub(1, std::memory_order_relaxed); }
        [[nodiscard]] UInt32 getRefCount() const { return m_ref_count.load(std::memory_order_relaxed); }

        [[nodiscard]] virtual Bool loadFromSource(const String& absolute_source_path) = 0;
        virtual void unloadRuntime() = 0;

        [[nodiscard]] virtual Bool isReadOnly() const = 0;
        [[nodiscard]] virtual Bool saveToSource(const String& absolute_path) const;

        [[nodiscard]] virtual Json serializeMeta() const;
        [[nodiscard]] virtual Bool deserializeMeta(const Json& json);

        [[nodiscard]] static const char* assetTypeToString(AssetType type);
        [[nodiscard]] static AssetType assetTypeFromString(const String& str);
        [[nodiscard]] static const char* assetTypeToExtension(AssetType type);
        [[nodiscard]] static Bool assetTypeIsReadOnly(AssetType type);
    };

} // dodoe
