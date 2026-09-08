// do@Redlive

#include "texture_manager.h"

#include "texture.h"

#include "runtime/core/utils/common.h"
#include "runtime/core/object/object_id.h"
#include "runtime/core/context/system_context.h"
#include "runtime/resource/file/file_id.h"
#include "runtime/resource/file/file_system.h"
#include "runtime/resource/resource_manager.h"
#include "runtime/resource/asset/asset_manager.h"
#include "runtime/resource/parser/texture_blob.h"
#include "runtime/function/graphics/gfx_context.h"
#include "runtime/function/render/render_command.h"
#include "runtime/function/render/render_settings.h"

#include <algorithm>
#include <cmath>

namespace dodoe {

    namespace {

        ObjectID ResolveTextureRef(const String& path) {
            if (auto* asset_manager = ResourceManager::Self().getAssetManager()) {
                const ObjectID ref = asset_manager->resolvePathToRef(FileID(path));
                if (ref.isValid()) {
                    return ref;
                }
            }
            return ObjectID{UUID(static_cast<UInt64>(string2hash(path))), 0};
        }

        void TexelDirection(UInt32 face, UInt32 x, UInt32 y, UInt32 size, Float* out_dir) {
            const Float s = (static_cast<Float>(x) + 0.5f) / static_cast<Float>(size) * 2.0f - 1.0f;
            const Float t = (static_cast<Float>(y) + 0.5f) / static_cast<Float>(size) * 2.0f - 1.0f;
            switch (face) {
            case 0: out_dir[0] = 1.0f; out_dir[1] = -t; out_dir[2] = -s; break;
            case 1: out_dir[0] = -1.0f; out_dir[1] = -t; out_dir[2] = s; break;
            case 2: out_dir[0] = s; out_dir[1] = 1.0f; out_dir[2] = t; break;
            case 3: out_dir[0] = s; out_dir[1] = -1.0f; out_dir[2] = -t; break;
            case 4: out_dir[0] = s; out_dir[1] = -t; out_dir[2] = 1.0f; break;
            default: out_dir[0] = -s; out_dir[1] = -t; out_dir[2] = -1.0f; break;
            }
        }

        void DirectionToFaceUV(const Float* dir, UInt32& face, Float& u, Float& v) {
            const Float ax = dir[0] < 0.0f ? -dir[0] : dir[0];
            const Float ay = dir[1] < 0.0f ? -dir[1] : dir[1];
            const Float az = dir[2] < 0.0f ? -dir[2] : dir[2];
            Float sc = 0.0f;
            Float tc = 0.0f;
            Float ma = 1.0f;
            if (ax >= ay && ax >= az) {
                face = dir[0] >= 0.0f ? 0u : 1u;
                sc = face == 0u ? -dir[2] : dir[2];
                tc = -dir[1];
                ma = ax;
            } else if (ay >= az) {
                face = dir[1] >= 0.0f ? 2u : 3u;
                sc = dir[0];
                tc = face == 2u ? dir[2] : -dir[2];
                ma = ay;
            } else {
                face = dir[2] >= 0.0f ? 4u : 5u;
                sc = face == 4u ? dir[0] : -dir[0];
                tc = -dir[1];
                ma = az;
            }
            const Float inv_ma = 1.0f / ma;
            u = sc * inv_ma * 0.5f + 0.5f;
            v = tc * inv_ma * 0.5f + 0.5f;
        }

        void SampleFaceBilinear(const Float* face_data, UInt32 size, Float u, Float v, Float* out_rgba) {
            Float fx = u * static_cast<Float>(size) - 0.5f;
            Float fy = v * static_cast<Float>(size) - 0.5f;
            fx = fx < 0.0f ? 0.0f : fx;
            fy = fy < 0.0f ? 0.0f : fy;
            const UInt32 max_index = size - 1u;
            UInt32 x0 = static_cast<UInt32>(fx);
            UInt32 y0 = static_cast<UInt32>(fy);
            if (x0 > max_index) x0 = max_index;
            if (y0 > max_index) y0 = max_index;
            const UInt32 x1 = x0 + 1u > max_index ? max_index : x0 + 1u;
            const UInt32 y1 = y0 + 1u > max_index ? max_index : y0 + 1u;
            const Float frx = fx - static_cast<Float>(x0);
            const Float fry = fy - static_cast<Float>(y0);
            for (UInt32 c = 0; c < 4u; ++c) {
                const Float c00 = face_data[(static_cast<Size_t>(y0) * size + x0) * 4u + c];
                const Float c10 = face_data[(static_cast<Size_t>(y0) * size + x1) * 4u + c];
                const Float c01 = face_data[(static_cast<Size_t>(y1) * size + x0) * 4u + c];
                const Float c11 = face_data[(static_cast<Size_t>(y1) * size + x1) * 4u + c];
                const Float top = c00 + (c10 - c00) * frx;
                const Float bottom = c01 + (c11 - c01) * frx;
                out_rgba[c] = top + (bottom - top) * fry;
            }
        }

        void SampleCubemap(const DynamicArray<Float>* faces, UInt32 size, const Float* dir, Float* out_rgba) {
            UInt32 face = 0;
            Float u = 0.0f;
            Float v = 0.0f;
            DirectionToFaceUV(dir, face, u, v);
            SampleFaceBilinear(faces[face].data(), size, u, v, out_rgba);
        }

        Float RadicalInverseBase2(UInt32 index) {
            UInt32 bits = index;
            bits = (bits << 16u) | (bits >> 16u);
            bits = ((bits & 0x55555555u) << 1u) | ((bits & 0xAAAAAAAAu) >> 1u);
            bits = ((bits & 0x33333333u) << 2u) | ((bits & 0xCCCCCCCCu) >> 2u);
            bits = ((bits & 0x0F0F0F0Fu) << 4u) | ((bits & 0xF0F0F0F0u) >> 4u);
            bits = ((bits & 0x00FF00FFu) << 8u) | ((bits & 0xFF00FF00u) >> 8u);
            return static_cast<Float>(static_cast<Double>(bits) * 2.3283064365386963e-10);
        }

        Float RadicalInverseBase3(UInt32 index) {
            UInt32 bits = index;
            Float result = 0.0f;
            Float scale = 1.0f / 3.0f;
            while (bits != 0u) {
                result += static_cast<Float>(bits % 3u) * scale;
                bits /= 3u;
                scale /= 3.0f;
            }
            return result;
        }

        void EvaluateSHBasis9(const Float* dir, Float* out_basis) {
            const Float x = dir[0];
            const Float y = dir[1];
            const Float z = dir[2];
            out_basis[0] = 0.282095f;
            out_basis[1] = 0.488603f * y;
            out_basis[2] = 0.488603f * z;
            out_basis[3] = 0.488603f * x;
            out_basis[4] = 1.092548f * x * z;
            out_basis[5] = 1.092548f * y * z;
            out_basis[6] = 0.315392f * (3.0f * z * z - 1.0f);
            out_basis[7] = 1.092548f * x * y;
            out_basis[8] = 1.092548f * (x * x - y * y);
        }

        void PrefilterFaceGGX(Float* dst, const DynamicArray<Float>* src_faces, UInt32 src_size,
                             UInt32 face, UInt32 dst_size, Float roughness) {
            constexpr UInt32 kSampleCount = 32;
            const Float alpha = roughness * roughness;
            for (UInt32 y = 0; y < dst_size; ++y) {
                for (UInt32 x = 0; x < dst_size; ++x) {
                    Float n[3];
                    TexelDirection(face, x, y, dst_size, n);
                    const Float up[3] = {0.0f, 0.0f, 1.0f};
                    const Float fallback_up[3] = {1.0f, 0.0f, 0.0f};
                    const Float* up_vec = (n[2] * n[2] < 0.999f) ? up : fallback_up;
                    Float tx[3] = {
                        up_vec[1] * n[2] - up_vec[2] * n[1],
                        up_vec[2] * n[0] - up_vec[0] * n[2],
                        up_vec[0] * n[1] - up_vec[1] * n[0]
                    };
                    Float tx_len = std::sqrt(tx[0] * tx[0] + tx[1] * tx[1] + tx[2] * tx[2]);
                    if (tx_len < 1e-6f) {
                        tx_len = 1.0f;
                    }
                    tx[0] /= tx_len;
                    tx[1] /= tx_len;
                    tx[2] /= tx_len;
                    const Float ty[3] = {
                        n[1] * tx[2] - n[2] * tx[1],
                        n[2] * tx[0] - n[0] * tx[2],
                        n[0] * tx[1] - n[1] * tx[0]
                    };

                    Float accum[4] = {0.0f, 0.0f, 0.0f, 0.0f};
                    Float total_weight = 0.0f;
                    for (UInt32 sample = 0; sample < kSampleCount; ++sample) {
                        const Float u1 = RadicalInverseBase2(sample);
                        const Float u2 = RadicalInverseBase3(sample);
                        const Float phi = 6.2831853f * u1;
                        const Float cos_theta = std::sqrt(
                            (1.0f - u2) / (1.0f + (alpha * alpha - 1.0f) * u2));
                        const Float sin_theta = std::sqrt(
                            1.0f - std::max(0.0f, 1.0f - cos_theta * cos_theta));
                        const Float h[3] = {
                            tx[0] * (sin_theta * std::cos(phi)) + ty[0] * (sin_theta * std::sin(phi)) + n[0] * cos_theta,
                            tx[1] * (sin_theta * std::cos(phi)) + ty[1] * (sin_theta * std::sin(phi)) + n[1] * cos_theta,
                            tx[2] * (sin_theta * std::cos(phi)) + ty[2] * (sin_theta * std::sin(phi)) + n[2] * cos_theta
                        };
                        const Float ndot_h = n[0] * h[0] + n[1] * h[1] + n[2] * h[2];
                        const Float l[3] = {
                            2.0f * ndot_h * h[0] - n[0],
                            2.0f * ndot_h * h[1] - n[1],
                            2.0f * ndot_h * h[2] - n[2]
                        };
                        const Float ndot_l = n[0] * l[0] + n[1] * l[1] + n[2] * l[2];
                        if (ndot_l > 0.0f) {
                            Float color[4];
                            SampleCubemap(src_faces, src_size, l, color);
                            for (UInt32 c = 0; c < 3u; ++c) {
                                accum[c] += color[c] * ndot_l;
                            }
                            total_weight += ndot_l;
                        }
                    }
                    const Float inv_weight = 1.0f / std::max(total_weight, 1e-5f);
                    const Size_t di = (static_cast<Size_t>(y) * dst_size + x) * 4u;
                    dst[di] = accum[0] * inv_weight;
                    dst[di + 1] = accum[1] * inv_weight;
                    dst[di + 2] = accum[2] * inv_weight;
                    dst[di + 3] = 1.0f;
                }
            }
        }
    }

    TextureCubemap* TextureCubemap::LoadFromFaces(const DynamicArray<String>& face_paths) {
        auto* texture_manager = GetRenderSystem()->getSharedRenderService()->getTextureManager();
        if (!texture_manager) {
            return nullptr;
        }
        return texture_manager->loadCubemapTexture(face_paths);
    }

    Bool TextureManager::initialize(const TextureManagerCreateInfo& info) {
        DO_PROFILE_SCOPE_CATEGORY("TextureManager::initialize", "startup");
        m_gfx = info.gfx;
        m_descriptor_table = info.descriptor_table;
        if (!m_gfx) {
            DO_ERROR("TextureManager::initialize: graphics context is unavailable");
            return false;
        }
        m_device = m_gfx->getDevice();
        if (!m_device) {
            DO_ERROR("TextureManager::initialize: graphics device is unavailable");
            return false;
        }
        if (RenderSettings::IsBindlessActive() && !m_descriptor_table) {
            DO_ERROR("TextureManager::initialize: bindless descriptor table is unavailable");
            return false;
        }
        createFallbackTexture();
        createBrdfLookupTexture();
        DO_INFO("TextureManager: initialized (bindless={})", RenderSettings::IsBindlessActive());
        return true;
    }

    void TextureManager::shutdown() {
        DO_PROFILE_SCOPE_CATEGORY("TextureManager::shutdown", "shutdown");
        DO_INFO("TextureManager: releasing {} 2D texture(s) and {} cubemap(s)",
            m_texture2d_cache.size(), m_cubemap_cache.size());
        m_slot_lut.clear();
        m_texture2d_cache.clear();
        m_cubemap_cache.clear();
        m_cubemap_by_path.clear();
        m_fallback = {};
        m_fallback_cubemap = {};
        m_brdf_lut = {};
        m_device = nullptr;
        m_descriptor_table = nullptr;
        m_gfx = nullptr;
    }

    Texture* TextureManager::findTexture(const InstanceID id) {
        {
            const auto it = m_texture2d_cache.find(id);
            if (it != m_texture2d_cache.end()) {
                return it->second.get();
            }
        }
        {
            const auto it = m_cubemap_cache.find(id);
            if (it != m_cubemap_cache.end()) {
                return it->second.get();
            }
        }
        return m_fallback.get();
    }

    Texture2D* TextureManager::findTexture2D(const InstanceID id) {
        const auto it = m_texture2d_cache.find(id);
        if (it != m_texture2d_cache.end()) {
            return it->second.get();
        }
        return nullptr;
    }

    Texture2D* TextureManager::getFallback() const {
        return m_fallback.get();
    }

    TextureCubemap* TextureManager::getFallbackCubemap() const {
        return m_fallback_cubemap.get();
    }

    Texture2D* TextureManager::getBrdfLut() const {
        return m_brdf_lut.get();
    }

    void TextureManager::removeTexture(const InstanceID id) {
        DO_PROFILE_SCOPE_CATEGORY("TextureManager::removeTexture", "texture");
        m_texture2d_cache.erase(id);
        m_cubemap_cache.erase(id);
        // DO_DEBUG("TextureManager: removed texture {}", static_cast<UInt64>(id));
    }

    Texture2D* TextureManager::realizeTexture(ResourceCommand& cmd) {
        DO_PROFILE_SCOPE_CATEGORY("TextureManager::realizeTexture", "render-command");
        auto texture = std::move(cmd.texture_object);
        if (!texture) {
            DO_WARN("TextureManager::realizeTexture: command has no texture object");
            return nullptr;
        }
        const String texture_path = texture->getPath();
        if (!m_device) {
            DO_ERROR("TextureManager::realizeTexture: graphics device is unavailable for '{}'", texture_path);
            return nullptr;
        }
        if (RenderSettings::IsBindlessActive() && !m_descriptor_table) {
            DO_ERROR("TextureManager::realizeTexture: bindless descriptor table is unavailable for '{}'", texture_path);
            return nullptr;
        }

        const UInt32 width = static_cast<UInt32>(texture->getWidth());
        const UInt32 height = static_cast<UInt32>(texture->getHeight());

        auto texture_desc = GfxTextureDesc()
            .setDimension(GfxTextureDimension::Texture2D)
            .setWidth(width)
            .setHeight(height)
            .setFormat(cmd.texture_is_hdr ? GfxFormat::RGBA32_FLOAT : GfxFormat::RGBA8_UNORM)
            .setMipLevels(1)
            .enableAutomaticStateTracking(GfxResourceStates::ShaderResource)
            .setDebugName(texture->getPath().c_str());

        auto handle = create_ref<GfxTexture>(texture_desc);
        handle->initializeGpu(m_device);

        if (!cmd.resource_data.empty()) {
            const UInt32 bpp = cmd.texture_is_hdr ? 16u : 4u;
            const Size_t row_pitch = static_cast<Size_t>(width) * bpp;
            GDrawCommandList.writeTexture(handle, 0, 0, cmd.resource_data.data(), row_pitch);
        }
        texture->setGpuHandle(handle);

        const UInt32 slot = static_cast<UInt32>(m_slot_lut.size());
        m_slot_lut.push_back(texture->getInstanceID());
        texture->setSlot(slot);

        if (RenderSettings::IsBindlessActive()) {
            DescriptorIndex desc_idx = static_cast<DescriptorIndex>(m_descriptor_table->allocateSlot());
            DO_ASSERT(static_cast<UInt32>(desc_idx) == slot);
            auto item = GfxBindingSetItem::Texture_SRV(0, handle->getRHIHandle());
            item.slot = desc_idx;
            handle->getRHIHandle()->AddRef();
            m_device->writeDescriptorTable(m_descriptor_table->getDescriptorTable(), item);
            texture->setDescriptorIndex(desc_idx);
        }

        Texture2D* realized = texture.get();
        const InstanceID id = realized->getInstanceID();
        m_texture2d_cache.emplace(id, std::move(texture));
        DO_INFO("TextureManager: realized texture '{}' ({}x{}, hdr={}, slot={})",
            texture_path,
            width, height, cmd.texture_is_hdr, slot);
        return realized;
    }

    UInt32 TextureManager::resolveAtlasIndex(const Texture2D* texture) const {
        if (!texture) { return 0; }
        if (texture->getDescriptorIndex() >= 0) {
            return static_cast<UInt32>(texture->getDescriptorIndex());
        }
        return texture->getSlot();
    }

    void TextureManager::createFallbackTexture() {
        DO_PROFILE_SCOPE_CATEGORY("TextureManager::createFallbackTexture", "startup");
        auto texture_desc = GfxTextureDesc()
            .setDimension(GfxTextureDimension::Texture2D)
            .setWidth(1)
            .setHeight(1)
            .setFormat(GfxFormat::RGBA8_UNORM)
            .setMipLevels(1)
            .enableAutomaticStateTracking(GfxResourceStates::ShaderResource)
            .setDebugName("Render TextureManager Fallback");

        const UByte white[4] = {255, 255, 255, 255};

        auto upload_cmd = m_device->createCommandList();
        upload_cmd->open();
        auto handle_rhi = m_device->createTexture(texture_desc);
        upload_cmd->writeTexture(handle_rhi, 0, 0, white, 4);
        upload_cmd->close();
        m_device->executeCommandList(upload_cmd);

        auto handle = create_ref<GfxTexture>(handle_rhi, texture_desc, "Render TextureManager Fallback");

        auto fb_scope = create_scope<Texture2D>(ObjectID{UUID(0), 1});
        Texture2D* fb = fb_scope.get();
        fb->setName("<fallback>");
        fb->setDimensions(1, 1);
        fb->setGpuHandle(handle);

        const UInt32 slot = static_cast<UInt32>(m_slot_lut.size());
        m_slot_lut.push_back(fb->getInstanceID());
        fb->setSlot(slot);

        if (RenderSettings::IsBindlessActive()) {
            auto fallback_item = GfxBindingSetItem::Texture_SRV(0, handle_rhi);
            DescriptorIndex fallback_descriptor_index = m_descriptor_table->createDescriptor(fallback_item);
            fb->setDescriptorIndex(fallback_descriptor_index);
        }

        m_fallback = std::move(fb_scope);

        const auto cube_desc = GfxTextureDesc()
            .setDimension(GfxTextureDimension::TextureCube)
            .setWidth(1)
            .setHeight(1)
            .setArraySize(6)
            .setMipLevels(1)
            .setFormat(GfxFormat::RGBA8_UNORM)
            .enableAutomaticStateTracking(GfxResourceStates::ShaderResource)
            .setDebugName("Render TextureManager Fallback Cubemap");

        const UByte black[4] = {0, 0, 0, 0};

        auto cube_upload = m_device->createCommandList();
        cube_upload->open();
        auto cube_rhi = m_device->createTexture(cube_desc);
        for (UInt32 face = 0; face < 6; ++face) {
            cube_upload->writeTexture(cube_rhi, face, 0, black, 4);
        }
        cube_upload->close();
        m_device->executeCommandList(cube_upload);

        auto cube_handle = create_ref<GfxTexture>(cube_rhi, cube_desc, "Render TextureManager Fallback Cubemap");
        auto cb_scope = create_scope<TextureCubemap>(ObjectID{UUID(0), 2});
        cb_scope->setFaceSize(1);
        cb_scope->setGpuHandle(cube_handle);
        m_fallback_cubemap = std::move(cb_scope);
        DO_INFO("TextureManager: fallback 2D texture and cubemap created");
    }

    void TextureManager::createBrdfLookupTexture() {
        DO_PROFILE_SCOPE_CATEGORY("TextureManager::createBrdfLookupTexture", "startup");
        constexpr UInt32 kLutSize = 128;
        constexpr UInt32 kSampleCount = 512;
        constexpr Float kPi = 3.14159265358979f;

        DynamicArray<Float> lut(static_cast<Size_t>(kLutSize) * kLutSize * 4u);
        for (UInt32 v = 0; v < kLutSize; ++v) {
            const Float roughness = (static_cast<Float>(v) + 0.5f) / static_cast<Float>(kLutSize);
            for (UInt32 u = 0; u < kLutSize; ++u) {
                const Float ndot_v = std::min(
                    (static_cast<Float>(u) + 0.5f) / static_cast<Float>(kLutSize), 0.99f);
                const Float view[3] = {ndot_v, std::sqrt(std::max(0.0f, 1.0f - ndot_v * ndot_v)), 0.0f};
                const Float normal[3] = {0.0f, 0.0f, 1.0f};
                const Float alpha = roughness * roughness;
                const Float k = (roughness + 1.0f) * (roughness + 1.0f) / 8.0f;
                Float sum_scale = 0.0f;
                Float sum_bias = 0.0f;
                for (UInt32 sample = 0; sample < kSampleCount; ++sample) {
                    const Float u1 = RadicalInverseBase2(sample);
                    const Float u2 = RadicalInverseBase3(sample);
                    const Float phi = 2.0f * kPi * u1;
                    const Float cos_theta = std::sqrt(
                        (1.0f - u2) / (1.0f + (alpha * alpha - 1.0f) * u2));
                    const Float sin_theta = std::sqrt(std::max(0.0f, 1.0f - cos_theta * cos_theta));
                    const Float h[3] = {sin_theta * std::cos(phi), sin_theta * std::sin(phi), cos_theta};
                    const Float ndot_h = cos_theta;
                    const Float vdot_h_raw = view[0] * h[0] + view[1] * h[1] + view[2] * h[2];
                    const Float vdot_h = std::max(vdot_h_raw, 1e-5f);
                    const Float l[3] = {
                        2.0f * ndot_h * h[0] - view[0],
                        2.0f * ndot_h * h[1] - view[1],
                        2.0f * ndot_h * h[2] - view[2]
                    };
                    const Float ndot_l = std::max(l[2], 0.0f);
                    if (ndot_l > 0.0f) {
                        const Float g_v = ndot_v / std::max(ndot_v * (1.0f - k) + k, 1e-5f);
                        const Float g_l = ndot_l / std::max(ndot_l * (1.0f - k) + k, 1e-5f);
                        const Float g = g_v * g_l;
                        const Float g_vis = g * vdot_h / std::max(ndot_h * ndot_v, 1e-5f);
                        const Float fc = std::pow(1.0f - vdot_h, 5.0f);
                        sum_scale += (1.0f - fc) * g_vis;
                        sum_bias += fc * g_vis;
                    }
                }
                const Size_t di = (static_cast<Size_t>(v) * kLutSize + u) * 4u;
                lut[di] = sum_scale / static_cast<Float>(kSampleCount);
                lut[di + 1] = sum_bias / static_cast<Float>(kSampleCount);
                lut[di + 2] = 0.0f;
                lut[di + 3] = 1.0f;
            }
        }

        auto texture_desc = GfxTextureDesc()
            .setDimension(GfxTextureDimension::Texture2D)
            .setWidth(kLutSize)
            .setHeight(kLutSize)
            .setFormat(GfxFormat::RGBA32_FLOAT)
            .setMipLevels(1)
            .enableAutomaticStateTracking(GfxResourceStates::ShaderResource)
            .setDebugName("Render BRDF Lookup");

        auto upload_cmd = m_device->createCommandList();
        upload_cmd->open();
        auto handle_rhi = m_device->createTexture(texture_desc);
        upload_cmd->writeTexture(handle_rhi, 0, 0, lut.data(),
            static_cast<Size_t>(kLutSize) * 4u * sizeof(Float));
        upload_cmd->close();
        m_device->executeCommandList(upload_cmd);

        auto handle = create_ref<GfxTexture>(handle_rhi, texture_desc, "Render BRDF Lookup");
        auto lut_scope = create_scope<Texture2D>(ObjectID{UUID(0), 3});
        lut_scope->setName("<brdf_lut>");
        lut_scope->setDimensions(static_cast<Int32>(kLutSize), static_cast<Int32>(kLutSize));
        lut_scope->setGpuHandle(handle);
        m_brdf_lut = std::move(lut_scope);
        DO_INFO("TextureManager: BRDF lookup texture created ({}x{})", kLutSize, kLutSize);
    }

    TextureCubemap* TextureManager::loadCubemapTexture(const DynamicArray<String>& face_paths) {
        return loadCubemapTexture(face_paths, GDrawCommandList, nullptr);
    }

    TextureCubemap* TextureManager::loadCubemapTexture(const DynamicArray<String>& face_paths, DrawCommandList& cmd_list, FrameStagingAllocator* staging) {
        DO_PROFILE_SCOPE_CATEGORY("TextureManager::loadCubemapTexture", "texture");
        if (face_paths.size() < 6) {
            DO_ERROR("TextureManager::loadCubemapTexture: expected 6 faces, got {}", face_paths.size());
            return nullptr;
        }

        const auto path_it = m_cubemap_by_path.find(face_paths[0]);
        if (path_it != m_cubemap_by_path.end()) {
            const InstanceID existing = path_it->second;
            const auto it = m_cubemap_cache.find(existing);
            if (it != m_cubemap_cache.end()) {
                // DO_DEBUG("TextureManager: reusing cubemap '{}'", face_paths[0]);
                return it->second.get();
            }
        }

        constexpr ui32 kFaceCount = 6;

        std::array<TextureBlob, kFaceCount> faces{};
        for (ui32 i = 0; i < kFaceCount; ++i) {
            auto fp = FileSystem::RelativeToAbsolute(face_paths[i], FileSystem::GetEngineResPath());
            faces[i].load(fp, false);
            if (!faces[i].isValid()) {
                DO_ERROR("TextureManager::loadCubemapTexture: failed to load face {} ('{}')", i, face_paths[i]);
                return nullptr;
            }
            if (faces[i].width != faces[i].height ||
                (i > 0 && (faces[i].width != faces[0].width || faces[i].height != faces[0].height))) {
                DO_ERROR("TextureManager::loadCubemapTexture: face {} has incompatible dimensions ({}x{})",
                    i, faces[i].width, faces[i].height);
                return nullptr;
            }
        }

        const ui32 face_size = static_cast<ui32>(faces[0].width);
        ui32 mip_count = 1;
        while ((face_size >> mip_count) != 0) {
            ++mip_count;
        }

        auto desc = GfxTextureDesc()
            .setDimension(GfxTextureDimension::TextureCube)
            .setWidth(face_size)
            .setHeight(face_size)
            .setArraySize(kFaceCount)
            .setMipLevels(mip_count)
            .setFormat(GfxFormat::RGBA32_FLOAT)
            .enableAutomaticStateTracking(GfxResourceStates::ShaderResource)
            .setDebugName("SkyLight Cubemap");
        auto cubemap = cmd_list.createTexture(desc);
        if (!cubemap) {
            DO_ERROR("TextureManager::loadCubemapTexture: failed to create GPU cubemap");
            return nullptr;
        }

        DynamicArray<Float> filtered_faces[6];
        Float sh_accum[9][3]{};
        Vector4f sh_irradiance[9]{};
        const Float sh_texel_weight = 4.0f * 3.14159265358979f /
            (6.0f * static_cast<Float>(face_size) * static_cast<Float>(face_size));

        for (ui32 i = 0; i < kFaceCount; ++i) {
            constexpr ui32 kFaceRemap[kFaceCount] = {0, 1, 2, 3, 4, 5};
            constexpr ui32 kFaceTransform[kFaceCount] = {1, 1, 2, 2, 1, 1};
            const ui32 file = kFaceRemap[i];
            const ui32 w = faces[file].width;
            const ui32 h = faces[file].height;
            const Size_t rp = static_cast<Size_t>(w) * 4u * sizeof(Float);
            const Float* src = static_cast<const Float*>(faces[file].pixels);
            DynamicArray<Float> transformed(static_cast<Size_t>(w) * h * 4u);
            const ui32 mode = kFaceTransform[i];
            for (ui32 y = 0; y < h; ++y) {
                for (ui32 x = 0; x < w; ++x) {
                    ui32 sx = x;
                    ui32 sy = y;
                    if (mode == 1) { sx = w - 1u - x; }
                    else if (mode == 2) { sy = h - 1u - y; }
                    else if (mode == 3) { sx = w - 1u - x; sy = h - 1u - y; }
                    const Size_t si = (static_cast<Size_t>(sy) * w + sx) * 4u;
                    const Size_t di = (static_cast<Size_t>(y) * w + x) * 4u;
                    transformed[di] = src[si];
                    transformed[di + 1] = src[si + 1];
                    transformed[di + 2] = src[si + 2];
                    transformed[di + 3] = src[si + 3];
                }
            }
            cmd_list.writeTexture(cubemap, i, 0, transformed.data(), rp);

            for (ui32 y = 0; y < h; ++y) {
                for (ui32 x = 0; x < w; ++x) {
                    Float dir[3];
                    TexelDirection(i, x, y, w, dir);
                    Float basis[9];
                    EvaluateSHBasis9(dir, basis);
                    const Size_t si = (static_cast<Size_t>(y) * w + x) * 4u;
                    for (UInt32 b = 0; b < 9u; ++b) {
                        sh_accum[b][0] += transformed[si] * basis[b] * sh_texel_weight;
                        sh_accum[b][1] += transformed[si + 1] * basis[b] * sh_texel_weight;
                        sh_accum[b][2] += transformed[si + 2] * basis[b] * sh_texel_weight;
                    }
                }
            }

            filtered_faces[i] = std::move(transformed);
        }

        {
            constexpr Float kPi = 3.14159265358979f;
            const Float sh_conv[9] = {kPi, 2.0f * kPi / 3.0f, 2.0f * kPi / 3.0f, 2.0f * kPi / 3.0f,
                kPi / 4.0f, kPi / 4.0f, kPi / 4.0f, kPi / 4.0f, kPi / 4.0f};
            Vector4f sh_coeffs[9];
            for (UInt32 b = 0; b < 9u; ++b) {
                const Float scale = sh_conv[b] / kPi;
                sh_irradiance[b] = Vector4f(sh_accum[b][0] * scale, sh_accum[b][1] * scale,
                    sh_accum[b][2] * scale, 0.0f);
            }
        }

        UInt32 src_size = face_size;
        for (ui32 mip = 1; mip < mip_count; ++mip) {
            const ui32 dst_size = src_size >> 1;
            const Float roughness = static_cast<Float>(mip) / static_cast<Float>(mip_count - 1);
            DynamicArray<Float> cur_faces[6];
            for (ui32 face = 0; face < kFaceCount; ++face) {
                cur_faces[face].resize(static_cast<Size_t>(dst_size) * dst_size * 4u);
                PrefilterFaceGGX(cur_faces[face].data(), filtered_faces, src_size, face, dst_size, roughness);
                const Size_t dst_rp = static_cast<Size_t>(dst_size) * 4u * sizeof(Float);
                cmd_list.writeTexture(cubemap, face, mip, cur_faces[face].data(), dst_rp);
            }
            for (ui32 face = 0; face < kFaceCount; ++face) {
                filtered_faces[face] = std::move(cur_faces[face]);
            }
            src_size = dst_size;
        }

        auto texture = create_scope<TextureCubemap>(ResolveTextureRef(face_paths[0]));
        texture->setFaceSize(faces[0].width);
        texture->setGpuHandle(cubemap);
        texture->setIrradianceSH(sh_irradiance);
        TextureCubemap* texture_raw = texture.get();

        const InstanceID id = texture->getInstanceID();
        m_cubemap_cache.emplace(id, std::move(texture));
        m_cubemap_by_path[face_paths[0]] = id;
        DO_INFO("TextureManager: loaded cubemap '{}' ({}x{})", face_paths[0], faces[0].width, faces[0].height);
        return texture_raw;
    }

    Texture2D* TextureManager::resolveSlot(const UInt32 slot) const {
        if (slot < m_slot_lut.size()) {
            if (auto* obj = Object::FindObjectFromInstanceID(m_slot_lut[slot])) {
                return static_cast<Texture2D*>(obj);
            }
        }
        return m_fallback.get();
    }

} // dodoe
