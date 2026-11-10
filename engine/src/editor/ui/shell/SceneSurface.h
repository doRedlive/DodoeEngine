// do@Redlive
#pragma once
#include "dopch.h"

#include <QWidget>

#include <cstdint>

class QLabel;
class QTimer;

namespace cakery {

class EditorWorkspaceContext;

class SceneSurface final : public QWidget {
public:
    explicit SceneSurface(EditorWorkspaceContext& context, QWidget* parent = nullptr);

    void attach();

protected:
    void showEvent(QShowEvent* event) override;
    void resizeEvent(QResizeEvent* event) override;
    void mousePressEvent(QMouseEvent* event) override;
    void mouseMoveEvent(QMouseEvent* event) override;
    void mouseReleaseEvent(QMouseEvent* event) override;
    void wheelEvent(QWheelEvent* event) override;
    void keyPressEvent(QKeyEvent* event) override;
    void keyReleaseEvent(QKeyEvent* event) override;
    void dragEnterEvent(QDragEnterEvent* event) override;
    void dropEvent(QDropEvent* event) override;

private:
    void initFpsOverlay();
    void positionFpsLabel();
    void refreshFps();
    void publishMetrics();

    EditorWorkspaceContext& m_context;
    UInt64 m_sequence = 0;
    bool m_attached = false;
    QLabel* m_fpsLabel = nullptr;
    QTimer* m_fpsTimer = nullptr;
};

} // namespace cakery
