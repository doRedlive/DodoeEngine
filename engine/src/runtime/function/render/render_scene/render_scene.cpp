// do@Redlive

#include "render_scene.h"
#include "runtime/core/math/math.h"
#include "runtime/core/object/object_id.h"
#include "runtime/core/utils/common.h"
#include "runtime/function/graphics/draw_command_list.h"
#include "runtime/function/render/render_service/shared_render_service.h"
#include "runtime/function/render/mesh_draw/mesh.h"
#include "runtime/function/render/texture/texture.h"
#include "runtime/resource/asset/asset_manager.h"
#include "runtime/resource/resource_manager.h"

namespace dodoe {

    namespace {
        Bool primitiveUpdateRequiresUpsert(const PrimitiveUpdateType update_type) {
            return HasAnyFlags(
                update_type,
                PrimitiveUpdateType::Added | PrimitiveUpdateType::MeshChanged | PrimitiveUpdateType::MaterialChanged | PrimitiveUpdateType::ProxyChanged);
        }

        Bool spriteUpdateRequiresUpsert(const SpriteUpdateType update_type) {
            return HasAnyFlags(
                update_type,
                SpriteUpdateType::Added | SpriteUpdateType::TextureChanged | SpriteUpdateType::MaterialChanged | SpriteUpdateType::StateChanged);
        }
    }

    Bool RenderScene::initialize(const RenderSceneCreateInfo& info) {
        m_shared_render_service = info.shared_render_service;
        m_max_primitive_upserts_per_frame = info.max_primitive_upserts_per_frame > 0 ? info.max_primitive_upserts_per_frame : 1;
        m_max_sprite_upserts_per_frame = info.max_sprite_upserts_per_frame > 0 ? info.max_sprite_upserts_per_frame : 1;
        reset();
        m_gpu_scene = GpuScene::Create({});
        return m_gpu_scene != nullptr;
    }

    void RenderScene::shutdown() {
        reset();
    }

    void RenderScene::reset() {
        m_primitive_objects.clear();
        m_sprite_objects.clear();
        m_scene_data_dirty = true;
        m_primitive_scene_info_indices.clear();
        m_light_scene_info_indices.clear();
        m_sprite_scene_info_indices.clear();
        m_pending_primitive_updates.clear();
        m_pending_sprite_updates.clear();
        m_pending_light_updates.clear();
        m_pending_primitive_order.clear();
        m_pending_sprite_order.clear();
        m_frame_number = 0;
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
        LightUpdateType update_type = LightUpdateType::Added;
        if (it != m_light_scene_info_indices.end()) {
            update_type = LightUpdateType::DataChanged;
            m_light_scene_infos[it->second] = std::move(info);
        } else {
            m_light_scene_info_indices[id] = m_light_scene_infos.size();
            m_light_scene_infos.push_back(std::move(info));
        }
        markLightDirty(id, update_type);
    }

    void RenderScene::updateLightSceneInfoTransform(const UUID id, const Matrix4f& world_transform) {
        const auto it = m_light_scene_info_indices.find(id);
        if (it != m_light_scene_info_indices.end()) {
            m_light_scene_infos[it->second].setWorldTransform(world_transform);
            markLightDirty(id, LightUpdateType::TransformChanged);
        }
    }

    void RenderScene::removeLightSceneInfo(const UUID id) {
        const auto it = m_light_scene_info_indices.find(id);
        if (it == m_light_scene_info_indices.end()) {
            return;
        }
        markLightDirty(id, LightUpdateType::Removed);
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
        if (sprite->isBatch()) {
            m_sprite_objects[id] = std::move(sprite);
            markSpriteDirty(id, SpriteUpdateType::Added);
            return;
        }

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

    void RenderScene::submitUIInstances(DynamicArray<UISceneInfo> instances) {
        m_ui_scene_infos = std::move(instances);
    }

    void RenderScene::flushUpdates(DrawCommandList& cmd_list) {
        DO_PROFILE_SCOPE_CATEGORY("RenderScene::flushUpdates", "frame");
        if (!m_scene_data_dirty && m_pending_primitive_updates.empty() && 
            m_pending_sprite_updates.empty() && m_pending_light_updates.empty()) {
            return;
        }
        m_frame_number++;
        rebuildPipelineSceneData(cmd_list);
    }

    PrimitiveRenderObject* RenderScene::findPrimitive(const UUID id) {
        const auto it = m_primitive_objects.find(id);
        return it != m_primitive_objects.end() ? it->second.get() : nullptr;
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
        const auto [it, inserted] = m_pending_primitive_updates.try_emplace(id, PrimitiveUpdateType::None);
        it->second |= update_type;
        if (inserted) {
            m_pending_primitive_order.push_back(id);
        }
        m_scene_data_dirty = true;
    }

    void RenderScene::markSpriteDirty(const UUID id, const SpriteUpdateType update_type) {
        const auto [it, inserted] = m_pending_sprite_updates.try_emplace(id, SpriteUpdateType::None);
        it->second |= update_type;
        if (inserted) {
            m_pending_sprite_order.push_back(id);
        }
        m_scene_data_dirty = true;
    }

    void RenderScene::markLightDirty(const UUID id, const LightUpdateType update_type) {
        m_pending_light_updates[id] |= update_type;
        m_scene_data_dirty = true;
    }

    void RenderScene::upsertPrimitiveSceneInfo(const UUID id) {
        PrimitiveRenderObject* primitive = findPrimitive(id);
        if (primitive == nullptr || !primitive->getMesh() || primitive->getMesh()->getLODData().empty()) {
            removePrimitiveSceneInfo(id);
            return;
        }

        Vector3f bounds_min(-0.5f, -0.5f, -0.5f);
        Vector3f bounds_max(0.5f, 0.5f, 0.5f);
        const Mesh* mesh = primitive->getMesh();
        if (mesh) {
            bounds_min = mesh->getBoundsMin();
            bounds_max = mesh->getBoundsMax();
        }

        PrimitiveSceneInfo info = primitive->buildSceneInfo(
            static_cast<Identifier>(static_cast<uint64_t>(id)),
            primitive->getWorldTransform(),
            bounds_min,
            bounds_max);

        resolveBatchMaterialInstances(info);

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
        info->setSubMeshes(primitive->buildSections(materials));
        info->setMeshBatches(primitive->buildMeshBatches(info->getId(), materials, 0));
        resolveBatchMaterialInstances(*info);
    }

    void RenderScene::resolveBatchMaterialInstances(PrimitiveSceneInfo& info) {
        auto* material_system = m_shared_render_service->getMaterialSystem();
        auto* texture_manager = m_shared_render_service->getTextureManager();

        const auto& materials = info.getMaterials();
        auto& batches = info.getMeshBatches();

        auto resolveTexture = [&](const PPtr<Texture2D>& texture_ptr) -> GfxTextureHandle {
            if (!texture_manager) return {};
            Texture2D* tex = texture_ptr.get();
            if (!tex && !texture_ptr.getLegacyPath().empty()) {
                const String legacy_path = texture_ptr.getLegacyPath();
                tex = ResourceManager::Self().loadObjectByPath<Texture2D>(FileID(legacy_path));
                if (!tex) {
                    tex = ResourceManager::Self().loadObject<Texture2D>(UUID(static_cast<UInt64>(string2hash(legacy_path))), 0);
                }
            }
            return tex ? tex->getGpuHandle() : GfxTextureHandle{};
        };

        for (Size_t i = 0; i < batches.size(); i++) {
            auto& batch = batches[i];
            if (batch.material_instance) continue;

            const PPtr<Material>& material_ptr = i < materials.size() ? materials[i] : PPtr<Material>{};
            Material* material = material_ptr.get();
            if (!material && !material_ptr.getLegacyPath().empty()) {
                material = ResourceManager::Self().loadObjectByPath<Material>(FileID(material_ptr.getLegacyPath()));
            }

            UnorderedMap<String, MaterialParamValue> overrides;
            auto addTex = [&](const String& name, const PPtr<Texture2D>& texture_ptr) {
                const GfxTextureHandle handle = resolveTexture(texture_ptr);
                if (handle) {
                    MaterialParamValue val{};
                    val.texture = handle;
                    overrides[name] = val;
                }
            };
            if (material) {
                addTex("base_color_texture", material->getBaseColorTexture());
                addTex("normal_texture", material->getNormalTexture());
                addTex("metallic_roughness_texture", material->getMetallicRoughnessTexture());
                addTex("emissive_texture", material->getEmissiveTexture());
            }

            String instance_name = String(fmt::format("Mat_{}_{}", info.getId(), i).c_str());
            batch.material_instance = const_cast<MaterialInstance*>(
                material_system->getOrCreateInstance(instance_name, "GBuffer", overrides));
        }
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
        if (sprite == nullptr || !sprite->isVisible()) {
            removeSpriteSceneInfo(id);
            return;
        }

        SpriteSceneInfo info(static_cast<Identifier>(static_cast<UInt64>(id)));
        info.setRenderObject(sprite);
        info.setVisible(sprite->isVisible());
        info.setSortingKey(sprite->getSortingKey());

        if (sprite->isBatch()) {
            info.setBatchInstances(sprite->getBatchInstances());
            info.setBatchAtlases(sprite->getBatchAtlases());
            info.setBounds(sprite->getBoundsCenter(), sprite->getBoundsExtents());
        } else {
            const Matrix4f& transform = sprite->getWorldTransform();
            const Vector3f translation = Vector3f(transform[3]);
            const Vector3f scale = Vector3f(
                Math::Length(Vector3f(transform[0])),
                Math::Length(Vector3f(transform[1])),
                Math::Length(Vector3f(transform[2]))
            );

            info.setWorldTransform(transform);
            info.setPosition(Vector2f(translation.x, translation.y));
            info.setScale(Vector2f(scale.x, scale.y));
            info.setRotation(0.0f);
            info.setSprite(sprite->getSprite());
            info.setAtlasIndex(sprite->getAtlasIndex());
            info.setUVRect(sprite->getUVMinX(), sprite->getUVMinY(), sprite->getUVMaxX(), sprite->getUVMaxY());
            info.setColor(sprite->getColor());
            info.setMaterialId(sprite->getMaterialId());
            info.setFlags(sprite->getFlags());
        }

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
            SpriteSceneInfo& info = m_sprite_scene_infos[it->second];
            if (info.hasInstances()) {
                info.setWorldTransform(sprite->getWorldTransform());
                return;
            }
            const Matrix4f& transform = sprite->getWorldTransform();
            const Vector3f translation = Vector3f(transform[3]);
            const Vector3f scale = Vector3f(
                Math::Length(Vector3f(transform[0])),
                Math::Length(Vector3f(transform[1])),
                Math::Length(Vector3f(transform[2]))
            );
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

    void RenderScene::rebuildPipelineSceneData(DrawCommandList& cmd_list) {
        DO_PROFILE_SCOPE_CATEGORY("RenderScene::rebuildPipelineSceneData", "frame");
        RenderSceneDelta delta;
        delta.source_frame = m_frame_number;

        processPendingPrimitiveUpdates(delta);
        processPendingSpriteUpdates(delta);
        processPendingLightUpdates(delta);

        m_scene_data_dirty = !m_pending_primitive_updates.empty() || !m_pending_sprite_updates.empty() || !m_pending_light_updates.empty();

        if (m_gpu_scene) {
            syncPrimitiveGpuScene(delta);
            syncSpriteGpuScene(delta);
            syncLightGpuScene(delta);
            m_gpu_scene->flushUpdates(cmd_list);
        }
    }

    void RenderScene::processPendingPrimitiveUpdates(RenderSceneDelta& delta) {
        if (m_pending_primitive_updates.empty()) {
            m_pending_primitive_order.clear();
            return;
        }

        UInt32 upsert_budget = m_max_primitive_upserts_per_frame;
        DynamicArray<UUID> remaining;
        remaining.reserve(m_pending_primitive_order.size());

        for (const UUID id : m_pending_primitive_order) {
            const auto it = m_pending_primitive_updates.find(id);
            if (it == m_pending_primitive_updates.end()) {
                continue;
            }

            if (primitiveUpdateRequiresUpsert(it->second)) {
                if (upsert_budget == 0) {
                    remaining.push_back(id);
                    continue;
                }
                upsert_budget--;
            }

            applyPrimitiveUpdate(id, it->second);
            delta.primitive_updates[id] = it->second;
            m_pending_primitive_updates.erase(it);
        }
        m_pending_primitive_order = std::move(remaining);
    }

    void RenderScene::processPendingSpriteUpdates(RenderSceneDelta& delta) {
        if (m_pending_sprite_updates.empty()) {
            m_pending_sprite_order.clear();
            return;
        }

        UInt32 upsert_budget = m_max_sprite_upserts_per_frame;
        DynamicArray<UUID> remaining;
        remaining.reserve(m_pending_sprite_order.size());

        for (const UUID id : m_pending_sprite_order) {
            const auto it = m_pending_sprite_updates.find(id);
            if (it == m_pending_sprite_updates.end()) {
                continue;
            }

            if (spriteUpdateRequiresUpsert(it->second)) {
                if (upsert_budget == 0) {
                    remaining.push_back(id);
                    continue;
                }
                upsert_budget--;
            }

            applySpriteUpdate(id, it->second);
            delta.sprite_updates[id] = it->second;
            m_pending_sprite_updates.erase(it);
        }
        m_pending_sprite_order = std::move(remaining);
    }

    void RenderScene::processPendingLightUpdates(RenderSceneDelta& delta) {
        delta.light_updates = std::move(m_pending_light_updates);
        m_pending_light_updates.clear();
    }

    void RenderScene::applyPrimitiveUpdate(const UUID id, const PrimitiveUpdateType update_type) {
        if (HasAnyFlags(update_type, PrimitiveUpdateType::Removed) && m_primitive_objects.find(id) == m_primitive_objects.end()) {
            removePrimitiveSceneInfo(id);
            return;
        }

        if (HasAnyFlags(update_type, PrimitiveUpdateType::Added | PrimitiveUpdateType::MeshChanged | PrimitiveUpdateType::ProxyChanged)) {
            upsertPrimitiveSceneInfo(id);
            return;
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

    void RenderScene::applySpriteUpdate(const UUID id, const SpriteUpdateType update_type) {
        if (HasAnyFlags(update_type, SpriteUpdateType::Removed) && m_sprite_objects.find(id) == m_sprite_objects.end()) {
            removeSpriteSceneInfo(id);
            return;
        }

        if (HasAnyFlags(update_type, SpriteUpdateType::Added | SpriteUpdateType::TextureChanged | SpriteUpdateType::MaterialChanged | SpriteUpdateType::StateChanged)) {
            upsertSpriteSceneInfo(id);
            return;
        }
        if (HasAnyFlags(update_type, SpriteUpdateType::TransformChanged)) {
            applySpriteTransform(id);
        }
    }

    void RenderScene::syncPrimitiveGpuScene(const RenderSceneDelta& delta) {
        for (const auto& [id, update_type] : delta.primitive_updates) {
            if (HasAnyFlags(update_type, PrimitiveUpdateType::Removed) && m_primitive_objects.find(id) == m_primitive_objects.end()) {
                auto it = m_cpu_to_gpu_map.find(id);
                if (it != m_cpu_to_gpu_map.end()) {
                    m_gpu_scene->unregisterObject(it->second);
                    m_cpu_to_gpu_map.erase(it);
                }
                continue;
            }

            GpuObjectHandle handle;
            auto it = m_cpu_to_gpu_map.find(id);
            if (it == m_cpu_to_gpu_map.end()) {
                GpuObjectMeta meta{};
                meta.flags = 0;
                meta.data_offset = 0;
                meta.texture_id = 0;
                meta.material_id = 0;
                meta.bounds_id = 0;
                handle = m_gpu_scene->registerObject(GpuObjectType::Primitive, meta);
                m_cpu_to_gpu_map[id] = handle;
            } else {
                handle = it->second;
            }

            const auto* info = findPrimitiveSceneInfo(id);
            if (!info) continue;

            if (HasAnyFlags(update_type, PrimitiveUpdateType::TransformChanged)) {
                const Matrix4f& transform = info->getWorldTransform();
                m_gpu_scene->updateTransform(handle, transform);
                const Vector3f& bounds_min = info->getBoundsMin();
                const Vector3f& bounds_max = info->getBoundsMax();
                const Vector3f center = (bounds_min + bounds_max) * 0.5f;
                const Vector3f extent = (bounds_max - bounds_min) * 0.5f;
                m_gpu_scene->updateBounds(handle, center, extent);
            }

            if (HasAnyFlags(update_type, PrimitiveUpdateType::Added | PrimitiveUpdateType::MeshChanged |
                                           PrimitiveUpdateType::MaterialChanged | PrimitiveUpdateType::ProxyChanged |
                                           PrimitiveUpdateType::StateChanged)) {
                PrimitiveGpuData gpu_data{};
                gpu_data.transform_index = handle.index();
                gpu_data.mesh_id = 0;
                gpu_data.section_start = 0;
                gpu_data.section_count = static_cast<UInt32>(info->getSubMeshes().size());
                gpu_data.material_start = 0;
                gpu_data.material_count = static_cast<UInt32>(info->getMaterials().size());
                m_gpu_scene->updatePrimitiveInstance(handle, gpu_data);
            }
        }
    }

    TextureManager* RenderScene::getTextureManager() const {
        return m_shared_render_service ? m_shared_render_service->getTextureManager() : nullptr;
    }

    UInt32 RenderScene::resolveSpriteAtlasIndex(const SpriteSceneInfo& info) const {
        auto* texture_manager = getTextureManager();
        if (!texture_manager) { return 0; }
        const auto* sprite = info.getSprite().get();
        if (!sprite) { return 0; }
        return texture_manager->resolveAtlasIndex(sprite->getTexture().get());
    }

    void RenderScene::syncSpriteGpuScene(const RenderSceneDelta& delta) {
        for (const auto& [id, update_type] : delta.sprite_updates) {
            if (HasAnyFlags(update_type, SpriteUpdateType::Removed) && m_sprite_objects.find(id) == m_sprite_objects.end()) {
                auto it = m_cpu_to_gpu_map.find(id);
                if (it != m_cpu_to_gpu_map.end()) {
                    m_gpu_scene->unregisterObject(it->second);
                    m_cpu_to_gpu_map.erase(it);
                }
                continue;
            }

            const auto* info = findSpriteSceneInfo(id);
            if (!info) continue;
            if (info->hasInstances()) {
                continue;
            }

            GpuObjectHandle handle;
            auto it = m_cpu_to_gpu_map.find(id);
            if (it == m_cpu_to_gpu_map.end()) {
                GpuObjectMeta meta{};
                meta.flags = 0;
                meta.data_offset = 0;
                meta.texture_id = 0;
                meta.material_id = 0;
                meta.bounds_id = 0;
                handle = m_gpu_scene->registerObject(GpuObjectType::Sprite, meta);
                m_cpu_to_gpu_map[id] = handle;
            } else {
                handle = it->second;
            }

            if (HasAnyFlags(update_type, SpriteUpdateType::TransformChanged)) {
                const Matrix4f& transform = info->getWorldTransform();
                m_gpu_scene->updateTransform(handle, transform);
                const Vector3f translation = Vector3f(transform[3]);
                const Vector3f extent = Vector3f(
                    info->getScale().x * 0.5f,
                    info->getScale().y * 0.5f,
                    0.01f);
                m_gpu_scene->updateBounds(handle, translation, extent);
            }

            if (HasAnyFlags(update_type, SpriteUpdateType::Added | SpriteUpdateType::TextureChanged |
                                           SpriteUpdateType::MaterialChanged | SpriteUpdateType::StateChanged)) {
                SpriteGpuData gpu_data{};
                gpu_data.position_x = info->getPosition().x;
                gpu_data.position_y = info->getPosition().y;
                gpu_data.scale_x = info->getScale().x;
                gpu_data.scale_y = info->getScale().y;
                gpu_data.rotation = info->getRotation();
                gpu_data.atlas_index = resolveSpriteAtlasIndex(*info);
                gpu_data.uv_min_x = info->getUVMinX();
                gpu_data.uv_min_y = info->getUVMinY();
                gpu_data.uv_max_x = info->getUVMaxX();
                gpu_data.uv_max_y = info->getUVMaxY();
                gpu_data.color = info->getColor();
                gpu_data.sorting_key = info->getSortingKey();
                gpu_data.material_id = info->getMaterialId();
                gpu_data.flags = info->getFlags();
                m_gpu_scene->updateSpriteInstance(handle, gpu_data);
            }
        }
    }

    void RenderScene::syncLightGpuScene(const RenderSceneDelta& delta) {
        for (const auto& [id, update_type] : delta.light_updates) {
            if (HasAnyFlags(update_type, LightUpdateType::Removed)) {
                auto it = m_cpu_to_gpu_map.find(id);
                if (it != m_cpu_to_gpu_map.end()) {
                    m_gpu_scene->unregisterObject(it->second);
                    m_cpu_to_gpu_map.erase(it);
                }
                continue;
            }

            GpuObjectHandle handle;
            auto it = m_cpu_to_gpu_map.find(id);
            if (it == m_cpu_to_gpu_map.end()) {
                GpuObjectMeta meta{};
                meta.flags = 0;
                meta.data_offset = 0;
                meta.texture_id = 0;
                meta.material_id = 0;
                meta.bounds_id = 0;
                handle = m_gpu_scene->registerObject(GpuObjectType::Light, meta);
                m_cpu_to_gpu_map[id] = handle;
            } else {
                handle = it->second;
            }

            const auto* info = findLightSceneInfo(id);
            if (!info) continue;

            if (HasAnyFlags(update_type, LightUpdateType::Added | LightUpdateType::TransformChanged | LightUpdateType::DataChanged)) {
                const Matrix4f& transform = info->getWorldTransform();
                const Vector3f position = Vector3f(transform[3]);
                m_gpu_scene->updateTransform(handle, transform);
                m_gpu_scene->updateBounds(handle, position, Vector3f(1.0f));

                LightGpuData gpu_data{};
                gpu_data.position = position;
                gpu_data.light_type = static_cast<UInt32>(info->getLightType());
                gpu_data.cast_shadow = info->castsShadow() ? 1u : 0u;
                gpu_data.cubemap_index = 0u;

                switch (info->getLightType()) {
                case LightType::Directional: {
                    const auto& d = info->getDirectionalLightData();
                    gpu_data.direction = d.direction;
                    gpu_data.color = d.color;
                    gpu_data.intensity = d.irradiance;
                    gpu_data.radius = 0.0f;
                    gpu_data.range = 0.0f;
                    gpu_data.inner_angle = 0.0f;
                    gpu_data.outer_angle = 0.0f;
                    break;
                }
                case LightType::Point: {
                    const auto& p = info->getPointLightData();
                    gpu_data.direction = Vector3f(0.0f);
                    gpu_data.radius = p.radius;
                    gpu_data.range = p.range;
                    gpu_data.color = p.color;
                    gpu_data.intensity = p.intensity;
                    gpu_data.inner_angle = 0.0f;
                    gpu_data.outer_angle = 0.0f;
                    break;
                }
                case LightType::Spot: {
                    const auto& s = info->getSpotLightData();
                    gpu_data.direction = Vector3f(transform[2]);
                    gpu_data.radius = s.radius;
                    gpu_data.range = s.range;
                    gpu_data.color = s.color;
                    gpu_data.intensity = s.intensity;
                    gpu_data.inner_angle = s.inner_angle;
                    gpu_data.outer_angle = s.outer_angle;
                    break;
                }
                case LightType::Sky: {
                    const auto& sk = info->getSkyLightData();
                    gpu_data.direction = Vector3f(0.0f);
                    gpu_data.color = Vector3f(1.0f);
                    gpu_data.intensity = sk.intensity;
                    gpu_data.radius = 0.0f;
                    gpu_data.range = 0.0f;
                    gpu_data.inner_angle = 0.0f;
                    gpu_data.outer_angle = 0.0f;
                    break;
                }
                }
                m_gpu_scene->updateLightInstance(handle, gpu_data);
            }
        }
    }

} // dodoe
