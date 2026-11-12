#include <gtest/gtest.h>

#include "runtime/function/render/render_graph/render_graph_builder.h"

namespace {
    using namespace dodoe;
    struct Parameters {};
    const auto recordNothing = [](const Parameters&, const RenderGraphPassContext&, DrawCommandList&) {};

    TEST(RenderGraph, ImportedAliasesShareOneIdentity) {
        RenderGraphBuilder graph;
        const auto buffer = create_ref<GfxBuffer>(GfxBufferDesc().setByteSize(64));
        const auto texture = create_ref<GfxTexture>(GfxTextureDesc().setWidth(16).setHeight(16));
        EXPECT_EQ(graph.importBuffer(buffer, "A").index, graph.importBuffer(buffer, "B").index);
        EXPECT_EQ(graph.importTexture(texture, "A").index, graph.importTexture(texture, "B").index);
        EXPECT_EQ(graph.importBackBuffer("A").index, graph.importBackBuffer("B").index);
    }

    TEST(RenderGraph, SharedReadDoesNotKeepUnrelatedConsumerAlive) {
        RenderGraphBuilder graph;
        const auto shared = graph.importTexture(create_ref<GfxTexture>(GfxTextureDesc()), "Shared");
        const auto dead = graph.createTexture(MakeRenderTarget2D(16, 16, GfxFormat::RGBA8_UNORM, "Dead"), "Dead");
        graph.addPass<Parameters>("UnusedConsumer", RenderGraphPassFlags::Raster,
            [&](RenderGraphPassBuilder& builder, Parameters&) { builder.read(shared); builder.writeColor(dead); }, recordNothing);
        graph.addPass<Parameters>("Present", RenderGraphPassFlags::Raster,
            [&](RenderGraphPassBuilder& builder, Parameters&) {
                builder.read(shared);
                builder.writeColor(builder.importBackBuffer("Output"));
            }, recordNothing);
        graph.compile();
        ASSERT_EQ(graph.graph().getCulledPasses().size(), 2u);
        EXPECT_TRUE(graph.graph().getCulledPasses()[0]);
        EXPECT_FALSE(graph.graph().getCulledPasses()[1]);
        EXPECT_FALSE(graph.graph().getResources()[dead.index].is_live);
    }

    TEST(RenderGraph, NeverCullKeepsItsProducer) {
        RenderGraphBuilder graph;
        const auto input = graph.createTexture(MakeRenderTarget2D(16, 16, GfxFormat::RGBA8_UNORM, "Input"), "Input");
        graph.addPass<Parameters>("Producer", RenderGraphPassFlags::Raster,
            [&](RenderGraphPassBuilder& builder, Parameters&) { builder.writeColor(input); }, recordNothing);
        graph.addPass<Parameters>("Readback", RenderGraphPassFlags::NeverCull,
            [&](RenderGraphPassBuilder& builder, Parameters&) { builder.read(input); }, recordNothing);
        graph.compile();
        EXPECT_FALSE(graph.graph().getCulledPasses()[0]);
        EXPECT_FALSE(graph.graph().getCulledPasses()[1]);
        EXPECT_EQ(graph.graph().getLevels().size(), 2u);
    }

    TEST(RenderGraph, UploadOwnsBytesAndRasterUsesVertexState) {
        RenderGraphBuilder graph;
        RenderGraphBufferDesc desc{};
        desc.desc.setByteSize(64).setIsVertexBuffer(true);
        const auto buffer = graph.createBuffer(desc, "Instances");
        UInt32 value = 0x12345678;
        graph.addPass<Parameters>("Draw", RenderGraphPassFlags::Raster | RenderGraphPassFlags::NeverCull,
            [&](RenderGraphPassBuilder& builder, Parameters&) {
                builder.uploadBuffer(buffer, &value, sizeof(value), 16);
                builder.readBuffer(buffer, RenderGraphBufferUsage::Vertex);
            }, recordNothing);
        value = 0;
        graph.compile();
        const auto& pass = graph.graph().getPasses().front();
        ASSERT_EQ(pass->getUploads().size(), 1u);
        UInt32 saved = 0;
        std::memcpy(&saved, pass->getUploads().front().data.data(), sizeof(saved));
        EXPECT_EQ(saved, 0x12345678u);
        EXPECT_EQ(pass->getUploads().front().offset, 16u);
        ASSERT_EQ(pass->getPreBarriers().size(), 1u);
        EXPECT_EQ(pass->getPreBarriers().front().to_state, GfxResourceStates::VertexBuffer);
    }

    TEST(DrawCommandList, TransferPreservesPendingResourcesWithoutDraws) {
        DrawCommandList source;
        const auto resource = create_ref<GfxBuffer>(GfxBufferDesc().setByteSize(64));
        source.requestRealize(resource);
        source.requestRealize(resource);
        auto detached = source.detachRecordedCommands();
        EXPECT_TRUE(source.getRealizePlan().empty());
        DrawCommandList destination;
        destination.append(std::move(detached));
        ASSERT_EQ(destination.getRealizePlan().size(), 1u);
        EXPECT_EQ(destination.getRealizePlan().front().buffer.get(), resource.get());
        EXPECT_FALSE(resource->isGpuReady());
    }
}
