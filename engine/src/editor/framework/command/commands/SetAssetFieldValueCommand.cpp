#include "SetAssetFieldValueCommand.h"
#include "framework/EditorContext.h"
#include "framework/asset/AssetDatabase.h"
#include "framework/command/FieldValueUtils.h"

#include "runtime/core/meta/reflection/reflection.h"
#include "runtime/resource/resource_manager.h"
#include "runtime/resource/asset/asset_manager.h"
#include "runtime/resource/asset/asset.h"
#include "runtime/function/log/log_system.h"

#include <cstring>

namespace cakery {

namespace {

const char* ReflectionTypeNameForAsset(dodoe::AssetType type)
{
    switch (type) {
        case dodoe::AssetType::Material: return "MaterialAsset";
        case dodoe::AssetType::Tileset: return "TilesetAsset";
        case dodoe::AssetType::Anim2DClip: return "Anim2DClipAsset";
        default: return nullptr;
    }
}

dodoe::Asset* FindLoadedAsset(dodoe::UUID id)
{
    auto* am = dodoe::ResourceManager::Self().getAssetManager();
    if (!am) return nullptr;
    return am->findAsset(id);
}

} // namespace

SetAssetFieldValueCommand::SetAssetFieldValueCommand(dodoe::UUID asset, std::string field,
                                                     dodoe::Json oldVal, dodoe::Json newVal)
    : m_asset(asset)
    , m_field(std::move(field))
    , m_old(std::move(oldVal))
    , m_new(std::move(newVal))
{}

bool SetAssetFieldValueCommand::execute(EditorContext& ctx)
{
    dodoe::Asset* asset = FindLoadedAsset(m_asset);
    if (!asset) return false;

    const char* typeName = ReflectionTypeNameForAsset(asset->getType());
    if (!typeName) return false;

    dodoe::TypeMeta meta = dodoe::TypeMeta::newMetaFromName(dodoe::String(typeName));
    if (!meta.isValid()) return false;

    dodoe::FieldAccessor field = meta.get_field_by_name(m_field.c_str());
    const char* fieldTypeName = field.getFieldTypeName();
    if (!fieldTypeName || !fieldTypeName[0] || std::strcmp(fieldTypeName, "unknownType") == 0) return false;

    if (m_old.is_null()) {
        m_old = CaptureFieldValue(fieldTypeName, field.get(asset));
    }

    bool applied = ApplyFieldValue(fieldTypeName, field, asset, m_new);

    ctx.assets().markDirty(m_asset);
    return applied;
}

void SetAssetFieldValueCommand::undo(EditorContext& ctx)
{
    dodoe::Asset* asset = FindLoadedAsset(m_asset);
    if (!asset) return;

    const char* typeName = ReflectionTypeNameForAsset(asset->getType());
    if (!typeName) return;

    dodoe::TypeMeta meta = dodoe::TypeMeta::newMetaFromName(dodoe::String(typeName));
    if (!meta.isValid()) return;

    dodoe::FieldAccessor field = meta.get_field_by_name(m_field.c_str());
    const char* fieldTypeName = field.getFieldTypeName();
    if (!fieldTypeName || !fieldTypeName[0] || std::strcmp(fieldTypeName, "unknownType") == 0) return;

    ApplyFieldValue(fieldTypeName, field, asset, m_old);

    ctx.assets().markDirty(m_asset);
}

std::string SetAssetFieldValueCommand::label() const
{
    return "Modify asset." + m_field;
}

bool SetAssetFieldValueCommand::mergeWith(const ICommand& next)
{
    auto* n = dynamic_cast<const SetAssetFieldValueCommand*>(&next);
    if (!n || n->m_asset != m_asset || n->m_field != m_field) {
        return false;
    }
    m_new = n->m_new;
    return true;
}

} // namespace cakery
