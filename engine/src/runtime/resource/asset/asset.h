// do@Redlive

#pragma once

#include "dopch.h"

#include "runtime/core/utils/json.h"
#include "runtime/resource/file/file_id.h"
#include "runtime/core/object/object_id.h"

namespace dodoe {

    enum class AssetType : UInt16 {
        Unknown = 0,
        Texture,
        Sprite,
        Mesh,
        Material,
        Anim2DClip,
        AnimatorController,
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
        ObjectID ref{};
        AssetType type{AssetType::Unknown};
        String name{};
        FileID source_file{};
        String source_path{};
        UInt64 source_file_mtime{0};
        UInt64 asset_file_mtime{0};
        UInt64 import_signature{0};
        DynamicArray<String> tags{};
        DynamicArray<ObjectID> dependencies{};
        Bool is_builtin{false};
    };

    class Asset {
    protected:
        AssetMetaData m_meta;
        AssetLoadState m_load_state{AssetLoadState::Unloaded};
        Asset() = default;

    public:
        static constexpr AssetType kStaticType = AssetType::Unknown;

        virtual ~Asset() = default;

        [[nodiscard]] const ObjectID& getObjectID() const { return m_meta.ref; }
        [[nodiscard]] AssetType getType() const { return m_meta.type; }
        [[nodiscard]] const String& getName() const { return m_meta.name; }
        [[nodiscard]] const String& getSourcePath() const { return m_meta.source_path; }

        void setObjectID(const ObjectID& id) { m_meta.ref = id; }
        void setName(const String& name) { m_meta.name = name; }

        [[nodiscard]] const AssetMetaData& getMetaData() const { return m_meta; }
        AssetMetaData& getMetaDataMutable() { return m_meta; }
        void setMetaData(const AssetMetaData& meta) { m_meta = meta; }

        [[nodiscard]] AssetLoadState getLoadState() const { return m_load_state; }
        [[nodiscard]] Bool isLoaded() const { return m_load_state == AssetLoadState::Loaded; }
        void setLoadState(AssetLoadState state) { m_load_state = state; }

        [[nodiscard]] virtual Bool loadFromSource(const String& absolute_source_path) = 0;
        virtual void unloadRuntime() = 0;

        [[nodiscard]] virtual Bool isReadOnly() const = 0;
        [[nodiscard]] virtual Bool saveToSource(const String& absolute_path) const;

        [[nodiscard]] virtual Json serializeMeta() const;
        [[nodiscard]] virtual Bool deserializeMeta(const Json& json);

        [[nodiscard]] static const char* AssetTypeToString(AssetType type);
        [[nodiscard]] static AssetType AssetTypeFromString(const String& str);
        [[nodiscard]] static const char* AssetTypeToExtension(AssetType type);
        [[nodiscard]] static Bool AssetTypeIsReadOnly(AssetType type);
    };

} // dodoe
