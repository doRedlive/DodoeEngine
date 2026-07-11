// do@Redlive

#pragma once

#include <string>
#include <memory>

namespace cakery {

class EditorContext;

class ICommand {
public:
    virtual ~ICommand() = default;

    virtual bool execute(EditorContext& ctx) = 0;
    virtual void undo(EditorContext& ctx) = 0;
    virtual void redo(EditorContext& ctx) { execute(ctx); }

    virtual std::string label() const = 0;

    virtual bool mergeWith(const ICommand& /*next*/) { return false; }
};

} // namespace cakery
