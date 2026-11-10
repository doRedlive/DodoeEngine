// do@Redlive

#include "dopch.h"

#include "CommandRegistry.h"

#include "ExecutionRecorder.h"
#include "core/EditorSession.h"

#include <algorithm>
#include <cctype>
#include <chrono>
#include <cstdint>
#include <sstream>
#include <utility>

namespace cakery {

namespace {

bool IsUuidValue(const Json& value)
{
    if (value.is_number_unsigned()) {
        return value.get<UInt64>() != 0;
    }
    if (value.is_number_integer()) {
        return value.get<Int64>() > 0;
    }
    if (!value.is_string() || value.get<String>().empty()) {
        return false;
    }
    const String text = value.get<String>();
    return std::all_of(text.begin(), text.end(), [](unsigned char c) { return std::isdigit(c) != 0; });
}

bool IsParamType(const Json& value, const String& type)
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

CommandResult ValidateStructuredArgs(const CommandSpec& spec, const Json& args)
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
                const String text = it->get<String>();
                for (const auto& allowed : param.enumValues) {
                    if (text == allowed) {
                        matched = true;
                        break;
                    }
                }
            }
            if (!matched) {
                String allowedList;
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

String StructuredToken(const Json& value)
{
    if (value.is_string()) {
        return value.get<String>();
    }
    if (value.is_boolean()) {
        return value.get<bool>() ? "true" : "false";
    }
    if (value.is_number()) {
        return ToString(value);
    }
    return ToString(value);
}

} // namespace

CommandRegistry::CommandRegistry() = default;

void CommandRegistry::add(CommandSpec spec) {
    m_specs.push_back(std::move(spec));
}

bool CommandRegistry::has(const String& name) const {
    return find(name) != nullptr;
}

const CommandSpec* CommandRegistry::find(const String& name) const {
    for (auto& s : m_specs) {
        if (s.name == name) return &s;
    }
    return nullptr;
}

void CommandRegistry::parseLine(const String& line, String& outName, CommandArgs& outArgs) {
    std::istringstream iss(line);
    iss >> outName;

    String token;
    while (iss >> token) {
        auto eq = token.find('=');
        if (eq != String::npos) {
            String key = token.substr(0, eq);
            String val = token.substr(eq + 1);
            outArgs.named[key] = val;
        } else {
            outArgs.positional.push_back(token);
        }
    }
}

CommandResult CommandRegistry::invoke(EditorSession& session, const String& name, const CommandArgs& args) {
    const CommandSpec* spec = find(name);
    if (!spec) {
        return CommandResult::Err("Unknown command: " + name);
    }
    if (!spec->handler) {
        return CommandResult::Err("Command has no handler: " + name);
    }
    try {
        return spec->handler(session, args);
    } catch (const Json::exception& error) {
        return CommandResult::Err("Invalid arguments for " + name + ": " + String(error.what()));
    }
}

CommandResult CommandRegistry::execute(EditorSession& session, const String& line) {
    String name;
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
    session.executionRecorder().record(std::move(record));
    return result;
}

CommandResult CommandRegistry::executeStructuredInternal(EditorSession& session, const String& name,
                                                          const Json& args) {
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

CommandResult CommandRegistry::executeStructured(EditorSession& session, const String& name, const Json& args) {
    return executeStructured(session, name, args, CommandContext{});
}

CommandResult CommandRegistry::executeStructured(EditorSession& session, const String& name,
                                                 const Json& args, const CommandContext& context) {
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
    record.author = context.author.empty() ? String("user") : context.author;
    record.session = context.session;
    record.mutating = spec ? spec->mutating : false;
    record.duration_ms = std::chrono::duration<double, std::milli>(end - start).count();
    session.executionRecorder().record(std::move(record));
    return result;
}

DynamicArray<CommandSpec> CommandRegistry::list() const {
    return m_specs;
}

Json CommandRegistry::toolSchema() const {
    Json tools = Json::array();
    for (auto& s : m_specs) {
        Json t;
        t["name"] = s.name;
        t["description"] = s.summary;
        Json props = Json::object();
        for (auto& p : s.params) {
            Json prop;
            // UUID is an editor-level alias; expose a standard JSON Schema type.
            prop["type"] = p.type == "uuid" ? "string" : p.type;
            prop["description"] = p.help;
            if (!p.enumValues.empty()) {
                prop["enum"] = p.enumValues;
            }
            props[p.name] = prop;
        }
        t["parameters"] = Json{{"type", "object"}, {"properties", props}};
        Json required = Json::array();
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

String CommandRegistry::help(const String& name) const {
    const auto* spec = find(name);
    if (!spec) return "Unknown command: " + name;
    return spec->summary + "\nUsage: " + spec->usage;
}

} // namespace cakery
