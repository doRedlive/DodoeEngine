// do@Redlive

#ifdef DODOE_PYTHON_ENABLED

#include "py_bindings.h"

#include "runtime/function/world/entity.h"
#include "runtime/function/world/components.h"
#include "runtime/core/utils/common.h"

namespace dodoe::py_bindings {
namespace {

    struct ComponentRegistry {
        std::function<py::object(Entity&)> add_func;
        std::function<py::object(Entity&)> get_func;
        std::function<bool(Entity&)> has_func;
        std::function<void(Entity&)> remove_func;
    };

    std::unordered_map<String, ComponentRegistry> s_component_registry{};

    const ComponentRegistry* FindComponentRegistry(const String& type_name) {
        auto it = s_component_registry.find(type_name);
        if (it == s_component_registry.end()) { return nullptr; }
        return &it->second;
    }

    template <typename T>
    void RegisterComponentType(const String& type_name) {
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
        RegisterGeneratedComponents(m);

        // Hand-written bindings for special components
        py::class_<Rigidbody2dComponent> rb(m, "Rigidbody2dComponent");
        rb.def("set_linear_velocity", [](Rigidbody2dComponent& self, const Vector2f& velocity) {
            self.velocity_request = velocity;
        });
        rb.def("apply_force_to_center", [](Rigidbody2dComponent& self, const Vector2f& force, const Bool wake) {
            (void)wake;
            self.force_request = force;
        });
        rb.def("apply_linear_impulse_to_center", [](Rigidbody2dComponent& self, const Vector2f& impulse, const Bool wake) {
            (void)wake;
            self.impulse_request = impulse;
        });

        py::enum_<Rigidbody2dComponent::BodyType>(m, "RigidbodyBodyType")
            .value("Static", Rigidbody2dComponent::BodyType::Static)
            .value("Dynamic", Rigidbody2dComponent::BodyType::Dynamic)
            .value("Kinematic", Rigidbody2dComponent::BodyType::Kinematic)
            .export_values();

        py::class_<AnimatorComponent>(m, "AnimatorComponent")
            .def("play", [](AnimatorComponent& self, const String& state_name) { self.play_request = state_name; })
            .def("stop", [](AnimatorComponent& self) { self.stop_requested = true; })
            .def("resume", [](AnimatorComponent& self) { self.resume_requested = true; })
            .def("set_float", [](AnimatorComponent& self, const String& name, const Float value) { self.parameters[name] = value; })
            .def("set_int", [](AnimatorComponent& self, const String& name, const Int32 value) { self.parameters[name] = static_cast<Float>(value); })
            .def("set_bool", [](AnimatorComponent& self, const String& name, const Bool value) { self.parameters[name] = value ? 1.0f : 0.0f; })
            .def("set_trigger", [](AnimatorComponent& self, const String& name) { self.parameters[name] = 1.0f; })
            .def("reset_trigger", [](AnimatorComponent& self, const String& name) { self.parameters[name] = 0.0f; })
            .def("get_float", [](const AnimatorComponent& self, const String& name) { const auto it = self.parameters.find(name); return it != self.parameters.end() ? it->second : 0.0f; })
            .def("get_int", [](const AnimatorComponent& self, const String& name) { const auto it = self.parameters.find(name); return it != self.parameters.end() ? static_cast<Int32>(it->second) : 0; })
            .def("get_bool", [](const AnimatorComponent& self, const String& name) { const auto it = self.parameters.find(name); return it != self.parameters.end() && it->second != 0.0f; });
    }

    void BindEntity(py::module_& m) {
        py::class_<Entity>(m, "Entity")
            .def(py::init<>())
            .def_property_readonly("valid", &Entity::valid)
            .def_property_readonly("name", [](Entity& self) -> const String& { return self.name(); })
            .def("add_component", [](Entity& self, const String& type_name) -> py::object {
                const auto* reg = FindComponentRegistry(type_name);
                if (!reg) { throw py::key_error("unregistered component type: " + type_name); }
                return reg->add_func(self);
            })
            .def("get_component", [](Entity& self, const String& type_name) -> py::object {
                const auto* reg = FindComponentRegistry(type_name);
                if (!reg) { return py::none(); }
                return reg->get_func(self);
            })
            .def("has_component", [](Entity& self, const String& type_name) -> bool {
                const auto* reg = FindComponentRegistry(type_name);
                if (!reg) { return false; }
                return reg->has_func(self);
            })
            .def("remove_component", [](Entity& self, const String& type_name) {
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

#define DO_REG(T) RegisterComponentType<T>(#T)

    s_component_registry.clear();
    DO_REG(TransformComponent);
    DO_REG(Rigidbody2dComponent);
    DO_REG(BoxCollider2dComponent);
    DO_REG(CircleCollider2dComponent);
    DO_REG(SpriteRendererComponent);
    DO_REG(AnimatorComponent);

#undef DO_REG

    BindEntity(m);
}

} // namespace dodoe::py_bindings

#endif // DODOE_PYTHON_ENABLED
