// do@Redlive
#pragma once
#include "dopch.h"

#include "core/PlayState.h"
#include "core/Signal.h"

#include <functional>
#include <map>
#include <memory>
#include <typeindex>

namespace cakery {

struct DocumentChangedEvent {
    String documentId;
};

struct SelectionChangedEvent {};

struct PlayStateChangedEvent {
    PlayState state = PlayState::Edit;
};

struct AssetCatalogChangedEvent {};

struct HistoryChangedEvent {};

struct ExecutionRecordedEvent {};

struct TileEditModeChangedEvent {
    bool active = false;
};

struct NotificationEvent {
    String level;
    String message;
    String source;
};

struct ProjectOpenedEvent {
    String rootPath;
};

struct ProjectClosedEvent {};

class DomainEventBus {
public:
    template <typename E>
    Signal<const E&>& signal() {
        auto& slot = m_signals[typeid(E)];
        if (!slot) {
            slot = std::make_shared<Signal<const E&>>();
        }
        return *std::static_pointer_cast<Signal<const E&>>(slot);
    }

    template <typename E>
    void publish(const E& event) {
        signal<E>().fire(event);
    }

    template <typename E, typename Handler>
    ScopedConnection subscribe(Handler&& handler) {
        auto& sig = signal<E>();
        return ScopedConnection(sig, sig.connect(std::forward<Handler>(handler)));
    }

private:
    std::map<std::type_index, std::shared_ptr<void>> m_signals;
};

} // namespace cakery
