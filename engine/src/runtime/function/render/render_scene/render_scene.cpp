// do@Redlive

#include "render_scene.h"
#include "runtime/core/math/math.h"

namespace dodoe {

    Bool RenderScene::initialize(const RenderSceneCreateInfo& info) {
        (void)info;
        reset();
        return true;
    }

    void RenderScene::shutdown() {
        reset();
    }

    void RenderScene::reset() {
        m_mesh_bounds_cache.clear();
        m_primitive_objects.clear();
        m_sprite_objects.clear();
        m_scene_data_dirty = true;
        m_primitive_scene_info_indices.clear();
        m_light_scene_info_indices.clear();
        m_sprite_scene_info_indices.clear();
        m_pending_primitive_updates.clear();
        m_pending_sprite_updates.clear();
        m_primitive_scene_infos.clear();
        m_light_scene_infos.clear();
        m_sprite_scene_infos.clear();
    }

    void RenderScene::addPrimitive(Scope<PrimitiveRenderObject> primitive) {
        DO_ASSERT(primitive != nullptr, "RenderScene::addPrimitive requires valid primitive");

        const UUID id = primitive->getUUID();
        PrimitiveUpdateType update_type = PrimitiveUpdateType::Added;
        const auto existing_it = m_primitive_objects.find(id);
        if (existing_it != m_primitive_objects.end()) {
            update_type = PrimitiveUpdateType::None;
            const RenderObjectDirtyFlags dirty_flags = primitive->diff(*existing_it->second);
            if (HasAnyFlags(dirty_flags, RenderObjectDirtyFlags::Mesh)) {
                update_type |= PrimitiveUpdateType::MeshChanged;
            }
            if (HasAnyFlags(dirty_flags, RenderObjectDirtyFlags::Materials)) {
                update_type |= PrimitiveUpdateType::MaterialChanged;
            }
            if (HasAnyFlags(dirty_flags, RenderObjectDirtyFlags::State)) {
                update_type |= PrimitiveUpdateType::StateChanged;
            }
            if (HasAnyFlags(dirty_flags, RenderObjectDirtyFlags::ProxyData)) {
                update_type |= PrimitiveUpdateType::ProxyChanged;
            }
            if (existing_it->second->getWorldTransform() != primitive->getWorldTransform()) {
                update_type |= PrimitiveUpdateType::TransformChanged;
            }
        }

        m_primitive_objects[id] = std::move(primitive);
        if (update_type != PrimitiveUpdateType::None) {
            markPrimitiveDirty(id, update_type);
        }
    }

    void RenderScene::updatePrimitiveTransform(const UUID id, const Matrix4f& world_transform) {
        const auto it = m_primitive_objects.find(id);
        if (it == m_primitive_objects.end()) {
            return;
        }

        if (it->second->getWorldTransform() == world_transform) {
            return;
        }

        it->second->setWorldTransform(world_transform);
        markPrimitiveDirty(id, PrimitiveUpdateType::TransformChanged);
    }

    void RenderScene::removePrimitive(const UUID id) {
        m_primitive_objects.erase(id);
        markPrimitiveDirty(id, PrimitiveUpdateType::Removed);
    }

    void RenderScene::addLightSceneInfo(LightSceneInfo&& info) {
        const UUID id = static_cast<UUID>(static_cast<uint64_t>(info.getId()));
        const auto it = m_light_scene_info_indices.find(id);
        if (it != m_light_scene_info_indices.end()) {
            m_light_scene_infos[it->second] = std::move(info);
            return;
        }
        m_light_scene_info_indices[id] = m_light_scene_infos.size();
        m_light_scene_infos.push_back(std::move(info));
    }

    void RenderScene::updateLightSceneInfoTransform(const UUID id, const Matrix4f& world_transform) {
        const auto it = m_light_scene_info_indices.find(id);
        if (it != m_light_scene_info_indices.end()) {
            m_light_scene_infos[it->second].setWorldTransform(world_transform);
        }
    }

    void RenderScene::removeLightSceneInfo(const UUID id) {
        const auto it = m_light_scene_info_indices.find(id);
        if (it == m_light_scene_info_indices.end()) {
            return;
        }
        const Size_t remove_index = it->second;
        const Size_t last_index = m_light_scene_infos.size() - 1;
        if (remove_index != last_index) {
            LightSceneInfo& moved = m_light_scene_infos[last_index];
            const UUID moved_uuid = static_cast<UUID>(static_cast<uint64_t>(moved.getId()));
            m_light_scene_infos[remove_index] = std::move(moved);
            m_light_scene_info_indices[moved_uuid] = remove_index;
        }
        m_light_scene_infos.pop_back();
        m_light_scene_info_indices.erase(it);
    }

    const LightSceneInfo* RenderScene::findLightSceneInfo(const UUID id) const {
        const auto it = m_light_scene_info_indices.find(id);
        return it != m_light_scene_info_indices.end() ? &m_light_scene_infos[it->second] : nullptr;
    }

    void RenderScene::addSprite(Scope<SpriteRenderObject> sprite) {
        DO_ASSERT(sprite != nullptr, "RenderScene::addSprite requires valid sprite");

        const UUID id = sprite->getUUID();
        SpriteUpdateType update_type = SpriteUpdateType::Added;
        const auto existing_it = m_sprite_objects.find(id);
        if (existing_it != m_sprite_objects.end()) {
            update_type = SpriteUpdateType::None;
            const RenderObjectDirtyFlags dirty_flags = sprite->diff(*existing_it->second);
            if (HasAnyFlags(dirty_flags, RenderObjectDirtyFlags::Mesh)) {
                update_type |= SpriteUpdateType::TextureChanged;
            }
            if (HasAnyFlags(dirty_flags, RenderObjectDirtyFlags::Materials)) {
                update_type |= SpriteUpdateType::MaterialChanged;
            }
            if (HasAnyFlags(dirty_flags, RenderObjectDirtyFlags::State)) {
                update_type |= SpriteUpdateType::StateChanged;
            }
            if (existing_it->second->getWorldTransform() != sprite->getWorldTransform()) {
                update_type |= SpriteUpdateType::TransformChanged;
            }
        }

        m_sprite_objects[id] = std::move(sprite);
        if (update_type != SpriteUpdateType::None) {
            markSpriteDirty(id, update_type);
        }
    }

    void RenderScene::updateSpriteTransform(const UUID id, const Matrix4f& world_transform) {
        const auto it = m_sprite_objects.find(id);
        if (it == m_sprite_objects.end()) {
            return;
        }

        if (it->second->getWorldTransform() == world_transform) {
            return;
        }

        it->second->setWorldTransform(world_transform);
        markSpriteDirty(id, SpriteUpdateType::TransformChanged);
    }

    void RenderScene::removeSprite(const UUID id) {
        m_sprite_objects.erase(id);
        markSpriteDirty(id, SpriteUpdateType::Removed);
    }

    void RenderScene::flushUpdates() {
        if (!m_scene_data_dirty && m_pending_primitive_updates.empty() && m_pending_sprite_updates.empty()) {
            return;
        }
        rebuildPipelineSceneData();
    }

    const PrimitiveRenderObject* RenderScene::findPrimitive(const UUID id) const {
        const auto it = m_primitive_objects.find(id);
        return it != m_primitive_objects.end() ? it->second.get() : nullptr;
    }

    const SpriteRenderObject* RenderScene::findSprite(const UUID id) const {
        const auto it = m_sprite_objects.find(id);
        return it != m_sprite_objects.end() ? it->second.get() : nullptr;
    }

    PrimitiveSceneInfo* RenderScene::findPrimitiveSceneInfo(const UUID id) {
        const auto it = m_primitive_scene_info_indices.find(id);
        return it != m_primitive_scene_info_indices.end() ? &m_primitive_scene_infos[it->second] : nullptr;
    }

    const PrimitiveSceneInfo* RenderScene::findPrimitiveSceneInfo(const UUID id) const {
        const auto it = m_primitive_scene_info_indices.find(id);
        return it != m_primitive_scene_info_indices.end() ? &m_primitive_scene_infos[it->second] : nullptr;
    }

    const SpriteSceneInfo* RenderScene::findSpriteSceneInfo(const UUID id) const {
        const auto it = m_sprite_scene_info_indices.find(id);
        return it != m_sprite_scene_info_indices.end() ? &m_sprite_scene_infos[it->second] : nullptr;
    }

    void RenderScene::markPrimitiveDirty(const UUID id, const PrimitiveUpdateType update_type) {
        m_pending_primitive_updates[id] |= update_type;
        m_scene_data_dirty = true;
    }

    void RenderScene::markSpriteDirty(const UUID id, const SpriteUpdateType update_type) {
        m_pending_sprite_updates[id] |= update_type;
        m_scene_data_dirty = true;
    }

    void RenderScene::upsertPrimitiveSceneInfo(const UUID id) {
        const PrimitiveRenderObject* primitive = findPrimitive(id);
        const auto& lod_data = primitive->getLODData();
        if (primitive == nullptr || lod_data.empty()) {
            removePrimitiveSceneInfo(id);
            return;
        }

        const auto& bounds = getMeshBounds(primitive->getUploadData());
        PrimitiveSceneInfo info = primitive->buildSceneInfo(
            static_cast<Identifier>(static_cast<uint64_t>(id)),
            primitive->getWorldTransform(),
            bounds.min,
            bounds.max);

        const auto it = m_primitive_scene_info_indices.find(id);
        if (it != m_primitive_scene_info_indices.end()) {
            m_primitive_scene_infos[it->second] = std::move(info);
            return;
        }

        m_primitive_scene_info_indices[id] = m_primitive_scene_infos.size();
        m_primitive_scene_infos.push_back(std::move(info));
    }

    void RenderScene::applyPrimitiveTransform(const UUID id) {
        PrimitiveSceneInfo* info = findPrimitiveSceneInfo(id);
        if (info == nullptr) {
            upsertPrimitiveSceneInfo(id);
            return;
        }
        const auto* primitive = findPrimitive(id);
        if (primitive) {
            info->setWorldTransform(primitive->getWorldTransform());
        }
    }

    void RenderScene::updatePrimitiveMaterials(const UUID id) {
        const PrimitiveRenderObject* primitive = findPrimitive(id);
        PrimitiveSceneInfo* info = findPrimitiveSceneInfo(id);
        if (primitive == nullptr || info == nullptr) {
            upsertPrimitiveSceneInfo(id);
            return;
        }

        const auto materials = primitive->resolveMaterials();
        info->setMaterials(materials);
        info->setSections(primitive->buildSections(materials));
        info->setMeshBatches(primitive->buildMeshBatches(info->getId(), materials, 0));
    }

    void RenderScene::updatePrimitiveState(const UUID id) {
        const PrimitiveRenderObject* primitive = findPrimitive(id);
        PrimitiveSceneInfo* info = findPrimitiveSceneInfo(id);
        if (primitive == nullptr || info == nullptr) {
            upsertPrimitiveSceneInfo(id);
            return;
        }

        info->setMobility(primitive->getMobility());
        info->setVisible(primitive->isVisible());
        info->setCastShadow(primitive->castsShadow());
    }

    void RenderScene::removePrimitiveSceneInfo(const UUID id) {
        const auto it = m_primitive_scene_info_indices.find(id);
        if (it == m_primitive_scene_info_indices.end()) {
            return;
        }

        const Size_t remove_index = it->second;
        const Size_t last_index = m_primitive_scene_infos.size() - 1;
        if (remove_index != last_index) {
            PrimitiveSceneInfo& moved = m_primitive_scene_infos[last_index];
            const UUID moved_uuid = static_cast<UUID>(static_cast<uint64_t>(moved.getId()));
            m_primitive_scene_infos[remove_index] = std::move(moved);
            m_primitive_scene_info_indices[moved_uuid] = remove_index;
        }
        m_primitive_scene_infos.pop_back();
        m_primitive_scene_info_indices.erase(it);
    }

    void RenderScene::upsertSpriteSceneInfo(const UUID id) {
        const SpriteRenderObject* sprite = findSprite(id);
        DO_DEBUG("RenderScene::upsertSpriteSceneInfo: id={}, sprite={}, visible={}",
                  static_cast<UInt64>(id),
                  static_cast<const void*>(sprite),
                  sprite ? sprite->isVisible() : false);
        if (sprite == nullptr || !sprite->isVisible()) {
            removeSpriteSceneInfo(id);
            return;
        }

        const Matrix4f& transform = sprite->getWorldTransform();
        const Vector3f translation = Vector3f(transform[3]);
        const Vector3f scale = Vector3f(
            Math::Length(Vector3f(transform[0])),
            Math::Length(Vector3f(transform[1])),
            Math::Length(Vector3f(transform[2]))
        );

        SpriteSceneInfo info(static_cast<Identifier>(static_cast<UInt64>(id)));
        info.setRenderObject(sprite);
        info.setWorldTransform(transform);
        info.setPosition(Vector2f(translation.x, translation.y));
        info.setScale(Vector2f(scale.x, scale.y));
        info.setRotation(0.0f);
        info.setUVRect(sprite->getUVMinX(), sprite->getUVMinY(), sprite->getUVMaxX(), sprite->getUVMaxY());
        info.setColor(sprite->getColor());
        info.setSortingKey(0);
        info.setMaterialId(sprite->getMaterialId());
        info.setFlags(sprite->getFlags());
        info.setTexture(sprite->getTexture());

        const auto it = m_sprite_scene_info_indices.find(id);
        if (it != m_sprite_scene_info_indices.end()) {
            m_sprite_scene_infos[it->second] = std::move(info);
            return;
        }

        m_sprite_scene_info_indices[id] = m_sprite_scene_infos.size();
        m_sprite_scene_infos.push_back(std::move(info));
    }

    void RenderScene::applySpriteTransform(const UUID id) {
        const auto it = m_sprite_scene_info_indices.find(id);
        if (it == m_sprite_scene_info_indices.end()) {
            upsertSpriteSceneInfo(id);
            return;
        }
        const auto* sprite = findSprite(id);
        if (sprite) {
            const Matrix4f& transform = sprite->getWorldTransform();
            const Vector3f translation = Vector3f(transform[3]);
            const Vector3f scale = Vector3f(
                Math::Length(Vector3f(transform[0])),
                Math::Length(Vector3f(transform[1])),
                Math::Length(Vector3f(transform[2]))
            );
            SpriteSceneInfo& info = m_sprite_scene_infos[it->second];
            info.setWorldTransform(transform);
            info.setPosition(Vector2f(translation.x, translation.y));
            info.setScale(Vector2f(scale.x, scale.y));
        }
    }

    void RenderScene::removeSpriteSceneInfo(const UUID id) {
        const auto it = m_sprite_scene_info_indices.find(id);
        if (it == m_sprite_scene_info_indices.end()) {
            return;
        }

        const Size_t remove_index = it->second;
        const Size_t last_index = m_sprite_scene_infos.size() - 1;
        if (remove_index != last_index) {
            SpriteSceneInfo& moved = m_sprite_scene_infos[last_index];
            const UUID moved_uuid = static_cast<UUID>(static_cast<uint64_t>(moved.getId()));
            m_sprite_scene_infos[remove_index] = std::move(moved);
            m_sprite_scene_info_indices[moved_uuid] = remove_index;
        }
        m_sprite_scene_infos.pop_back();
        m_sprite_scene_info_indices.erase(it);
    }

    const RenderScene::Aabb& RenderScene::getMeshBounds(const MeshUploadData& upload_data) {
        static const Aabb kDefaultBounds{
            Vector3f(-0.5f, -0.5f, -0.5f),
            Vector3f(0.5f, 0.5f, 0.5f)
        };

        if (upload_data.position_data.empty()) {
            return kDefaultBounds;
        }

        const Size_t hash = upload_data.name.empty() ? 0 : std::hash<String>{}(upload_data.name);
        const auto cached = m_mesh_bounds_cache.find(hash);
        if (cached != m_mesh_bounds_cache.end()) {
            return cached->second;
        }

        Vector3f min_corner = upload_data.position_data.front();
        Vector3f max_corner = upload_data.position_data.front();
        for (const auto& position : upload_data.position_data) {
            min_corner = Vector3f(
                (std::min)(min_corner.x, position.x),
                (std::min)(min_corner.y, position.y),
                (std::min)(min_corner.z, position.z)
            );
            max_corner = Vector3f(
                (std::max)(max_corner.x, position.x),
                (std::max)(max_corner.y, position.y),
                (std::max)(max_corner.z, position.z)
            );
        }

        return m_mesh_bounds_cache.emplace(hash, Aabb{min_corner, max_corner}).first->second;
    }

    void RenderScene::rebuildPipelineSceneData() {
        for (const auto& [id, update_type] : m_pending_primitive_updates) {
            if (HasAnyFlags(update_type, PrimitiveUpdateType::Removed) && m_primitive_objects.find(id) == m_primitive_objects.end()) {
                removePrimitiveSceneInfo(id);
                continue;
            }

            if (HasAnyFlags(update_type, PrimitiveUpdateType::Added | PrimitiveUpdateType::MeshChanged | PrimitiveUpdateType::ProxyChanged)) {
                upsertPrimitiveSceneInfo(id);
                continue;
            }
            if (HasAnyFlags(update_type, PrimitiveUpdateType::TransformChanged)) {
                applyPrimitiveTransform(id);
            }
            if (HasAnyFlags(update_type, PrimitiveUpdateType::MaterialChanged)) {
                updatePrimitiveMaterials(id);
            }
            if (HasAnyFlags(update_type, PrimitiveUpdateType::StateChanged)) {
                updatePrimitiveState(id);
            }
        }

        for (const auto& [id, update_type] : m_pending_sprite_updates) {
            if (HasAnyFlags(update_type, SpriteUpdateType::Removed) && m_sprite_objects.find(id) == m_sprite_objects.end()) {
                removeSpriteSceneInfo(id);
                continue;
            }
            if (HasAnyFlags(update_type, SpriteUpdateType::Added | SpriteUpdateType::TextureChanged | SpriteUpdateType::MaterialChanged | SpriteUpdateType::StateChanged)) {
                upsertSpriteSceneInfo(id);
                continue;
            }
            if (HasAnyFlags(update_type, SpriteUpdateType::TransformChanged)) {
                applySpriteTransform(id);
            }
        }

        m_pending_primitive_updates.clear();
        m_pending_sprite_updates.clear();
        m_scene_data_dirty = false;
    }

} // dodoe
