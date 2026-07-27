// do@Redlive

#pragma once

#include "dopch.h"

#include "runtime/function/render/render_scene/ui_scene_info.h"

namespace dodoe {

    class UIRenderBatch {
    private:
        DynamicArray<UISceneInfo> m_infos{};
        DynamicArray<Rect> m_clip_stack{};

    public:
        void addQuad(UISceneInfo info) {
            if (!m_clip_stack.empty()) {
                info.setClipRect(m_clip_stack.back());
            }
            m_infos.push_back(std::move(info));
        }

        void addNineSlice(UISceneInfo info, const NineSliceMargins& margins) {
            (void)margins;
            addQuad(info);
        }

        void pushClipRect(const Rect& clip) { m_clip_stack.push_back(clip); }
        void popClipRect() { if (!m_clip_stack.empty()) m_clip_stack.pop_back(); }

        void clear() { m_infos.clear(); m_clip_stack.clear(); }

        [[nodiscard]] const DynamicArray<UISceneInfo>& getInfos() const { return m_infos; }
        [[nodiscard]] Size_t getCount() const { return m_infos.size(); }

        void submit();
    };

} // namespace dodoe
