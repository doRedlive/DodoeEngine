// do@Redlive

#include "CommandRegistry.h"

#include <algorithm>
#include <sstream>
#include <utility>

namespace cakery {

CommandRegistry& CommandRegistry::self() {
    static CommandRegistry instance;
    return instance;
}

void CommandRegistry::add(CommandSpec spec) {
    m_specs.push_back(std::move(spec));
}

bool CommandRegistry::has(const std::string& name) const {
    return find(name) != nullptr;
}

const CommandSpec* CommandRegistry::find(const std::string& name) const {
    for (auto& s : m_specs) {
        if (s.name == name) return &s;
    }
    return nullptr;
}

void CommandRegistry::parseLine(const std::string& line, std::string& outName, CommandArgs& outArgs) {
    std::istringstream iss(line);
    iss >> outName;

    std::string token;
    while (iss >> token) {
        auto eq = token.find('=');
        if (eq != std::string::npos) {
            std::string key = token.substr(0, eq);
            std::string val = token.substr(eq + 1);
            outArgs.named[key] = val;
        } else {
            outArgs.positional.push_back(token);
        }
    }
}

CommandResult CommandRegistry::execute(EditorSession& session, const std::string& line) {
    std::string name;
    CommandArgs args;
    parseLine(line, name, args);

    const auto* spec = find(name);
    if (!spec) {
        return CommandResult::Err("Unknown command: " + name);
    }
    if (!spec->handler) {
        return CommandResult::Err("Command has no handler: " + name);
    }
    return spec->handler(session, args);
}

CommandResult CommandRegistry::executeStructured(EditorSession& session, const std::string& name, const nlohmann::json& args) {
    const auto* spec = find(name);
    if (!spec) {
        return CommandResult::Err("Unknown command: " + name);
    }
    if (!spec->handler) {
        return CommandResult::Err("Command has no handler: " + name);
    }
    CommandArgs cargs;
    if (args.is_object()) {
        cargs.named = args;
    }
    cargs.raw = args;
    return spec->handler(session, cargs);
}

std::vector<CommandSpec> CommandRegistry::list() const {
    return m_specs;
}

nlohmann::json CommandRegistry::toolSchema() const {
    nlohmann::json tools = nlohmann::json::array();
    for (auto& s : m_specs) {
        nlohmann::json t;
        t["name"] = s.name;
        t["description"] = s.summary;
        nlohmann::json props = nlohmann::json::object();
        for (auto& p : s.params) {
            nlohmann::json prop;
            prop["type"] = p.type;
            prop["description"] = p.help;
            props[p.name] = prop;
        }
        t["parameters"] = props;
        t["mutating"] = s.mutating;
        tools.push_back(t);
    }
    return tools;
}

std::string CommandRegistry::help(const std::string& name) const {
    const auto* spec = find(name);
    if (!spec) return "Unknown command: " + name;
    return spec->summary + "\nUsage: " + spec->usage;
}

} // namespace cakery
