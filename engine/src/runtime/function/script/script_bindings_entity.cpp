// do@Redlive

#include "script_bindings.h"

#include "runtime/function/world/entity.h"
#include "runtime/function/world/components.h"
#include "runtime/core/utils/common.h"

namespace dodoe::script_bindings {
namespace {

    struct ComponentRegistry {
        std::function<py::object(Entity&)> add_func;
        std::function<py::object(Entity&)> get_func;
        std::function<bool(Entity&)> has_func;
        std::function<void(Entity&)> remove_func;
    };

    std::unordered_map<std::string, ComponentRegistry> s_component_registry{};

    const ComponentRegistry* FindComponentRegistry(const std::string& type_name) {
        auto it = s_component_registry.find(type_name);
        if (it == s_component_registry.end()) { return nullptr; }
        return &it->second;
    }

    template <typename T>
    void RegisterComponentType(const std::string& type_name) {
        ComponentRegistry registry{};
        registry.add_func = [](Entity& self) -> py::object {
            if (self.hasComponent<T>()) {
                return py::cast(self.getComponent<T>(), py::return_value_policy::reference);
            }
            return py::cast(self.addComponent<T>(), py::return_value_policy::reference);
        };
        registry.get_func = [](Entity& self) -> py::object {
            if (!self.hasComponent<T>()) { return py::none(); }
            return py::cast(self.getComponent<T>(), py::return_value_policy::reference);
        };
        registry.has_func = [](Entity& self) -> bool { return self.hasComponent<T>(); };
        registry.remove_func = [](Entity& self) { if (self.hasComponent<T>()) { self.removeComponent<T>(); } };
        s_component_registry[type_name] = std::move(registry);
    }

    void BindColor(py::module_& m) {
        py::class_<Color>(m, "Color")
            .def(py::init<>())
            .def(py::init<float, float, float, float>(), py::arg("r") = 1.0f, py::arg("g") = 1.0f, py::arg("b") = 1.0f, py::arg("a") = 1.0f)
            .def_readwrite("r", &Color::r)
            .def_readwrite("g", &Color::g)
            .def_readwrite("b", &Color::b)
            .def_readwrite("a", &Color::a);
    }

    void BindComponents(py::module_& m) {
        py::class_<TransformComponent>(m, "TransformComponent")
            .def(py::init<>())
            .def_readwrite("position", &TransformComponent::position)
            .def_readwrite("rotation", &TransformComponent::rotation)
            .def_readwrite("scale", &TransformComponent::scale)
            .def("set_position", &TransformComponent::setPosition)
            .def("set_rotation", &TransformComponent::setRotation)
            .def("set_scale", &TransformComponent::setScale);

        py::class_<TagComponent>(m, "TagComponent")
            .def(py::init<>())
            .def(py::init<const std::string&>())
            .def_readwrite("tag", &TagComponent::tag);

        py::class_<Rigidbody2dComponent> rb(m, "Rigidbody2dComponent");
        rb.def(py::init<>());
        rb.def_property("body_type",
            [](const Rigidbody2dComponent& c) { return static_cast<int>(c.type); },
            [](Rigidbody2dComponent& c, int val) { c.type = static_cast<Rigidbody2dComponent::BodyType>(val); });
        rb.def_readwrite("gravity_scale", &Rigidbody2dComponent::gravity_scale);
        rb.def_readwrite("fixed_rotation", &Rigidbody2dComponent::fixed_rotation);
        rb.def("set_linear_velocity", &Rigidbody2dComponent::setLinearVelocity);
        rb.def("apply_force_to_center", &Rigidbody2dComponent::applyForceToCenter);
        rb.def("apply_linear_impulse_to_center", &Rigidbody2dComponent::applyLinearImpulseToCenter);

        py::enum_<Rigidbody2dComponent::BodyType>(m, "RigidbodyBodyType")
            .value("Static", Rigidbody2dComponent::BodyType::Static)
            .value("Dynamic", Rigidbody2dComponent::BodyType::Dynamic)
            .value("Kinematic", Rigidbody2dComponent::BodyType::Kinematic)
            .export_values();

        py::class_<BoxCollider2dComponent>(m, "BoxCollider2dComponent")
            .def(py::init<>())
            .def_readwrite("offset", &BoxCollider2dComponent::offset)
            .def_readwrite("size", &BoxCollider2dComponent::size)
            .def_readwrite("friction", &BoxCollider2dComponent::friction)
            .def_readwrite("restitution", &BoxCollider2dComponent::restitution)
            .def_readwrite("restitution_threshold", &BoxCollider2dComponent::restitution_threshold)
            .def_readwrite("density", &BoxCollider2dComponent::density);

        py::class_<SpriteRendererComponent>(m, "SpriteRendererComponent")
            .def(py::init<>())
            .def_readwrite("color", &SpriteRendererComponent::color)
            .def_readwrite("depth", &SpriteRendererComponent::depth_)
            .def_readwrite("flip", &SpriteRendererComponent::flip)
            .def_readwrite("pivot", &SpriteRendererComponent::pivot);

        py::class_<Animation2dComponent>(m, "Animation2dComponent")
            .def(py::init<>())
            .def_readwrite("cur_anim_id", &Animation2dComponent::cur_anim_id)
            .def_readwrite("cur_frame_id", &Animation2dComponent::cur_frame_id)
            .def_readwrite("cur_time_duration", &Animation2dComponent::cur_time_duration)
            .def_readwrite("speed", &Animation2dComponent::speed)
            .def("add_clip", &Animation2dComponent::addClip);
    }

    void BindEntity(py::module_& m) {
        py::class_<Entity>(m, "Entity")
            .def(py::init<>())
            .def_property_readonly("valid", &Entity::valid)
            .def_property_readonly("name", [](Entity& self) -> const std::string& { return self.name(); })
            .def("add_component", [](Entity& self, const std::string& type_name) -> py::object {
                const auto* reg = FindComponentRegistry(type_name);
                if (!reg) { throw py::key_error("unregistered component type: " + type_name); }
                return reg->add_func(self);
            })
            .def("get_component", [](Entity& self, const std::string& type_name) -> py::object {
                const auto* reg = FindComponentRegistry(type_name);
                if (!reg) { return py::none(); }
                return reg->get_func(self);
            })
            .def("has_component", [](Entity& self, const std::string& type_name) -> bool {
                const auto* reg = FindComponentRegistry(type_name);
                if (!reg) { return false; }
                return reg->has_func(self);
            })
            .def("remove_component", [](Entity& self, const std::string& type_name) {
                const auto* reg = FindComponentRegistry(type_name);
                if (!reg) { return; }
                reg->remove_func(self);
            })
            .def("__bool__", [](const Entity& self) { return self.valid(); })
            .def("__eq__", [](const Entity& a, const Entity& b) { return a == b; });
    }

} // anonymous namespace

void RegisterEntity(py::module_& m) {
    BindColor(m);
    BindComponents(m);

    s_component_registry.clear();
    RegisterComponentType<TagComponent>("TagComponent");
    RegisterComponentType<TransformComponent>("TransformComponent");
    RegisterComponentType<Rigidbody2dComponent>("Rigidbody2dComponent");
    RegisterComponentType<BoxCollider2dComponent>("BoxCollider2dComponent");
    RegisterComponentType<SpriteRendererComponent>("SpriteRendererComponent");
    RegisterComponentType<Animation2dComponent>("Animation2dComponent");

    BindEntity(m);
}

} // dodoe::script_bindings
