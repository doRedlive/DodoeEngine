// do@Redlive

#pragma once

#include "Panel.h"
#include <QListWidget>

namespace cakery {

class ConsolePanel : public Panel {
    Q_OBJECT
public:
    explicit ConsolePanel(EditorContext& ctx, QWidget* parent = nullptr);
};

} // namespace cakery
