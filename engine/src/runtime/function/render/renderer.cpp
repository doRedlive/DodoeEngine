//
// Created by Redlive 2026/3/17.
//

#include "renderer.h"

#include "runtime/function/render/backend/draw_defs.h"
#include "render_api.h"

#include "backend/opengl/gl_renderer.h"
#include "backend/vulkan/vk_renderer.h"

namespace dodoe {

    Scope<Renderer> Renderer::create(RendererCreateInfo create_info) {
        auto context = create_scope<Renderer>();
        context->initialize(create_info);
        return context;
    }

    void Renderer::destroy(Scope<Renderer>& renderer) {
        if (!renderer) {
            return;
        }

        renderer->shutdown();
        renderer.reset();
    }

    void Renderer::initialize(RendererCreateInfo create_info) {
        DoAssert(create_info.render2d_graph, "RendererCreateInfo::render2d_graph must not be null.");
        render2d_graph_ = create_info.render2d_graph;
    }

    void Renderer::shutdown() {

    }

    void Renderer::draw_sprite(const Ref<Texture>& texture, const Vector2f& pos, 
            const Vector2f& size, const Vector3f& rotation, const Vector4f& color) {
        DoAssert(render2d_graph_ && render2d_graph_->sprite_stage, 
            "Renderer::draw_sprite: renderer2d_graph and sprite_stage must not be null");

        if (!texture) {
            DoError("Renderer::draw_sprite: texture is null.");
            return;
        }
        
        TextureDrawContext draw_context;
        draw_context.texture = texture;
        draw_context.dst_rect = Vector4f(pos.x, pos.y, size.x, size.y);
        draw_context.uv_rect  = Vector4f(0.0f, 0.0f, 1.0f, 1.0f);
        draw_context.rotation = rotation;
        draw_context.color = color;

        render2d_graph_->sprite_stage->queue_draw_context(draw_context);
    }

    void Renderer::draw_line() {

    }

    void Renderer::draw_text() {

    }

} // dodoe