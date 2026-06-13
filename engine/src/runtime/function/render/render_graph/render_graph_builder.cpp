// do@Redlive

#include "render_graph_builder.h"

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

    void RenderGraphBuilder::compile() {
        m_graph.compile();
    }

    DrawCommandList RenderGraphBuilder::execute(ThreadPool& pool, const RenderGraphExecuteContext& context) {
        return m_graph.execute(pool, context);
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

    RenderGraphTextureHandle RenderGraphPassBuilder::createTransientTexture(const RenderGraphTextureDesc& desc, const String& name) {
        DO_ASSERT(m_builder, "RenderGraphPassBuilder builder is null");
        return m_builder->createTexture(desc, name);
    }

    RenderGraphBufferHandle RenderGraphPassBuilder::createTransientBuffer(const RenderGraphBufferDesc& desc, const String& name) {
        DO_ASSERT(m_builder, "RenderGraphPassBuilder builder is null");
        return m_builder->createBuffer(desc, name);
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

} // dodoe
