// do@Redlive

#include "ui_render_batch.h"
#include "runtime/function/render/render_command_queue.h"
#include <algorithm>

namespace dodoe {

    void UIRenderBatch::submit() {
        if (!m_infos.empty()) {
            std::sort(m_infos.begin(), m_infos.end(),
                [](const UISceneInfo& a, const UISceneInfo& b) {
                    return a.getDepth() < b.getDepth();
                });
            RenderCommandQueue::SubmitUI(std::move(m_infos));
            m_infos.clear();
        }
    }

} // namespace dodoe
