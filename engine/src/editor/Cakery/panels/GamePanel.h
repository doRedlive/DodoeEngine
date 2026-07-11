// do@Redlive

#pragma once

#include "Panel.h"

namespace cakery {

class GamePanel : public Panel {
    Q_OBJECT
public:
    explicit GamePanel(EditorContext& ctx, QWidget* parent = nullptr);

protected:
    void paintEvent(QPaintEvent* event) override;
};

} // namespace cakery
