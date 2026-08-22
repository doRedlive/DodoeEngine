// do@Redlive

#include "EditHistory.h"

#include "core/document/EditorDocumentModel.h"

#include <utility>

namespace cakery {

EditHistory::EditHistory(EditorHistory& history, EditorDocumentModel& document)
    : m_history(history), m_document(document)
{
}

void EditHistory::beginTransaction(Author who, std::string message) {
    m_inTransaction = true;
    m_currentAuthor = who;
    m_currentMessage = std::move(message);
    m_commandCount = 0;
    m_undoCountAtBegin = m_history.undoCount();
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
    while (m_history.undoCount() > m_undoCountAtBegin) {
        if (!m_history.undo(m_document)) {
            break;
        }
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

    if (mode == CheckpointMode::DocumentSnapshot) {
        SnapshotEntry snap;
        snap.checkpointId = id;
        snap.document.name = m_document.name();
        snap.document.entities = m_document.entities();
        m_snapshots.push_back(std::move(snap));
    }

    changed.fire();
    return id;
}

void EditHistory::revertTo(uint64_t checkpointId) {
    for (auto& snap : m_snapshots) {
        if (snap.checkpointId == checkpointId) {
            m_document.replaceDocument(snap.document);
            m_history.clear();
            changed.fire();
            return;
        }
    }
}

std::vector<CheckpointInfo> EditHistory::checkpoints() const {
    return m_checkpoints;
}

} // namespace cakery
