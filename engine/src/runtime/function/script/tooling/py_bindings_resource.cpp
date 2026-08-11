// do@Redlive

#ifdef DODOE_PYTHON_ENABLED

#include "py_bindings.h"

#include "runtime/function/animation/anim_clip_2d.h"
#include "runtime/function/render/texture/sprite.h"
#include "runtime/resource/resource_manager.h"
#include "runtime/resource/file/file_id.h"

namespace dodoe::py_bindings {
namespace {

    struct TextureRes {
        InstanceID id{0};
        String path{};
        Float ppu{kDefaultPixelsPerUnit};

        [[nodiscard]] Bool valid() const { return id != 0; }
    };

} // anonymous namespace

void RegisterResource(py::module_& m) {
    py::class_<TextureRes>(m, "TextureRes")
        .def(py::init<>())
        .def_readwrite("id", &TextureRes::id)
        .def_readwrite("path", &TextureRes::path)
        .def_readwrite("ppu", &TextureRes::ppu)
        .def_property_readonly("valid", &TextureRes::valid);

    py::class_<AnimClip2DRes>(m, "AnimClip2DRes")
        .def(py::init<>())
        .def_readwrite("id", &AnimClip2DRes::id)
        .def_readwrite("name", &AnimClip2DRes::name);

    py::module_ rm = m.def_submodule("resource_manager");
    auto load_texture_fn = [](const String& path) -> TextureRes {
        auto handle = ResourceManager::Self().getTexture(path);
        TextureRes res;
        if (handle.isValid()) {
            res.id = static_cast<InstanceID>(static_cast<UInt64>(handle.getUUID()));
            res.path = path;
        }
        return res;
    };
    rm.def("load_texture", load_texture_fn);
    rm.def("get_texture", load_texture_fn);
}

} // namespace dodoe::py_bindings

#endif // DODOE_PYTHON_ENABLED
