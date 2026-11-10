// do@Redlive
#pragma once
#include "dopch.h"

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
    UInt64 id;
    Author author;
    String message;
    int commandCount = 0;
};

struct CheckpointInfo {
    UInt64 id;
    String name;
    Author author;
    CheckpointMode mode;
};

class EditHistory {
public:
    EditHistory(EditorHistory& history, EditorDocumentModel& document);

    void beginTransaction(Author who, String message);
    UInt64 commit();
    void abortTransaction();

    DynamicArray<TransactionInfo> log() const;

    UInt64 createCheckpoint(String name, Author who,
                              CheckpointMode mode = CheckpointMode::DocumentSnapshot);
    void revertTo(UInt64 checkpointId);

    DynamicArray<CheckpointInfo> checkpoints() const;

    Signal<> changed;

private:
    EditorHistory& m_history;
    EditorDocumentModel& m_document;
    UInt64 m_nextId = 1;

    bool m_inTransaction = false;
    Author m_currentAuthor = Author::User;
    String m_currentMessage;
    int m_commandCount = 0;
    Size_t m_undoCountAtBegin = 0;
    ScopedConnection m_historySubscription;

    DynamicArray<TransactionInfo> m_transactions;
    DynamicArray<CheckpointInfo> m_checkpoints;

    struct SnapshotEntry {
        UInt64 checkpointId;
        EditorDocument document;
    };
    DynamicArray<SnapshotEntry> m_snapshots;
};

} // namespace cakery
