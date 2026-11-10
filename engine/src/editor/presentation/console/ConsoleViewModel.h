// do@Redlive
#pragma once
#include "dopch.h"

#include "core/EditorSession.h"
#include "presentation/ViewModel.h"

namespace cakery {

enum class ConsoleEntryLevel {
    Trace,
    Debug,
    Info,
    Warning,
    Error,
    Critical,
};

struct ConsoleEntry {
    ConsoleEntryLevel level = ConsoleEntryLevel::Info;
    String message;
    String source;
};

class ConsoleViewModel : public ViewModel {
public:
    explicit ConsoleViewModel(EditorSession& session);

    const DynamicArray<ConsoleEntry>& entries() const { return m_entries; }

    void append(ConsoleEntryLevel level, const String& message, const String& source);
    void pollBackend();
    void clear();

private:
    void rebuildMerged();

    EditorSession& m_session;
    DynamicArray<ConsoleEntry> m_local;
    DynamicArray<ConsoleEntry> m_backend;
    DynamicArray<ConsoleEntry> m_entries;
};

} // namespace cakery
