#pragma once

#include <cstdint>
#include <string>
#include <vector>
#include <memory>

#include "framework/core/Signal.h"
#include "runtime/resource/res_type/scene_res.h"

namespace cakery {

class EditorContext;

enum class Author { User, AI, System };
enum class CheckpointMode { StackMarker, SceneSnapshot };

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
    explicit EditHistory(EditorContext& ctx) : m_ctx(ctx) {}

    void beginTransaction(Author who, std::string message);
    uint64_t commit();
    void abortTransaction();

    std::vector<TransactionInfo> log() const;

    uint64_t createCheckpoint(std::string name, Author who,
                               CheckpointMode mode = CheckpointMode::SceneSnapshot);
    void revertTo(uint64_t checkpointId);

    std::vector<CheckpointInfo> checkpoints() const;

    Signal<> changed;

private:
    EditorContext& m_ctx;
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
        dodoe::SceneRes sceneData;
    };
    std::vector<SnapshotEntry> m_snapshots;
};

} // namespace cakery
