// do@Redlive

#pragma once

#include "core/document/EditorDocument.h"
#include "core/history/EditorHistory.h"
#include "core/Signal.h"

#include <cstdint>
#include <string>
#include <vector>

namespace cakery {

class EditorDocumentModel;

enum class Author { User, AI, System };
enum class CheckpointMode { StackMarker, DocumentSnapshot };

struct TransactionInfo {
    uint64_t id;
    Author author;
    std::string message;
    int commandCount = 0;
};

struct CheckpointInfo {
    uint64_t id;
    std::string name;
    Author author;
    CheckpointMode mode;
};

class EditHistory {
public:
    EditHistory(EditorHistory& history, EditorDocumentModel& document);

    void beginTransaction(Author who, std::string message);
    uint64_t commit();
    void abortTransaction();

    std::vector<TransactionInfo> log() const;

    uint64_t createCheckpoint(std::string name, Author who,
                              CheckpointMode mode = CheckpointMode::DocumentSnapshot);
    void revertTo(uint64_t checkpointId);

    std::vector<CheckpointInfo> checkpoints() const;

    Signal<> changed;

private:
    EditorHistory& m_history;
    EditorDocumentModel& m_document;
    uint64_t m_nextId = 1;

    bool m_inTransaction = false;
    Author m_currentAuthor = Author::User;
    std::string m_currentMessage;
    int m_commandCount = 0;
    size_t m_undoCountAtBegin = 0;

    std::vector<TransactionInfo> m_transactions;
    std::vector<CheckpointInfo> m_checkpoints;

    struct SnapshotEntry {
        uint64_t checkpointId;
        EditorDocument document;
    };
    std::vector<SnapshotEntry> m_snapshots;
};

} // namespace cakery
