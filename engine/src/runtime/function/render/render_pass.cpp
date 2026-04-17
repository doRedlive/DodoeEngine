// Created by Redlive on 2026/4/6.

#include "render_pass.h"

#include "render_graph.h"

namespace dodoe {

	void RenderPass::bind(RenderGraphPass& pass, RenderGraph& graph) {
		m_pass = &pass;
		m_graph = &graph;
	}

	RenderGraph& RenderPass::graph() {
		return *m_graph;
	}

	const RenderGraph& RenderPass::graph() const {
		return *m_graph;
	}

	rhi::TextureHandle RenderPass::getTextureResource(const std::string& name) const {
		return m_graph->getTextureResource(name);
	}

	rhi::BufferHandle RenderPass::getBufferResource(const std::string& name) const {
		return m_graph->getBufferResource(name);
	}

	RenderGraphPass::RenderGraphPass(std::string name, Ref<RenderPass> pass_implementation)
		: m_name(std::move(name)), m_implementation(std::move(pass_implementation)) {
	}

	void RenderGraphPass::bind(RenderGraph& graph) {
		if (m_implementation) {
			m_implementation->bind(*this, graph);
		}
	}

	bool RenderGraphPass::isNeedRenderPass() const {
		return m_implementation ? m_implementation->isNeedRenderPass() : true;
	}

	void RenderGraphPass::setup() {
		if (m_implementation) {
			m_implementation->setup();
		}
	}

	void RenderGraphPass::execute(const size_t index) {
		if (m_implementation && isNeedRenderPass()) {
			m_implementation->execute(index);
		}
	}

	void RenderGraphPass::cleanup() {
		if (m_implementation) {
			m_implementation->cleanup();
		}
	}

	void RenderGraphPass::onViewportResize(const Vector2i& viewport_extent) {
		if (m_implementation) {
			m_implementation->onViewportResize(viewport_extent);
		}
	}

	void RenderGraphPass::onWindowResize(const Vector2i& window_extent) {
		if (m_implementation) {
			m_implementation->onWindowResize(window_extent);
		}
	}

	RenderGraphPass& RenderGraphPass::addTextureRead(const std::string& name, const TextureResourceDesc& desc) {
		m_read_resources.push_back({name, RenderGraphResourceKind::Texture, desc, {}});
		return *this;
	}

	RenderGraphPass& RenderGraphPass::addTextureWrite(const std::string& name, const TextureResourceDesc& desc) {
		m_write_resources.push_back({name, RenderGraphResourceKind::Texture, desc, {}});
		return *this;
	}

	RenderGraphPass& RenderGraphPass::addBufferRead(const std::string& name, const BufferResourceDesc& desc) {
		m_read_resources.push_back({name, RenderGraphResourceKind::Buffer, {}, desc});
		return *this;
	}

	RenderGraphPass& RenderGraphPass::addBufferWrite(const std::string& name, const BufferResourceDesc& desc) {
		m_write_resources.push_back({name, RenderGraphResourceKind::Buffer, {}, desc});
		return *this;
	}

	void RenderGraphPass::clearResourceUsages() {
		m_read_resources.clear();
		m_write_resources.clear();
	}

} // dodoe
