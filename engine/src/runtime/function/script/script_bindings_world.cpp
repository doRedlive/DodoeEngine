// do@Redlive

#ifdef DODOE_PYTHON_ENABLED

#include "script_bindings.h"

#include "runtime/function/world/world.h"
#include "runtime/function/world/scene.h"
#include "runtime/function/world/registry.h"
#include "runtime/function/world/entity.h"
#include "runtime/core/application.h"
#include "runtime/core/context/system_context.h"

namespace dodoe::script_bindings {

    void RegisterWorld(py::module_& m) {
        py::class_<Registry>(m, "Registry")
            .def("create_entity", &Registry::create)
            .def("destroy_entity", [](Registry& reg, const Entity& entity) { reg.destroy(entity); })
            .def("valid_entity", [](Registry& reg, const Entity& entity) { return reg.valid(entity); })
            .def("clear", &Registry::clear);

        py::class_<Scene>(m, "Scene")
            .def_property("name",
                [](Scene& s) -> const std::string& { return s.getName(); },
                &Scene::setName)
            .def("create_entity",
                [](Scene& scene, const std::string& name) -> Entity { return scene.createEntity(name); },
                py::arg("name") = std::string("Entity"))
            .def("destroy_entity", &Scene::destroyEntity)
            .def("get_registry", [](Scene& scene) -> Registry* { return &scene.registry(); },
                py::return_value_policy::reference);

        py::class_<World>(m, "World")
            .def_property_readonly("name", &World::getName)
            .def("create_scene", &World::createScene)
            .def("delete_scene", &World::deleteScene)
            .def("get_scene", &World::getScene, py::return_value_policy::reference)
            .def("get_current_scene", &World::getCurrentScene, py::return_value_policy::reference)
            .def("load_scene", &World::loadScene);

        m.def("get_world", []() -> World* {
            return dodoe::GetWorld();
        }, py::return_value_policy::reference);
    }

} // dodoe::script_bindings

#endif // DODOE_PYTHON_ENABLED
