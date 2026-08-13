// do@Redlive

#include "animator_controller_asset.h"

#include "runtime/core/meta/serializer/serializer.h"
#include "runtime/core/utils/json.h"

#include <fstream>
#include <sstream>

namespace dodoe {

    namespace {

        Json WriteClipRef(const AnimatorClipRef& ref) {
            Json j = Json::object();
            j["type"] = static_cast<UInt32>(ref.type);
            if (ref.type == AnimatorClipType::Clip2D) {
                j["clip_2d"] = Serializer::write(ref.clip_2d);
            }
            else if (ref.type == AnimatorClipType::Clip3D) {
                j["clip_3d"] = Serializer::write(ref.clip_3d);
            }
            return j;
        }

        void ReadClipRef(const Json& j, AnimatorClipRef& ref) {
            if (j.contains("type")) {
                ref.type = static_cast<AnimatorClipType>(j["type"].get<UInt32>());
            }
            if (j.contains("clip_2d")) {
                Serializer::read(j["clip_2d"], ref.clip_2d);
            }
            if (j.contains("clip_3d")) {
                Serializer::read(j["clip_3d"], ref.clip_3d);
            }
        }

        Json WriteState(const AnimatorState& state) {
            Json j = Json::object();
            j["name"] = Serializer::write(state.name);
            j["clip"] = WriteClipRef(state.clip);
            j["loop"] = Serializer::write(state.loop);
            j["speed"] = Serializer::write(state.speed);
            return j;
        }

        void ReadState(const Json& j, AnimatorState& state) {
            if (j.contains("name")) {
                Serializer::read(j["name"], state.name);
            }
            if (j.contains("clip")) {
                ReadClipRef(j["clip"], state.clip);
            }
            if (j.contains("loop")) {
                Serializer::read(j["loop"], state.loop);
            }
            if (j.contains("speed")) {
                Serializer::read(j["speed"], state.speed);
            }
        }

        Json WriteTransition(const AnimatorTransition& transition) {
            Json j = Json::object();
            j["from_state"] = Serializer::write(transition.from_state);
            j["to_state"] = Serializer::write(transition.to_state);
            Json conditions = Json::array();
            for (const auto& condition : transition.conditions) {
                Json c;
                c["parameter"] = Serializer::write(condition.parameter);
                c["mode"] = static_cast<UInt32>(condition.mode);
                c["threshold"] = Serializer::write(condition.threshold);
                conditions.push_back(std::move(c));
            }
            j["conditions"] = std::move(conditions);
            j["has_exit_time"] = Serializer::write(transition.has_exit_time);
            j["exit_time"] = Serializer::write(transition.exit_time);
            j["duration"] = Serializer::write(transition.duration);
            return j;
        }

        void ReadTransition(const Json& j, AnimatorTransition& transition) {
            if (j.contains("from_state")) {
                Serializer::read(j["from_state"], transition.from_state);
            }
            if (j.contains("to_state")) {
                Serializer::read(j["to_state"], transition.to_state);
            }
            if (j.contains("conditions") && j["conditions"].is_array()) {
                DynamicArray<AnimatorCondition> conditions;
                for (const auto& c : j["conditions"]) {
                    AnimatorCondition condition;
                    if (c.contains("parameter")) {
                        Serializer::read(c["parameter"], condition.parameter);
                    }
                    if (c.contains("mode")) {
                        condition.mode = static_cast<AnimatorConditionMode>(c["mode"].get<UInt32>());
                    }
                    if (c.contains("threshold")) {
                        Serializer::read(c["threshold"], condition.threshold);
                    }
                    conditions.push_back(std::move(condition));
                }
                transition.conditions = std::move(conditions);
            }
            if (j.contains("has_exit_time")) {
                Serializer::read(j["has_exit_time"], transition.has_exit_time);
            }
            if (j.contains("exit_time")) {
                Serializer::read(j["exit_time"], transition.exit_time);
            }
            if (j.contains("duration")) {
                Serializer::read(j["duration"], transition.duration);
            }
        }

    } // namespace

    Bool AnimatorControllerAsset::loadFromSource(const String& absolute_source_path) {
        std::ifstream file(absolute_source_path.c_str());
        if (!file.is_open()) {
            return false;
        }

        std::stringstream buffer;
        buffer << file.rdbuf();

        Json json;
        try {
            json = Json::parse(buffer.str());
        } catch (const Json::exception&) {
            return false;
        }

        if (json.contains("parameters") && json["parameters"].is_array()) {
            DynamicArray<AnimatorParameter> parameters;
            parameters.reserve(json["parameters"].size());
            for (const auto& p : json["parameters"]) {
                AnimatorParameter parameter;
                if (p.contains("name")) {
                    Serializer::read(p["name"], parameter.name);
                }
                if (p.contains("type")) {
                    parameter.type = static_cast<AnimatorParameterType>(p["type"].get<UInt32>());
                }
                if (p.contains("default_float")) {
                    Serializer::read(p["default_float"], parameter.default_float);
                }
                if (p.contains("default_int")) {
                    Serializer::read(p["default_int"], parameter.default_int);
                }
                if (p.contains("default_bool")) {
                    Serializer::read(p["default_bool"], parameter.default_bool);
                }
                parameters.push_back(std::move(parameter));
            }
            m_parameters = std::move(parameters);
        }

        if (json.contains("states") && json["states"].is_array()) {
            DynamicArray<AnimatorState> states;
            states.reserve(json["states"].size());
            for (const auto& s : json["states"]) {
                AnimatorState state;
                ReadState(s, state);
                states.push_back(std::move(state));
            }
            m_states = std::move(states);
        }

        if (json.contains("transitions") && json["transitions"].is_array()) {
            DynamicArray<AnimatorTransition> transitions;
            transitions.reserve(json["transitions"].size());
            for (const auto& t : json["transitions"]) {
                AnimatorTransition transition;
                ReadTransition(t, transition);
                transitions.push_back(std::move(transition));
            }
            m_transitions = std::move(transitions);
        }

        if (json.contains("default_state")) {
            Serializer::read(json["default_state"], m_default_state);
        }

        m_meta.source_path = absolute_source_path;
        return true;
    }

    void AnimatorControllerAsset::unloadRuntime() {
        m_parameters.clear();
        m_states.clear();
        m_transitions.clear();
    }

    Bool AnimatorControllerAsset::saveToSource(const String& absolute_path) const {
        std::ofstream file(absolute_path.c_str());
        if (!file.is_open()) {
            return false;
        }

        Json json;

        Json parameters = Json::array();
        for (const auto& parameter : m_parameters) {
            Json p;
            p["name"] = Serializer::write(parameter.name);
            p["type"] = static_cast<UInt32>(parameter.type);
            p["default_float"] = Serializer::write(parameter.default_float);
            p["default_int"] = Serializer::write(parameter.default_int);
            p["default_bool"] = Serializer::write(parameter.default_bool);
            parameters.push_back(std::move(p));
        }
        json["parameters"] = std::move(parameters);

        Json states = Json::array();
        for (const auto& state : m_states) {
            states.push_back(WriteState(state));
        }
        json["states"] = std::move(states);

        Json transitions = Json::array();
        for (const auto& transition : m_transitions) {
            transitions.push_back(WriteTransition(transition));
        }
        json["transitions"] = std::move(transitions);

        json["default_state"] = Serializer::write(m_default_state);

        file << json.dump(4);
        file.flush();
        return true;
    }

} // dodoe
