// do@Redlive

#pragma once

#include "dopch.h"
#include "runtime/core/object/object.h"
#include "runtime/core/object/object_id.h"
#include "runtime/function/graphics/gfx.h"
#include "runtime/function/graphics/draw_command_list.h"
#include "runtime/function/render/shader/descriptor_table_manager.h"

namespace dodoe {

    class DODOE_API Texture : public Object {
    protected:
        String m_path{};
        GfxTextureHandle m_gpu_handle{};

    public:
        Texture() = default;
        explicit Texture(const ObjectID& id)
            : Object(id) {}

        [[nodiscard]] virtual Int32 getWidth() const = 0;
        [[nodiscard]] virtual Int32 getHeight() const = 0;
        [[nodiscard]] const String& getPath() const { return m_path; }
        [[nodiscard]] GfxTextureHandle getGpuHandle() const { return m_gpu_handle; }
        [[nodiscard]] virtual DescriptorIndex getDescriptorIndex() const { return -1; }

        void setPath(const String& p) { m_path = p; }
        void setGpuHandle(GfxTextureHandle handle) { m_gpu_handle = std::move(handle); }

    protected:
    };

    class DODOE_API Texture2D : public Texture {
        Int32 m_width{0};
        Int32 m_height{0};
        DescriptorIndex m_descriptor_index{-1};
        UInt32 m_slot{0};

    public:
        Texture2D() = default;
        explicit Texture2D(const ObjectID& id)
            : Texture(id) {}

        [[nodiscard]] const char* getObjectTypeName() const override { return "Texture2D"; }
        [[nodiscard]] Int32 getWidth() const override { return m_width; }
        [[nodiscard]] Int32 getHeight() const override { return m_height; }
        [[nodiscard]] DescriptorIndex getDescriptorIndex() const { return m_descriptor_index; }
        [[nodiscard]] UInt32 getSlot() const { return m_slot; }

        void setDimensions(const Int32 w, const Int32 h) { m_width = w; m_height = h; }
        void setDescriptorIndex(DescriptorIndex index) { m_descriptor_index = index; }
        void setSlot(UInt32 slot) { m_slot = slot; }
    };

    class DODOE_API TextureCubemap : public Texture {
        Int32 m_face_size{0};

    public:
        TextureCubemap() = default;
        explicit TextureCubemap(const ObjectID& id)
            : Texture(id) {}

        [[nodiscard]] const char* getObjectTypeName() const override { return "TextureCubemap"; }
        [[nodiscard]] Int32 getWidth() const override { return m_face_size; }
        [[nodiscard]] Int32 getHeight() const override { return m_face_size; }

        void setFaceSize(const Int32 size) { m_face_size = size; }

        [[nodiscard]] static TextureCubemap* LoadFromFaces(const DynamicArray<String>& face_paths);
    };

} // dodoe
