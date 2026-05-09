//
// Created by Redlive on 2026/3/25.
//

#include "render_resource.h"

#include "runtime/core/utils/common.h"
#include "runtime/resource/asset/texture_loader.h"
#include "runtime/resource/file/file_system.h"

namespace dodoe {
    namespace {
        constexpr ui32 kSkyboxFaceCount = 6;
        constexpr const char* kSkyboxFacePaths[kSkyboxFaceCount] = {
            "pictures/Skybox/skybox_specular_Y-.hdr", // +X (right)
            "pictures/Skybox/skybox_specular_Y+.hdr", // -X (left)
            "pictures/Skybox/skybox_specular_Z+.hdr", // +Y (up)
            "pictures/Skybox/skybox_specular_Z-.hdr", // -Y (down)
            "pictures/Skybox/skybox_specular_X+.hdr", // +Z (front)
            "pictures/Skybox/skybox_specular_X-.hdr", // -Z (back)
        };

        std::vector<float> rotateFace90Clockwise(const float* src, const int width, const int height) {
            std::vector<float> dst(static_cast<size_t>(width) * static_cast<size_t>(height) * 4u);
            for (int y = 0; y < height; ++y) {
                for (int x = 0; x < width; ++x) {
                    const int src_x = y;
                    const int src_y = height - 1 - x;
                    const size_t src_index = (static_cast<size_t>(src_y) * static_cast<size_t>(width) + static_cast<size_t>(src_x)) * 4u;
                    const size_t dst_index = (static_cast<size_t>(y) * static_cast<size_t>(width) + static_cast<size_t>(x)) * 4u;
                    dst[dst_index + 0] = src[src_index + 0];
                    dst[dst_index + 1] = src[src_index + 1];
                    dst[dst_index + 2] = src[src_index + 2];
                    dst[dst_index + 3] = src[src_index + 3];
                }
            }
            return dst;
        }

        std::vector<float> rotateFace90CounterClockwise(const float* src, const int width, const int height) {
            std::vector<float> dst(static_cast<size_t>(width) * static_cast<size_t>(height) * 4u);
            for (int y = 0; y < height; ++y) {
                for (int x = 0; x < width; ++x) {
                    const int src_x = width - 1 - y;
                    const int src_y = x;
                    const size_t src_index = (static_cast<size_t>(src_y) * static_cast<size_t>(width) + static_cast<size_t>(src_x)) * 4u;
                    const size_t dst_index = (static_cast<size_t>(y) * static_cast<size_t>(width) + static_cast<size_t>(x)) * 4u;
                    dst[dst_index + 0] = src[src_index + 0];
                    dst[dst_index + 1] = src[src_index + 1];
                    dst[dst_index + 2] = src[src_index + 2];
                    dst[dst_index + 3] = src[src_index + 3];
                }
            }
            return dst;
        }
    }

    RenderResource* g_RenderResource = new RenderResource();

    void RenderResource::initilize(rhi::DeviceHandle device) {
        device_ = device;
        render_scene_.initialize(device);
        createSkyboxTextureInternal();
    }

    void RenderResource::shutdown() {
        std::scoped_lock lock(submit_mutex_);
        logic_main_camera_dirty_ = false;
        logic_main_camera_view_proj_ = Matrix4f(1.0f);
        logic_main_camera_position_ = Vector3f(0.0f);
        skybox_texture_ = nullptr;
        render_scene_.reset();
        device_ = nullptr;
    }

    void RenderResource::submitMainCameraViewProjection(const Matrix4f& view_proj_matrix, const Vector3f& position) {
        std::scoped_lock lock(submit_mutex_);
        logic_main_camera_view_proj_ = view_proj_matrix;
        logic_main_camera_position_ = position;
        logic_main_camera_dirty_ = true;
    }

    void RenderResource::swapLogicRenderContext() {
        std::scoped_lock lock(submit_mutex_);
        if (logic_main_camera_dirty_) {
            render_scene_.setMainCameraViewProjection(logic_main_camera_view_proj_, logic_main_camera_position_);
            logic_main_camera_dirty_ = false;
        }
    }

    const RenderScene& RenderResource::renderScene() const {
        return render_scene_;
    }

    RenderScene& RenderResource::getRenderScene() {
        return render_scene_;
    }

    void RenderResource::rebuildSkyboxTexture() {
        createSkyboxTextureInternal();
    }

    void RenderResource::createSkyboxTextureInternal() {
        std::scoped_lock lock(submit_mutex_);
        if (!device_) {
            skybox_texture_ = nullptr;
            return;
        }

        std::array<TextureBlob, kSkyboxFaceCount> faces{};
        for (ui32 face_index = 0; face_index < kSkyboxFaceCount; ++face_index) {
            const auto face_path = FileSystem::relative2absolute(kSkyboxFacePaths[face_index]);
            faces[face_index].load(face_path, false);
            if (!faces[face_index].isValid()) {
                DO_ERROR("RenderResource: load skybox face {} failed.", face_path);
                skybox_texture_ = nullptr;
                return;
            }
            if (faces[face_index].width != faces[face_index].height) {
                DO_ERROR("RenderResource: skybox face {} is not square.", face_path);
                skybox_texture_ = nullptr;
                return;
            }
        }

        auto texture_desc = rhi::TextureDesc()
            .setDimension(rhi::TextureDimension::TextureCube)
            .setWidth(faces[0].width)
            .setHeight(faces[0].height)
            .setArraySize(kSkyboxFaceCount)
            .setMipLevels(1)
            .setFormat(rhi::Format::RGBA32_FLOAT)
            .enableAutomaticStateTracking(rhi::ResourceStates::ShaderResource)
            .setDebugName("RenderResource Skybox Cubemap");
        skybox_texture_ = device_->createTexture(texture_desc);
        if (!skybox_texture_) {
            DO_ERROR("RenderResource: create skybox cubemap failed.");
            return;
        }

        auto cmd = device_->createCommandList();
        cmd->open();
        std::vector<float> top_face_rotated_pixels{};
        std::vector<float> bottom_face_rotated_pixels{};
        for (ui32 face_index = 0; face_index < kSkyboxFaceCount; ++face_index) {
            const size_t row_pitch = static_cast<size_t>(faces[face_index].width) * 4u * sizeof(float);
            const void* upload_pixels = faces[face_index].pixels;
            if (face_index == 2) {
                top_face_rotated_pixels = rotateFace90Clockwise(
                    static_cast<const float*>(faces[face_index].pixels),
                    faces[face_index].width,
                    faces[face_index].height);
                upload_pixels = top_face_rotated_pixels.data();
            } else if (face_index == 3) {
                bottom_face_rotated_pixels = rotateFace90CounterClockwise(
                    static_cast<const float*>(faces[face_index].pixels),
                    faces[face_index].width,
                    faces[face_index].height);
                upload_pixels = bottom_face_rotated_pixels.data();
            }
            cmd->writeTexture(skybox_texture_, face_index, 0, upload_pixels, row_pitch);
        }
        cmd->close();
        device_->executeCommandList(cmd);
    }

    rhi::TextureHandle RenderResource::getSkyboxTexture() const {
        std::scoped_lock lock(submit_mutex_);
        return skybox_texture_;
    }

} // dodoe
