// do@Redlive

#include "mesh_phase_registry.h"

namespace dodoe {

    MeshPhaseRegistry& MeshPhaseRegistry::Self() {
        static MeshPhaseRegistry instance{};
        return instance;
    }

    void MeshPhaseRegistry::registerBuiltinPhases() {
        if (m_builtin_registered) {
            return;
        }
        registerPhase("GBuffer", MeshPassType::Opaque);
        registerPhase("Opaque", MeshPassType::Opaque);
        registerPhase("DirectionalShadow", MeshPassType::Shadow);
        registerPhase("Shadow", MeshPassType::Shadow);
        registerPhase("Transparent", MeshPassType::Transparent);
        m_builtin_registered = true;
    }

    void MeshPhaseRegistry::registerPhase(const String& name, const MeshPassType pass_type) {
        for (auto& phase : m_phases) {
            if (phase.name == name) {
                phase.pass_type = pass_type;
                return;
            }
        }
        MeshPhaseDesc desc{};
        desc.name = name;
        desc.pass_type = pass_type;
        m_phases.push_back(std::move(desc));
    }

    Bool MeshPhaseRegistry::find(const String& name, MeshPassType& out_pass_type) const {
        for (const auto& phase : m_phases) {
            if (phase.name == name) {
                out_pass_type = phase.pass_type;
                return true;
            }
        }
        return false;
    }

} // dodoe
