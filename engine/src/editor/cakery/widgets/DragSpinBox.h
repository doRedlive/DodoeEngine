// do@Redlive
// Unity-style drag-to-change numeric spinbox

#pragma once

#include <QDoubleSpinBox>

class QMouseEvent;
class QWheelEvent;
class QObject;
class QEvent;

namespace cakery {

class DragSpinBox : public QDoubleSpinBox {
    Q_OBJECT
public:
    explicit DragSpinBox(QWidget* parent = nullptr);

protected:
    bool eventFilter(QObject* watched, QEvent* event) override;
    void mousePressEvent(QMouseEvent* event) override;
    void mouseMoveEvent(QMouseEvent* event) override;
    void mouseReleaseEvent(QMouseEvent* event) override;
    void wheelEvent(QWheelEvent* event) override;

private:
    void handleMousePress(QMouseEvent* event);
    void handleMouseMove(QMouseEvent* event);
    bool handleMouseRelease(QMouseEvent* event);

    bool m_dragging = false;
    int m_dragStartX = 0;
    double m_dragStartValue = 0.0;
    static constexpr int kDragThreshold = 3;
    static constexpr double kSensitivity = 0.05;
};

} // namespace cakery
