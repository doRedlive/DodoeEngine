// do@Redlive

#pragma once

#include "dopch.h"
#include "runtime/core/object/object.h"
#include "runtime/function/graphics/gfx.h"
#include "runtime/function/graphics/draw_command_list.h"
#include "descriptor_table_manager.h"

namespace dodoe {

    class DODOE_API Texture : public Object {
        Int32 m_width{0};
        Int32 m_height{0};
        String m_path{};
        GfxTextureHandle m_gpu_handle{};
        DescriptorIndex m_descriptor_index{-1};

    public:
        Texture() = default;

        [[nodiscard]] const char* getObjectTypeName() const override { return "Texture"; }
        [[nodiscard]] Int32 getWidth() const { return m_width; }
        [[nodiscard]] Int32 getHeight() const { return m_height; }
        [[nodiscard]] const String& getPath() const { return m_path; }
        [[nodiscard]] GfxTextureHandle getGpuHandle() const { return m_gpu_handle; }
        [[nodiscard]] DescriptorIndex getDescriptorIndex() const { return m_descriptor_index; }

        void setDimensions(const Int32 w, const Int32 h) { m_width = w; m_height = h; }
        void setPath(const String& p) { m_path = p; }
        void setGpuHandle(GfxTextureHandle handle) { m_gpu_handle = std::move(handle); }
        void setDescriptorIndex(DescriptorIndex index) { m_descriptor_index = index; }

        [[nodiscard]] static Ref<Texture> Load(const String& path);
    };

} // dodoe
