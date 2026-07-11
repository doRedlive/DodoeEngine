// do@Redlive

#pragma once

#include <QWidget>
#include <vector>

#include "framework/core/Signal.h"

namespace cakery {

class EditorContext;

class Panel : public QWidget {
    Q_OBJECT
public:
    explicit Panel(EditorContext& ctx, QWidget* parent = nullptr)
        : QWidget(parent), m_ctx(ctx) {}

protected:
    EditorContext& ctx() { return m_ctx; }
    EditorContext& m_ctx;

    std::vector<ScopedConnection> m_connections;
};

} // namespace cakery
