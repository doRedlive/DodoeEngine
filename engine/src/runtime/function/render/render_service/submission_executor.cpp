// do@Redlive

#include "submission_executor.h"

namespace dodoe {

    namespace {

        UnorderedSet<const void*>& FailedReports() {
            static UnorderedSet<const void*> s_failed_reports;
            return s_failed_reports;
        }

        Bool ShouldReportFailure(const void* key) {
            return FailedReports().insert(key).second;
        }

        void MarkRecovered(const void* key) {
            FailedReports().erase(key);
        }

        Bool RealizeTexture(const GfxResourceRealizeEntry& entry, GfxDeviceHandle device) {
            const auto& texture = entry.texture;
            if (!texture) return true;
            if (texture->isGpuReady()) {
                MarkRecovered(texture.get());
                return true;
            }
            texture->initializeGpu(device);
            if (!texture->isGpuReady() || !texture->getRHIHandle()) {
                if (ShouldReportFailure(texture.get())) {
                    DO_ERROR("SubmissionExecutor: texture realize failed ({})", string_to_std(texture->getDebugName()));
                }
                return false;
            }
            return true;
        }

        Bool RealizeBuffer(const GfxResourceRealizeEntry& entry, GfxDeviceHandle device) {
            const auto& buffer = entry.buffer;
            if (!buffer) return true;
            if (buffer->isGpuReady()) {
                MarkRecovered(buffer.get());
                return true;
            }
            buffer->initializeGpu(device);
            if (!buffer->isGpuReady() || !buffer->getRHIHandle()) {
                if (ShouldReportFailure(buffer.get())) {
                    DO_ERROR("SubmissionExecutor: buffer realize failed ({})", string_to_std(buffer->getDebugName()));
                }
                return false;
            }
            return true;
        }

        Bool RealizeFramebuffer(const GfxResourceRealizeEntry& entry, GfxDeviceHandle device) {
            const auto& framebuffer = entry.framebuffer;
            if (!framebuffer) return true;
            if (framebuffer->isGpuReady()) {
                MarkRecovered(framebuffer.get());
                return true;
            }

            const auto& desc = framebuffer->getDesc();
            for (const auto& color : desc.colors()) {
                if (!color || !color->isGpuReady()) {
                    if (ShouldReportFailure(framebuffer.get())) {
                        DO_ERROR("SubmissionExecutor: framebuffer attachment is not ready");
                    }
                    return false;
                }
            }
            if (desc.depth() && !desc.depth()->isGpuReady()) {
                if (ShouldReportFailure(framebuffer.get())) {
                    DO_ERROR("SubmissionExecutor: framebuffer depth attachment is not ready");
                }
                return false;
            }

            framebuffer->initializeGpu(device);
            if (!framebuffer->isGpuReady() || !framebuffer->getRHIHandle()) {
                if (ShouldReportFailure(framebuffer.get())) {
                    DO_ERROR("SubmissionExecutor: framebuffer creation failed");
                }
                return false;
            }
            return true;
        }

        Bool RealizeBindingSet(const GfxResourceRealizeEntry& entry, GfxDeviceHandle device) {
            const auto& binding_set = entry.binding_set;
            if (!binding_set) return true;
            if (binding_set->isGpuReady()) {
                MarkRecovered(binding_set.get());
                return true;
            }

            if (!binding_set->getLayout()) {
                if (ShouldReportFailure(binding_set.get())) {
                    DO_ERROR("SubmissionExecutor: binding set realize failed, layout is null");
                }
                return false;
            }

            const auto& logical_desc = binding_set->getLogicalDesc();
            for (const auto& item : logical_desc.bindings) {
                if (!item.isResolved(device)) {
                    if (ShouldReportFailure(binding_set.get())) {
                        DO_ERROR("SubmissionExecutor: binding set realize deferred, resource not ready (slot={})",
                            item.slot);
                    }
                    return false;
                }
            }

            binding_set->initializeGpu(device);
            if (!binding_set->isGpuReady() || !binding_set->getRHIHandle()) {
                if (ShouldReportFailure(binding_set.get())) {
                    DO_ERROR("SubmissionExecutor: binding set realize failed");
                }
                return false;
            }
            return true;
        }

        Bool RealizeGraphicsPipeline(const GfxResourceRealizeEntry& entry, GfxDeviceHandle device) {
            const auto& pipeline = entry.pipeline;
            if (!pipeline) return true;
            if (pipeline->isGpuReady()) {
                MarkRecovered(pipeline.get());
                return true;
            }

            pipeline->initializeGpu(device);
            if (!pipeline->isGpuReady() || !pipeline->getRHIHandle()) {
                if (ShouldReportFailure(pipeline.get())) {
                    DO_ERROR("SubmissionExecutor: graphics pipeline realize failed");
                }
                return false;
            }
            return true;
        }

    } // namespace

    Bool SubmissionExecutor::prepareResources(DrawCommandList& commands, GfxDeviceHandle device) {
        DO_PROFILE_SCOPE_CATEGORY("SubmissionExecutor::prepareResources", "resource");
        if (!device) {
            DO_ERROR("SubmissionExecutor::prepareResources: graphics device is unavailable");
            return false;
        }

        // Discover leaf dependencies even when their original creation list is not
        // part of this submission (for example, a cached pending binding set).
        UnorderedSet<const void*> visited;
        Bool ready = true;
        auto texture = [&](const GfxTextureHandle& handle) {
            if (!handle || !visited.insert(handle.get()).second) return;
            GfxResourceRealizeEntry entry{};
            entry.texture = handle;
            ready = RealizeTexture(entry, device) && ready;
        };
        auto buffer = [&](const GfxBufferHandle& handle) {
            if (!handle || !visited.insert(handle.get()).second) return;
            GfxResourceRealizeEntry entry{};
            entry.buffer = handle;
            ready = RealizeBuffer(entry, device) && ready;
        };
        for (const auto& entry : commands.getRealizePlan()) {
            texture(entry.texture);
            buffer(entry.buffer);
            if (entry.descriptor_write) texture(entry.descriptor_write->texture);
            if (entry.framebuffer) {
                const auto& desc = entry.framebuffer->getDesc();
                for (const auto& color : desc.colors()) texture(color);
                texture(desc.depth());
            }
            if (entry.binding_set) {
                for (const auto& item : entry.binding_set->getLogicalDesc().bindings) {
                    texture(item.texture);
                    buffer(item.buffer);
                }
            }
        }
        if (!ready) return false;

        // All buffer/texture dependencies exist before dependent objects are created.
        for (const auto& entry : commands.getRealizePlan()) {
            switch (entry.kind) {
                case GfxResourceRealizeEntry::Kind::Framebuffer:
                    ready = RealizeFramebuffer(entry, device) && ready;
                    break;
                case GfxResourceRealizeEntry::Kind::BindingSet:
                    ready = RealizeBindingSet(entry, device) && ready;
                    break;
                case GfxResourceRealizeEntry::Kind::GraphicsPipeline:
                    ready = RealizeGraphicsPipeline(entry, device) && ready;
                    break;
                case GfxResourceRealizeEntry::Kind::DescriptorWrite: {
                    const auto& write = entry.descriptor_write;
                    if (!write || !write->table || !write->texture || !write->texture->isGpuReady()) {
                        ready = false;
                        break;
                    }
                    auto item = GfxBindingSetItem::Texture_SRV(write->slot, write->texture->getRHIHandle());
                    if (!device->writeDescriptorTable(write->table, item)) {
                        DO_ERROR("SubmissionExecutor: descriptor write failed (slot={})", write->slot);
                        ready = false;
                    }
                    break;
                }
                default:
                    break;
            }
        }
        return ready;
    }

    void SubmissionExecutor::replay(DrawCommandList& commands, GfxCommandList& target) {
        DO_PROFILE_SCOPE_CATEGORY("SubmissionExecutor::replay", "render-command");
        commands.execute(target);
    }

    Bool SubmissionExecutor::prepareAndReplay(DrawCommandList& commands, GfxDeviceHandle device, GfxCommandList& target) {
        if (!prepareResources(commands, device)) {
            DO_ERROR("SubmissionExecutor: resource preparation failed; submission commands were not replayed");
            return false;
        }
        replay(commands, target);
        return true;
    }

} // namespace dodoe
