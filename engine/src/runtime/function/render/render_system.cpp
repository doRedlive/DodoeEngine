//
// Created by GreenMuffin on 2026/3/6.
// Knight!
//

#include "render_system.h"

#include "render_settings.h"
#include "runtime/resource/parser/texture_blob.h"
#include "runtime/resource/file/file_system.h"

namespace {
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

namespace dodoe {

    bool RenderSystem::initialize(const RenderSystemCreateInfo& info) {
        m_window_manager = info.window_manager;

        auto window = m_window_manager->getWindow();
        auto backend_api = RenderSettings::GetRenderBackendApiType();

        m_viewport_manager = ViewportManager::Create({window});
        const bool enable_validation =
#ifdef DO_DEBUG
            true;
#else
            false;
#endif
        m_gfx = GfxContext::Create({window->getNativeWindow(), backend_api, enable_validation});
        const auto camera_type = RenderSettings::GetRenderingPipelineType() == RenderingPipelineType::Only2D
            ? CameraType::Orthographic : CameraType::Perspective;
        m_camera = Camera::Create({camera_type, m_viewport_manager->getLogicalSize(), m_viewport_manager->getWindowSize()});
        m_descriptor_table = DescriptorTableManager::Create({m_gfx.get()});
        m_texture_manager = TextureManager::Create({m_gfx.get(), m_descriptor_table.get()});

        m_renderer = Renderer::Create({m_gfx->getDevice(), m_camera.get(), m_texture_manager.get()});
        if (!m_renderer) {
            return false;
        }
        createSkyboxTextureInternal();
        m_rendering_pipeline = RenderingPipeline::Create({
            std::thread::hardware_concurrency(),
            m_gfx.get(),
            m_descriptor_table.get(),
            m_texture_manager.get()
        });

        return m_camera && m_descriptor_table && m_texture_manager && m_rendering_pipeline && m_renderer;
    }

    void RenderSystem::shutdown() {
        if (m_gfx && m_gfx->getDevice()) {
            m_gfx->getDevice()->waitForIdle();
        }

        RenderingPipeline::Destroy(m_rendering_pipeline);
        Renderer::Destroy(m_renderer);
        Camera::Destroy(m_camera);
        TextureManager::Destroy(m_texture_manager);
        DescriptorTableManager::Destroy(m_descriptor_table);
        if (m_gfx && m_gfx->getDevice()) {
            m_gfx->getDevice()->waitForIdle();
            m_gfx->getDevice()->runGarbageCollection();
        }
        GfxContext::Destroy(m_gfx);
    }

    void RenderSystem::prepare() {
        m_viewport_manager->update();
        if (m_viewport_manager->isViewportDirty()) [[unlikely]] {
            m_camera->setViewportSize(m_viewport_manager->getLogicalSize(), m_viewport_manager->getWindowSize());
        }
        if (m_viewport_manager->isWindowDirty()) [[unlikely]] {
            if (!m_gfx->recreateSwapchain()) {
                return;
            }
            m_gfx->getDevice()->runGarbageCollection();
            m_camera->setViewportSize(m_viewport_manager->getLogicalSize(), m_viewport_manager->getWindowSize());
        }
        m_viewport_manager->clearDirtyFlags();

        swapLogicRenderContext();
    }

    void RenderSystem::present() {
        uint32_t image_index = 0;
        if (!m_gfx->acquireNextSwapchainImage(image_index)) {
            return;
        }

        if (!m_rendering_pipeline) {
            return;
        }

        auto upload_command_list = m_gfx->getDevice()->createCommandList();
        upload_command_list->open();
        m_renderer->prepareBuffers(upload_command_list);
        upload_command_list->close();
        m_gfx->getDevice()->executeCommandList(upload_command_list);

        RenderViewFamily view_family{};
        RenderView main_view(Identifier{});
        const auto viewport_size = m_viewport_manager->getPixelSize();
        main_view.setViewportRect(Vector4i(0, 0, viewport_size.x, viewport_size.y));
        main_view.setMatrices(m_camera->getViewMatrix(), m_camera->getProjectionMatrix());
        view_family.addView(main_view);

        auto frame_command_list = m_rendering_pipeline->render(view_family, m_renderer->getRenderScene(), image_index);
        frame_command_list.execute(m_gfx->getCommandList());
        m_gfx->getDevice()->executeCommandList(m_gfx->getCommandList());

        if (m_gfx->presentSwapchainImage(image_index)) {
            m_gfx->getDevice()->runGarbageCollection();
        }
    }

    void RenderSystem::swapLogicRenderContext() {
        std::scoped_lock lock(m_submit_mutex);
        if (m_logic_main_camera_dirty_ && m_renderer) {
            m_renderer->setMainCameraViewProjection(m_logic_main_camera_view_proj_, m_logic_main_camera_position_);
            m_logic_main_camera_dirty_ = false;
        }
    }

    gfx::TextureHandle RenderSystem::getSkyboxTexture() const {
        std::scoped_lock lock(m_submit_mutex);
        return m_renderer ? m_renderer->getSkyboxTexture() : nullptr;
    }

    void RenderSystem::createSkyboxTextureInternal() {
        std::scoped_lock lock(m_submit_mutex);
        if (!m_gfx || !m_gfx->getDevice()) {
            if (m_renderer) {
                m_renderer->setSkyboxTexture(nullptr);
            }
            return;
        }

        constexpr ui32 kSkyboxFaceCount = 6;
        constexpr const char* kSkyboxFacePaths[kSkyboxFaceCount] = {
            "pictures/Skybox/skybox_specular_Y-.hdr",
            "pictures/Skybox/skybox_specular_Y+.hdr",
            "pictures/Skybox/skybox_specular_Z+.hdr",
            "pictures/Skybox/skybox_specular_Z-.hdr",
            "pictures/Skybox/skybox_specular_X+.hdr",
            "pictures/Skybox/skybox_specular_X-.hdr",
        };

        std::array<TextureBlob, kSkyboxFaceCount> faces{};
        for (ui32 face_index = 0; face_index < kSkyboxFaceCount; ++face_index) {
            const auto face_path = FileSystem::relative2absolute(kSkyboxFacePaths[face_index]);
            faces[face_index].load(face_path, false);
            if (!faces[face_index].isValid() || faces[face_index].width != faces[face_index].height) {
                if (m_renderer) {
                    m_renderer->setSkyboxTexture(nullptr);
                }
                return;
            }
        }

        auto texture_desc = gfx::TextureDesc()
            .setDimension(gfx::TextureDimension::TextureCube)
            .setWidth(faces[0].width)
            .setHeight(faces[0].height)
            .setArraySize(kSkyboxFaceCount)
            .setMipLevels(1)
            .setFormat(gfx::Format::RGBA32_FLOAT)
            .enableAutomaticStateTracking(gfx::ResourceStates::ShaderResource)
            .setDebugName("RenderSystem Skybox Cubemap");
        auto skybox_texture = m_gfx->getDevice()->createTexture(texture_desc);
        if (!skybox_texture) {
            if (m_renderer) {
                m_renderer->setSkyboxTexture(nullptr);
            }
            return;
        }

        auto cmd = m_gfx->getDevice()->createCommandList();
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
            cmd->writeTexture(skybox_texture, face_index, 0, upload_pixels, row_pitch);
        }
        cmd->close();
        m_gfx->getDevice()->executeCommandList(cmd);
        if (m_renderer) {
            m_renderer->setSkyboxTexture(skybox_texture);
        }
    }

} // dodoe
