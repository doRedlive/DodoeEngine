// do@Redlive

#pragma once

#include <memory>
#include <vector>
#include <string>

#include "ICommand.h"
#include "framework/core/Signal.h"

namespace cakery {

class EditorContext;

class CommandStack {
public:
    explicit CommandStack(EditorContext& ctx) : m_ctx(ctx) {}

    // 命令执行并入栈成功后返回其指针（调用方可读取命令产物）；失败或被合并返回 nullptr
    ICommand* execute(std::unique_ptr<ICommand> cmd);

    bool canUndo() const { return !m_undo.empty(); }
    bool canRedo() const { return !m_redo.empty(); }

    void undo();
    void redo();
    void clear();

    void beginMerge() { m_merging = true; }
    void endMerge()   { m_merging = false; m_lastMergeable = nullptr; }

    std::string undoLabel() const;
    std::string redoLabel() const;

    Signal<> changed;

private:
    EditorContext& m_ctx;
    std::vector<std::unique_ptr<ICommand>> m_undo;
    std::vector<std::unique_ptr<ICommand>> m_redo;
    bool m_merging = false;
    ICommand* m_lastMergeable = nullptr;
};

} // namespace cakery
