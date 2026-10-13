// do@Redlive

#include "CommandRegistry.h"

#include "ExecutionRecorder.h"

#include <algorithm>
#include <cctype>
#include <chrono>
#include <cstdint>
#include <sstream>
#include <utility>

namespace cakery {

namespace {

bool IsUuidValue(const nlohmann::json& value)
{
    if (value.is_number_unsigned()) {
        return value.get<std::uint64_t>() != 0;
    }
    if (value.is_number_integer()) {
        return value.get<std::int64_t>() > 0;
    }
    if (!value.is_string() || value.get<std::string>().empty()) {
        return false;
    }
    const std::string text = value.get<std::string>();
    return std::all_of(text.begin(), text.end(), [](unsigned char c) { return std::isdigit(c) != 0; });
}

bool IsParamType(const nlohmann::json& value, const std::string& type)
{
    if (type == "string") return value.is_string();
    if (type == "boolean" || type == "bool") return value.is_boolean();
    if (type == "integer" || type == "int") return value.is_number_integer() || value.is_number_unsigned();
    if (type == "number" || type == "float" || type == "double") return value.is_number();
    if (type == "object") return value.is_object();
    if (type == "array") return value.is_array();
    if (type == "uuid") return IsUuidValue(value);
    return true;
}

CommandResult ValidateStructuredArgs(const CommandSpec& spec, const nlohmann::json& args)
{
    if (!args.is_object()) {
        return CommandResult::Err("Arguments for " + spec.name + " must be an object");
    }
    for (const auto& param : spec.params) {
        const auto it = args.find(param.name);
        if (it == args.end() || it->is_null()) {
            if (param.required) {
                return CommandResult::Err("Missing required parameter: " + param.name);
            }
            continue;
        }
        if (!IsParamType(*it, param.type)) {
            return CommandResult::Err("Invalid type for parameter '" + param.name + "' (expected " +
                                      param.type + ")");
        }
        if (!param.enumValues.empty()) {
            bool matched = false;
            if (it->is_string()) {
                const std::string text = it->get<std::string>();
                for (const auto& allowed : param.enumValues) {
                    if (text == allowed) {
                        matched = true;
                        break;
                    }
                }
            }
            if (!matched) {
                std::string allowedList;
                for (const auto& allowed : param.enumValues) {
                    if (!allowedList.empty()) {
                        allowedList += ", ";
                    }
                    allowedList += allowed;
                }
                return CommandResult::Err("Parameter '" + param.name + "' must be one of: " + allowedList);
            }
        }
    }
    return CommandResult::Ok();
}

std::string StructuredToken(const nlohmann::json& value)
{
    if (value.is_string()) {
        return value.get<std::string>();
    }
    if (value.is_boolean()) {
        return value.get<bool>() ? "true" : "false";
    }
    if (value.is_number()) {
        return value.dump();
    }
    return value.dump();
}

} // namespace

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

CommandResult CommandRegistry::invoke(EditorSession& session, const std::string& name, const CommandArgs& args) {
    const CommandSpec* spec = find(name);
    if (!spec) {
        return CommandResult::Err("Unknown command: " + name);
    }
    if (!spec->handler) {
        return CommandResult::Err("Command has no handler: " + name);
    }
    try {
        return spec->handler(session, args);
    } catch (const nlohmann::json::exception& error) {
        return CommandResult::Err("Invalid arguments for " + name + ": " + std::string(error.what()));
    }
}

CommandResult CommandRegistry::execute(EditorSession& session, const std::string& line) {
    std::string name;
    CommandArgs args;
    parseLine(line, name, args);

    const auto start = std::chrono::steady_clock::now();
    const CommandResult result = invoke(session, name, args);
    const auto end = std::chrono::steady_clock::now();

    const CommandSpec* spec = find(name);
    ExecutionRecord record;
    record.command = name;
    record.args = line;
    record.ok = result.ok;
    record.message = result.message;
    record.data = result.data;
    record.author = "user";
    record.mutating = spec ? spec->mutating : false;
    record.duration_ms = std::chrono::duration<double, std::milli>(end - start).count();
    ExecutionRecorder::self().record(std::move(record));
    return result;
}

CommandResult CommandRegistry::executeStructuredInternal(EditorSession& session, const std::string& name,
                                                          const nlohmann::json& args) {
    const CommandSpec* spec = find(name);
    if (!spec) {
        return CommandResult::Err("Unknown command: " + name);
    }
    if (!spec->handler) {
        return CommandResult::Err("Command has no handler: " + name);
    }
    const CommandResult validation = ValidateStructuredArgs(*spec, args);
    if (!validation.ok) {
        return validation;
    }
    CommandArgs cargs;
    cargs.named = args;
    cargs.raw = args;
    // A few legacy handlers consume positional arguments. Preserve that
    // interface for structured callers using the declared parameter order.
    for (const auto& param : spec->params) {
        const auto it = args.find(param.name);
        if (it != args.end() && !it->is_null()) {
            cargs.positional.push_back(StructuredToken(*it));
        }
    }
    return invoke(session, name, cargs);
}

CommandResult CommandRegistry::executeStructured(EditorSession& session, const std::string& name, const nlohmann::json& args) {
    return executeStructured(session, name, args, CommandContext{});
}

CommandResult CommandRegistry::executeStructured(EditorSession& session, const std::string& name,
                                                 const nlohmann::json& args, const CommandContext& context) {
    const auto start = std::chrono::steady_clock::now();
    const CommandResult result = executeStructuredInternal(session, name, args);
    const auto end = std::chrono::steady_clock::now();

    const CommandSpec* spec = find(name);
    ExecutionRecord record;
    record.command = name;
    record.args = args;
    record.ok = result.ok;
    record.message = result.message;
    record.data = result.data;
    record.author = context.author.empty() ? std::string("user") : context.author;
    record.session = context.session;
    record.mutating = spec ? spec->mutating : false;
    record.duration_ms = std::chrono::duration<double, std::milli>(end - start).count();
    ExecutionRecorder::self().record(std::move(record));
    return result;
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
            // UUID is an editor-level alias; expose a standard JSON Schema type.
            prop["type"] = p.type == "uuid" ? "string" : p.type;
            prop["description"] = p.help;
            if (!p.enumValues.empty()) {
                prop["enum"] = p.enumValues;
            }
            props[p.name] = prop;
        }
        t["parameters"] = nlohmann::json{{"type", "object"}, {"properties", props}};
        nlohmann::json required = nlohmann::json::array();
        for (const auto& p : s.params) {
            if (p.required) {
                required.push_back(p.name);
            }
        }
        if (!required.empty()) {
            t["parameters"]["required"] = std::move(required);
        }
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
