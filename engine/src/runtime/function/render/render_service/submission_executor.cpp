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
                MarkRecovered(texture.Get());
                return true;
            }
            texture->initializeGpu(device);
            if (!texture->isGpuReady() || !texture->getRHIHandle()) {
                if (ShouldReportFailure(texture.Get())) {
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
                MarkRecovered(buffer.Get());
                return true;
            }
            buffer->initializeGpu(device);
            if (!buffer->isGpuReady() || !buffer->getRHIHandle()) {
                if (ShouldReportFailure(buffer.Get())) {
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
                MarkRecovered(framebuffer.Get());
                return true;
            }

            const auto& desc = framebuffer->getDesc();
            for (const auto& color : desc.colors()) {
                if (!color || !color->isGpuReady()) {
                    if (ShouldReportFailure(framebuffer.Get())) {
                        DO_ERROR("SubmissionExecutor: framebuffer realize deferred, attachment not ready ({})",
                            string_to_std(framebuffer->getDebugName()));
                    }
                    return false;
                }
            }
            if (desc.depth() && !desc.depth()->isGpuReady()) {
                if (ShouldReportFailure(framebuffer.Get())) {
                    DO_ERROR("SubmissionExecutor: framebuffer realize deferred, depth attachment not ready ({})",
                        string_to_std(framebuffer->getDebugName()));
                }
                return false;
            }

            framebuffer->initializeGpu(device);
            if (!framebuffer->isGpuReady() || !framebuffer->getRHIHandle()) {
                if (ShouldReportFailure(framebuffer.Get())) {
                    DO_ERROR("SubmissionExecutor: framebuffer realize failed ({})",
                        string_to_std(framebuffer->getDebugName()));
                }
                return false;
            }
            return true;
        }

        Bool RealizeBindingSet(const GfxResourceRealizeEntry& entry, GfxDeviceHandle device) {
            const auto& binding_set = entry.binding_set;
            if (!binding_set) return true;
            if (binding_set->isGpuReady()) {
                MarkRecovered(binding_set.Get());
                return true;
            }

            if (!binding_set->getLayout()) {
                if (ShouldReportFailure(binding_set.Get())) {
                    DO_ERROR("SubmissionExecutor: binding set realize failed, layout is null");
                }
                return false;
            }

            const auto& logical_desc = binding_set->getLogicalDesc();
            for (const auto& item : logical_desc.bindings) {
                if (!item.isResolved(device)) {
                    if (ShouldReportFailure(binding_set.Get())) {
                        DO_ERROR("SubmissionExecutor: binding set realize deferred, resource not ready (slot={})",
                            item.slot);
                    }
                    return false;
                }
            }

            binding_set->initializeGpu(device);
            if (!binding_set->isGpuReady() || !binding_set->getRHIHandle()) {
                if (ShouldReportFailure(binding_set.Get())) {
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
                MarkRecovered(pipeline.Get());
                return true;
            }

            pipeline->initializeGpu(device);
            if (!pipeline->isGpuReady() || !pipeline->getRHIHandle()) {
                if (ShouldReportFailure(pipeline.Get())) {
                    DO_ERROR("SubmissionExecutor: graphics pipeline realize failed");
                }
                return false;
            }
            return true;
        }

    } // namespace

    void SubmissionExecutor::prepareResources(DrawCommandList& commands, GfxDeviceHandle device) {
        DO_PROFILE_SCOPE_CATEGORY("SubmissionExecutor::prepareResources", "resource");
        if (!device) {
            DO_ERROR("SubmissionExecutor::prepareResources: graphics device is unavailable");
            return;
        }

        constexpr UInt32 kMaxRealizePasses = 2;
        for (UInt32 pass = 0; pass < kMaxRealizePasses; ++pass) {
            for (const auto& entry : commands.getRealizePlan()) {
                switch (entry.kind) {
                    case GfxResourceRealizeEntry::Kind::Texture:
                        RealizeTexture(entry, device);
                        break;
                    case GfxResourceRealizeEntry::Kind::Buffer:
                        RealizeBuffer(entry, device);
                        break;
                    case GfxResourceRealizeEntry::Kind::Framebuffer:
                        RealizeFramebuffer(entry, device);
                        break;
                    case GfxResourceRealizeEntry::Kind::BindingSet:
                        RealizeBindingSet(entry, device);
                        break;
                    case GfxResourceRealizeEntry::Kind::GraphicsPipeline:
                        RealizeGraphicsPipeline(entry, device);
                        break;
                    default:
                        break;
                }
            }
        }
    }

    void SubmissionExecutor::replay(DrawCommandList& commands, GfxCommandList& target) {
        DO_PROFILE_SCOPE_CATEGORY("SubmissionExecutor::replay", "render-command");
        commands.execute(target);
    }

    void SubmissionExecutor::prepareAndReplay(DrawCommandList& commands, GfxDeviceHandle device, GfxCommandList& target) {
        prepareResources(commands, device);
        replay(commands, target);
    }

} // namespace dodoe
