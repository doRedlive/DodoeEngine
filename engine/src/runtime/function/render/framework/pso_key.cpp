// do@Redlive

#include "pso_key.h"

namespace dodoe {
    namespace {

        Bool EqualStencilOpDesc(const GfxDepthStencilState::StencilOpDesc& lhs, const GfxDepthStencilState::StencilOpDesc& rhs) {
            return lhs.failOp == rhs.failOp &&
                   lhs.depthFailOp == rhs.depthFailOp &&
                   lhs.passOp == rhs.passOp &&
                   lhs.stencilFunc == rhs.stencilFunc;
        }

        Bool EqualRasterState(const GfxRasterState& lhs, const GfxRasterState& rhs) {
            return lhs.fillMode == rhs.fillMode &&
                   lhs.cullMode == rhs.cullMode &&
                   lhs.frontCounterClockwise == rhs.frontCounterClockwise &&
                   lhs.depthClipEnable == rhs.depthClipEnable &&
                   lhs.scissorEnable == rhs.scissorEnable &&
                   lhs.multisampleEnable == rhs.multisampleEnable &&
                   lhs.antialiasedLineEnable == rhs.antialiasedLineEnable &&
                   lhs.depthBias == rhs.depthBias &&
                   lhs.depthBiasClamp == rhs.depthBiasClamp &&
                   lhs.slopeScaledDepthBias == rhs.slopeScaledDepthBias &&
                   lhs.forcedSampleCount == rhs.forcedSampleCount &&
                   lhs.programmableSamplePositionsEnable == rhs.programmableSamplePositionsEnable &&
                   lhs.conservativeRasterEnable == rhs.conservativeRasterEnable &&
                   lhs.quadFillEnable == rhs.quadFillEnable &&
                   std::memcmp(lhs.samplePositionsX, rhs.samplePositionsX, sizeof(lhs.samplePositionsX)) == 0 &&
                   std::memcmp(lhs.samplePositionsY, rhs.samplePositionsY, sizeof(lhs.samplePositionsY)) == 0;
        }

        Bool EqualDepthStencilState(const GfxDepthStencilState& lhs, const GfxDepthStencilState& rhs) {
            return lhs.depthTestEnable == rhs.depthTestEnable &&
                   lhs.depthWriteEnable == rhs.depthWriteEnable &&
                   lhs.depthFunc == rhs.depthFunc &&
                   lhs.stencilEnable == rhs.stencilEnable &&
                   lhs.stencilReadMask == rhs.stencilReadMask &&
                   lhs.stencilWriteMask == rhs.stencilWriteMask &&
                   lhs.stencilRefValue == rhs.stencilRefValue &&
                   lhs.dynamicStencilRef == rhs.dynamicStencilRef &&
                   EqualStencilOpDesc(lhs.frontFaceStencil, rhs.frontFaceStencil) &&
                   EqualStencilOpDesc(lhs.backFaceStencil, rhs.backFaceStencil);
        }

        Bool EqualRenderState(const GfxRenderState& lhs, const GfxRenderState& rhs) {
            return lhs.blendState == rhs.blendState &&
                   EqualDepthStencilState(lhs.depthStencilState, rhs.depthStencilState) &&
                   EqualRasterState(lhs.rasterState, rhs.rasterState);
        }

        Bool EqualShadingRateState(const GfxVariableRateShadingState& lhs, const GfxVariableRateShadingState& rhs) {
            return lhs.enabled == rhs.enabled &&
                   lhs.shadingRate == rhs.shadingRate &&
                   lhs.pipelinePrimitiveCombiner == rhs.pipelinePrimitiveCombiner &&
                   lhs.imageCombiner == rhs.imageCombiner;
        }

        Size_t HashRasterState(const GfxRasterState& raster_state) {
            Size_t hash_value = 0;
            hash_combine(hash_value, static_cast<UInt32>(raster_state.fillMode));
            hash_combine(hash_value, static_cast<UInt32>(raster_state.cullMode));
            hash_combine(hash_value, raster_state.frontCounterClockwise);
            hash_combine(hash_value, raster_state.depthClipEnable);
            hash_combine(hash_value, raster_state.scissorEnable);
            hash_combine(hash_value, raster_state.multisampleEnable);
            hash_combine(hash_value, raster_state.antialiasedLineEnable);
            hash_combine(hash_value, raster_state.depthBias);
            hash_combine(hash_value, std::bit_cast<UInt32>(raster_state.depthBiasClamp));
            hash_combine(hash_value, std::bit_cast<UInt32>(raster_state.slopeScaledDepthBias));
            hash_combine(hash_value, raster_state.forcedSampleCount);
            hash_combine(hash_value, raster_state.programmableSamplePositionsEnable);
            hash_combine(hash_value, raster_state.conservativeRasterEnable);
            hash_combine(hash_value, raster_state.quadFillEnable);
            for (const auto sample_position : raster_state.samplePositionsX) {
                hash_combine(hash_value, sample_position);
            }
            for (const auto sample_position : raster_state.samplePositionsY) {
                hash_combine(hash_value, sample_position);
            }
            return hash_value;
        }

        Size_t HashDepthStencilState(const GfxDepthStencilState& depth_stencil_state) {
            Size_t hash_value = 0;
            hash_combine(hash_value, depth_stencil_state.depthTestEnable);
            hash_combine(hash_value, depth_stencil_state.depthWriteEnable);
            hash_combine(hash_value, static_cast<UInt32>(depth_stencil_state.depthFunc));
            hash_combine(hash_value, depth_stencil_state.stencilEnable);
            hash_combine(hash_value, depth_stencil_state.stencilReadMask);
            hash_combine(hash_value, depth_stencil_state.stencilWriteMask);
            hash_combine(hash_value, depth_stencil_state.stencilRefValue);
            hash_combine(hash_value, depth_stencil_state.dynamicStencilRef);
            hash_combine(hash_value, static_cast<UInt32>(depth_stencil_state.frontFaceStencil.failOp));
            hash_combine(hash_value, static_cast<UInt32>(depth_stencil_state.frontFaceStencil.depthFailOp));
            hash_combine(hash_value, static_cast<UInt32>(depth_stencil_state.frontFaceStencil.passOp));
            hash_combine(hash_value, static_cast<UInt32>(depth_stencil_state.frontFaceStencil.stencilFunc));
            hash_combine(hash_value, static_cast<UInt32>(depth_stencil_state.backFaceStencil.failOp));
            hash_combine(hash_value, static_cast<UInt32>(depth_stencil_state.backFaceStencil.depthFailOp));
            hash_combine(hash_value, static_cast<UInt32>(depth_stencil_state.backFaceStencil.passOp));
            hash_combine(hash_value, static_cast<UInt32>(depth_stencil_state.backFaceStencil.stencilFunc));
            return hash_value;
        }

        Size_t HashRenderState(const GfxRenderState& render_state) {
            Size_t hash_value = 0;
            hash_combine(hash_value, render_state.blendState.alphaToCoverageEnable);
            for (const auto& target : render_state.blendState.targets) {
                hash_combine(hash_value, target.blendEnable);
                hash_combine(hash_value, static_cast<UInt32>(target.srcBlend));
                hash_combine(hash_value, static_cast<UInt32>(target.destBlend));
                hash_combine(hash_value, static_cast<UInt32>(target.blendOp));
                hash_combine(hash_value, static_cast<UInt32>(target.srcBlendAlpha));
                hash_combine(hash_value, static_cast<UInt32>(target.destBlendAlpha));
                hash_combine(hash_value, static_cast<UInt32>(target.blendOpAlpha));
                hash_combine(hash_value, static_cast<UInt32>(target.colorWriteMask));
            }
            hash_combine(hash_value, HashDepthStencilState(render_state.depthStencilState));
            hash_combine(hash_value, HashRasterState(render_state.rasterState));
            return hash_value;
        }

        Size_t HashShadingRateState(const GfxVariableRateShadingState& shading_rate_state) {
            Size_t hash_value = 0;
            hash_combine(hash_value, shading_rate_state.enabled);
            hash_combine(hash_value, static_cast<UInt32>(shading_rate_state.shadingRate));
            hash_combine(hash_value, static_cast<UInt32>(shading_rate_state.pipelinePrimitiveCombiner));
            hash_combine(hash_value, static_cast<UInt32>(shading_rate_state.imageCombiner));
            return hash_value;
        }

        Size_t HashFramebufferInfo(const GfxFramebufferInfo& framebuffer_info) {
            Size_t hash_value = 0;
            const auto& rhi = framebuffer_info.getRHI();
            for (const auto format : rhi.colorFormats) {
                hash_combine(hash_value, static_cast<UInt32>(format));
            }
            hash_combine(hash_value, static_cast<UInt32>(rhi.depthFormat));
            hash_combine(hash_value, rhi.sampleCount);
            hash_combine(hash_value, rhi.sampleQuality);
            return hash_value;
        }

    } // namespace

    Bool GraphicsPipelineCacheKey::operator==(const GraphicsPipelineCacheKey& other) const {
        if (pass_type != other.pass_type ||
            primitive_type != other.primitive_type ||
            patch_control_points != other.patch_control_points ||
            input_layout != other.input_layout ||
            vertex_shader != other.vertex_shader ||
            hull_shader != other.hull_shader ||
            domain_shader != other.domain_shader ||
            geometry_shader != other.geometry_shader ||
            pixel_shader != other.pixel_shader ||
            binding_layouts.size() != other.binding_layouts.size() ||
            !EqualRenderState(render_state, other.render_state) ||
            !EqualShadingRateState(shading_rate_state, other.shading_rate_state) ||
            framebuffer_info.getRHI() != other.framebuffer_info.getRHI()) {
            return false;
        }

        for (Size_t index = 0; index < binding_layouts.size(); index++) {
            if (binding_layouts[index] != other.binding_layouts[index]) {
                return false;
            }
        }

        return true;
    }

    Size_t GraphicsPipelineCacheKeyHash::operator()(const GraphicsPipelineCacheKey& key) const {
        Size_t hash_value = 0;
        hash_combine(hash_value, static_cast<UInt32>(key.pass_type));
        hash_combine(hash_value, static_cast<UInt32>(key.primitive_type));
        hash_combine(hash_value, key.patch_control_points);
        hash_combine(hash_value, reinterpret_cast<Size_t>(key.input_layout));
        hash_combine(hash_value, reinterpret_cast<Size_t>(key.vertex_shader));
        hash_combine(hash_value, reinterpret_cast<Size_t>(key.hull_shader));
        hash_combine(hash_value, reinterpret_cast<Size_t>(key.domain_shader));
        hash_combine(hash_value, reinterpret_cast<Size_t>(key.geometry_shader));
        hash_combine(hash_value, reinterpret_cast<Size_t>(key.pixel_shader));
        for (const auto* binding_layout : key.binding_layouts) {
            hash_combine(hash_value, reinterpret_cast<Size_t>(binding_layout));
        }
        hash_combine(hash_value, HashRenderState(key.render_state));
        hash_combine(hash_value, HashShadingRateState(key.shading_rate_state));
        hash_combine(hash_value, HashFramebufferInfo(key.framebuffer_info));
        return hash_value;
    }

} // dodoe
