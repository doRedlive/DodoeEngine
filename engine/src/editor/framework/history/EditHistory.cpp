#include "EditHistory.h"
#include "framework/EditorContext.h"
#include "framework/command/CommandStack.h"
#include "runtime/function/world/scene.h"
#include "runtime/function/log/log_system.h"

namespace cakery {

void EditHistory::beginTransaction(Author who, std::string message) {
    m_inTransaction = true;
    m_currentAuthor = who;
    m_currentMessage = std::move(message);
    m_commandCount = 0;
    m_undoCountAtBegin = 0;
}

uint64_t EditHistory::commit() {
    if (!m_inTransaction) return 0;
    m_inTransaction = false;
    TransactionInfo info;
    info.id = m_nextId++;
    info.author = m_currentAuthor;
    info.message = m_currentMessage;
    info.commandCount = m_commandCount;
    m_transactions.push_back(info);
    changed.fire();
    return info.id;
}

void EditHistory::abortTransaction() {
    if (!m_inTransaction) return;
    m_inTransaction = false;
    while (m_commandCount > 0) {
        if (m_ctx.commands().canUndo()) {
            m_ctx.commands().undo();
        }
        --m_commandCount;
    }
    changed.fire();
}

std::vector<TransactionInfo> EditHistory::log() const {
    return m_transactions;
}

uint64_t EditHistory::createCheckpoint(std::string name, Author who, CheckpointMode mode) {
    uint64_t id = m_nextId++;
    CheckpointInfo info;
    info.id = id;
    info.name = name;
    info.author = who;
    info.mode = mode;
    m_checkpoints.push_back(info);

    if (mode == CheckpointMode::SceneSnapshot) {
        auto* scene = m_ctx.activeScene();
        if (scene) {
            SnapshotEntry snap;
            snap.checkpointId = id;
            snap.sceneData = scene->serialize();
            m_snapshots.push_back(std::move(snap));
        }
    }

    changed.fire();
    return id;
}

void EditHistory::revertTo(uint64_t checkpointId) {
    for (auto& snap : m_snapshots) {
        if (snap.checkpointId == checkpointId) {
            auto* scene = m_ctx.activeScene();
            if (scene) {
                scene->deserialize(snap.sceneData);
            }
            m_ctx.commands().clear();
            changed.fire();
            return;
        }
    }

    LOG_ERROR("[EditHistory] Checkpoint {} not found", checkpointId);
}

std::vector<CheckpointInfo> EditHistory::checkpoints() const {
    return m_checkpoints;
}

} // namespace cakery
