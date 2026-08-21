// do@Redlive

#pragma once

#include "Panel.h"
#include <QComboBox>
#include <QCheckBox>

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
    void buildToolbar();
    void setupViewport();

    bool m_firstShow = true;
    QComboBox* m_shadingCombo = nullptr;
    QCheckBox* m_2dCheck = nullptr;
    QCheckBox* m_gizmoCheck = nullptr;
};

} // namespace cakery
