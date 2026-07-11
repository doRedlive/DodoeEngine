// do@Redlive

#pragma once

#include "Panel.h"

namespace cakery {

class ScenePanel : public Panel {
    Q_OBJECT
public:
    explicit ScenePanel(EditorContext& ctx, QWidget* parent = nullptr);
    ~ScenePanel() override;

protected:
    void showEvent(QShowEvent* event) override;
    void paintEvent(QPaintEvent* event) override;
    void resizeEvent(QResizeEvent* event) override;

    void mousePressEvent(QMouseEvent* event) override;
    void mouseReleaseEvent(QMouseEvent* event) override;
    void mouseMoveEvent(QMouseEvent* event) override;
    void wheelEvent(QWheelEvent* event) override;

    void dragEnterEvent(QDragEnterEvent* event) override;
    void dropEvent(QDropEvent* event) override;

private:
    bool m_firstShow = true;
    void setupViewport();
};

} // namespace cakery
