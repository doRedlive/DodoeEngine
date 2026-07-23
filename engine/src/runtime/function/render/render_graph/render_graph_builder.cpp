// do@Redlive

#include "render_graph_builder.h"

#include "runtime/function/render/render_pipeline/render_pass.h"

namespace dodoe {

    RenderGraphTextureHandle RenderGraphBuilder::createTexture(const RenderGraphTextureDesc& desc, const String& name) {
        RenderGraphTextureHandle handle{};
        handle.index = registerTexture(desc, name);
        return handle;
    }

    RenderGraphBufferHandle RenderGraphBuilder::createBuffer(const RenderGraphBufferDesc& desc, const String& name) {
        RenderGraphBufferHandle handle{};
        handle.index = registerBuffer(desc, name);
        return handle;
    }

    RenderGraphTextureHandle RenderGraphBuilder::importTexture(const GfxTextureHandle& texture, const String& name) {
        RenderGraphTextureHandle handle{};
        handle.index = registerImportedTexture(texture, name);
        return handle;
    }

    RenderGraphBufferHandle RenderGraphBuilder::importBuffer(const GfxBufferHandle& buffer, const String& name) {
        RenderGraphBufferHandle handle{};
        handle.index = registerImportedBuffer(buffer, name);
        return handle;
    }

    RenderGraphTextureHandle RenderGraphBuilder::importBackBuffer(const String& name) {
        RenderGraphTextureHandle handle{};
        handle.index = registerBackBuffer(name);
        return handle;
    }

    void RenderGraphBuilder::beginViewSubgraph(const String& name) {
        m_current_subgraph = m_graph.addSubgraph(name);
    }

    void RenderGraphBuilder::endViewSubgraph() {
        m_current_subgraph = ~0u;
    }

    void RenderGraphBuilder::addPass(IRenderPass& render_pass, const RenderPassBuildContext& context) {
        render_pass.build(*this, context);
    }

    void RenderGraphBuilder::exportTexture(const RenderGraphTextureHandle handle, const GfxResourceStates final_state) {
        DO_ASSERT(handle.isValid(), "RenderGraphBuilder::exportTexture invalid handle");
        auto& resources = m_graph.getResources();
        DO_ASSERT(handle.index < resources.size(), "RenderGraphBuilder::exportTexture handle out of range");
        auto& resource = resources[handle.index];
        resource.is_exported = true;
        resource.export_final_state = final_state;
    }

    void RenderGraphBuilder::compile() {
        m_graph.compile();
    }

    void RenderGraphBuilder::execute(ThreadPool& pool, const RenderGraphExecuteContext& context, DrawCommandList& out_commands) {
        m_graph.execute(pool, context, out_commands);
    }

    void RenderGraphBuilder::reset() {
        m_graph.reset();
        m_blackboard.reset();
    }

    UInt32 RenderGraphBuilder::registerTexture(const RenderGraphTextureDesc& desc, const String& name) {
        return m_graph.addTextureResource(desc, name);
    }

    UInt32 RenderGraphBuilder::registerBuffer(const RenderGraphBufferDesc& desc, const String& name) {
        return m_graph.addBufferResource(desc, name);
    }

    UInt32 RenderGraphBuilder::registerImportedTexture(const GfxTextureHandle& texture, const String& name) {
        return m_graph.addImportedTextureResource(texture, name);
    }

    UInt32 RenderGraphBuilder::registerImportedBuffer(const GfxBufferHandle& buffer, const String& name) {
        return m_graph.addImportedBufferResource(buffer, name);
    }

    UInt32 RenderGraphBuilder::registerBackBuffer(const String& name) {
        return m_graph.addBackBufferResource(name);
    }

    RenderGraphTextureHandle RenderGraphPassBuilder::createTransientTexture(const RenderGraphTextureDesc& desc, const String& name) {
        DO_ASSERT(m_builder, "RenderGraphPassBuilder builder is null");
        return m_builder->createTexture(desc, name);
    }

    RenderGraphBufferHandle RenderGraphPassBuilder::createTransientBuffer(const RenderGraphBufferDesc& desc, const String& name) {
        DO_ASSERT(m_builder, "RenderGraphPassBuilder builder is null");
        return m_builder->createBuffer(desc, name);
    }

    RenderGraphTextureHandle RenderGraphPassBuilder::importTexture(const GfxTextureHandle& texture, const String& name) {
        DO_ASSERT(m_builder, "RenderGraphPassBuilder builder is null");
        return m_builder->importTexture(texture, name);
    }

    RenderGraphBufferHandle RenderGraphPassBuilder::importBuffer(const GfxBufferHandle& buffer, const String& name) {
        DO_ASSERT(m_builder, "RenderGraphPassBuilder builder is null");
        return m_builder->importBuffer(buffer, name);
    }

    RenderGraphTextureHandle RenderGraphPassBuilder::importBackBuffer(const String& name) {
        DO_ASSERT(m_builder, "RenderGraphPassBuilder builder is null");
        return m_builder->importBackBuffer(name);
    }

    RenderGraphTextureHandle RenderGraphPassBuilder::read(const RenderGraphTextureHandle handle) {
        DO_ASSERT(m_pass, "RenderGraphPassBuilder pass is null");
        DO_ASSERT(handle.isValid(), "RenderGraphPassBuilder::read invalid texture handle");
        m_pass->addAccess(handle.index, RenderGraphAccessType::Read);
        return handle;
    }

    RenderGraphTextureHandle RenderGraphPassBuilder::write(const RenderGraphTextureHandle handle) {
        DO_ASSERT(m_pass, "RenderGraphPassBuilder pass is null");
        DO_ASSERT(handle.isValid(), "RenderGraphPassBuilder::write invalid texture handle");
        m_pass->addAccess(handle.index, RenderGraphAccessType::Write);
        return handle;
    }

    RenderGraphBufferHandle RenderGraphPassBuilder::read(const RenderGraphBufferHandle handle) {
        DO_ASSERT(m_pass, "RenderGraphPassBuilder pass is null");
        DO_ASSERT(handle.isValid(), "RenderGraphPassBuilder::read invalid buffer handle");
        m_pass->addAccess(handle.index, RenderGraphAccessType::Read);
        return handle;
    }

    RenderGraphBufferHandle RenderGraphPassBuilder::write(const RenderGraphBufferHandle handle) {
        DO_ASSERT(m_pass, "RenderGraphPassBuilder pass is null");
        DO_ASSERT(handle.isValid(), "RenderGraphPassBuilder::write invalid buffer handle");
        m_pass->addAccess(handle.index, RenderGraphAccessType::Write);
        return handle;
    }

    RenderGraphBlackboard& RenderGraphPassBuilder::blackboard() const {
        DO_ASSERT(m_builder, "RenderGraphPassBuilder builder is null");
        return m_builder->blackboard();
    }

    RenderGraphTextureHandle RenderGraphPassBuilder::readTexture(
        const RenderGraphTextureHandle handle,
        const RenderGraphPipelineStage stage,
        const RenderGraphSubresourceRange& subresource)
    {
        DO_ASSERT(m_pass, "RenderGraphPassBuilder pass is null");
        DO_ASSERT(handle.isValid(), "RenderGraphPassBuilder::readTexture invalid handle");
        m_pass->addAccess(handle.index, RenderGraphAccessType::Read, stage, subresource);
        return handle;
    }

    RenderGraphTextureHandle RenderGraphPassBuilder::writeColor(
        const RenderGraphTextureHandle handle,
        const RenderGraphAttachmentInfo& attachment)
    {
        DO_ASSERT(m_pass, "RenderGraphPassBuilder pass is null");
        DO_ASSERT(handle.isValid(), "RenderGraphPassBuilder::writeColor invalid handle");
        m_pass->addAccess(handle.index, RenderGraphAccessType::Write, RenderGraphPipelineStage::RenderTarget);
        return handle;
    }

    RenderGraphTextureHandle RenderGraphPassBuilder::writeDepth(
        const RenderGraphTextureHandle handle,
        const RenderGraphAttachmentInfo& attachment)
    {
        DO_ASSERT(m_pass, "RenderGraphPassBuilder pass is null");
        DO_ASSERT(handle.isValid(), "RenderGraphPassBuilder::writeDepth invalid handle");
        m_pass->addAccess(handle.index, RenderGraphAccessType::Write, RenderGraphPipelineStage::DepthStencil);
        return handle;
    }

    RenderGraphTextureHandle RenderGraphPassBuilder::writeUav(
        const RenderGraphTextureHandle handle,
        const RenderGraphPipelineStage stage)
    {
        DO_ASSERT(m_pass, "RenderGraphPassBuilder pass is null");
        DO_ASSERT(handle.isValid(), "RenderGraphPassBuilder::writeUav invalid handle");
        m_pass->addAccess(handle.index, RenderGraphAccessType::ReadWrite, stage);
        return handle;
    }

    RenderGraphBufferHandle RenderGraphPassBuilder::readBuffer(
        const RenderGraphBufferHandle handle,
        const RenderGraphPipelineStage stage)
    {
        DO_ASSERT(m_pass, "RenderGraphPassBuilder pass is null");
        DO_ASSERT(handle.isValid(), "RenderGraphPassBuilder::readBuffer invalid handle");
        m_pass->addAccess(handle.index, RenderGraphAccessType::Read, stage);
        return handle;
    }

    RenderGraphBufferHandle RenderGraphPassBuilder::writeBuffer(
        const RenderGraphBufferHandle handle,
        const RenderGraphPipelineStage stage)
    {
        DO_ASSERT(m_pass, "RenderGraphPassBuilder pass is null");
        DO_ASSERT(handle.isValid(), "RenderGraphPassBuilder::writeBuffer invalid handle");
        m_pass->addAccess(handle.index, RenderGraphAccessType::Write, stage);
        return handle;
    }

    void RenderGraphPassBuilder::exportTexture(const RenderGraphTextureHandle handle, const GfxResourceStates final_state) {
        DO_ASSERT(m_builder, "RenderGraphPassBuilder builder is null");
        DO_ASSERT(handle.isValid(), "RenderGraphPassBuilder::exportTexture invalid handle");
        m_builder->exportTexture(handle, final_state);
    }

} // dodoe