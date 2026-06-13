// do@Redlive

#include "DragSpinBox.h"

#include <QAbstractSpinBox>
#include <QApplication>
#include <QCursor>
#include <QEvent>
#include <QLineEdit>
#include <QMouseEvent>
#include <QWheelEvent>

#include <cmath>

namespace cakery {

DragSpinBox::DragSpinBox(QWidget* parent)
    : QDoubleSpinBox(parent)
{
    setButtonSymbols(QAbstractSpinBox::NoButtons);
    if (lineEdit()) {
        lineEdit()->installEventFilter(this);
        lineEdit()->setCursor(Qt::SizeHorCursor);
    }
    setCursor(Qt::SizeHorCursor);
}

bool DragSpinBox::eventFilter(QObject* watched, QEvent* event)
{
    if (watched == lineEdit()) {
        switch (event->type()) {
        case QEvent::MouseButtonPress:
            handleMousePress(static_cast<QMouseEvent*>(event));
            break;
        case QEvent::MouseMove:
            handleMouseMove(static_cast<QMouseEvent*>(event));
            if (m_dragging) {
                return true;
            }
            break;
        case QEvent::MouseButtonRelease:
            if (handleMouseRelease(static_cast<QMouseEvent*>(event))) {
                return true;
            }
            break;
        case QEvent::Wheel:
            return true;
        default:
            break;
        }
    }

    return QDoubleSpinBox::eventFilter(watched, event);
}

void DragSpinBox::mousePressEvent(QMouseEvent* event)
{
    handleMousePress(event);
    QDoubleSpinBox::mousePressEvent(event);
}

void DragSpinBox::mouseMoveEvent(QMouseEvent* event)
{
    handleMouseMove(event);
    if (!m_dragging) {
        QDoubleSpinBox::mouseMoveEvent(event);
    }
}

void DragSpinBox::mouseReleaseEvent(QMouseEvent* event)
{
    if (!handleMouseRelease(event)) {
        QDoubleSpinBox::mouseReleaseEvent(event);
    }
}

void DragSpinBox::wheelEvent(QWheelEvent* event)
{
    event->ignore();
}

void DragSpinBox::handleMousePress(QMouseEvent* event)
{
    if (event->button() == Qt::LeftButton) {
        m_dragging = false;
        m_dragStartX = event->globalPosition().toPoint().x();
        m_dragStartValue = value();
    }
}

void DragSpinBox::handleMouseMove(QMouseEvent* event)
{
    if (!(event->buttons() & Qt::LeftButton)) {
        return;
    }

    int dx = event->globalPosition().toPoint().x() - m_dragStartX;

    if (!m_dragging) {
        if (std::abs(dx) < kDragThreshold) {
            return;
        }
        m_dragging = true;
        m_dragStartX = event->globalPosition().toPoint().x();
        m_dragStartValue = value();
        dx = 0;
    }

    double sensitivity = kSensitivity;
    if (QApplication::keyboardModifiers() & Qt::ShiftModifier) {
        sensitivity *= 0.1;
    } else if (QApplication::keyboardModifiers() & Qt::ControlModifier) {
        sensitivity *= 10.0;
    }

    double delta = dx * singleStep() * sensitivity;
    double newValue = m_dragStartValue + delta;

    if (newValue < minimum()) newValue = minimum();
    if (newValue > maximum()) newValue = maximum();

    setValue(newValue);
}

bool DragSpinBox::handleMouseRelease(QMouseEvent* event)
{
    if (m_dragging && event->button() == Qt::LeftButton) {
        m_dragging = false;
        event->accept();
        return true;
    }
    return false;
}

} // namespace cakery
