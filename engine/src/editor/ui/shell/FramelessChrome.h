// do@Redlive
#pragma once
#include "dopch.h"

#include <QDialog>
#include <QWidget>

#include <QMessageBox>

class QLabel;
class QToolButton;

namespace ads {
class CFloatingDockContainer;
}

namespace cakery {

class FloatingWindowTitleBar final : public QWidget {
public:
    explicit FloatingWindowTitleBar(QWidget* window, bool allowMaximize = true);

protected:
    void mousePressEvent(QMouseEvent* event) override;
    void mouseDoubleClickEvent(QMouseEvent* event) override;

private:
    void toggleMaximize();
    void updateMaximizeButton();

    QWidget* m_window = nullptr;
    QLabel* m_title = nullptr;
    QToolButton* m_maxButton = nullptr;
    bool m_allowMaximize = true;
};

class UnsavedChangesDialog final : public QDialog {
public:
    explicit UnsavedChangesDialog(QWidget* parent);

    QMessageBox::StandardButton result() const;

private:
    QMessageBox::StandardButton m_result = QMessageBox::Cancel;
};

void setupFramelessMessageBox(QMessageBox* messageBox);
ads::CFloatingDockContainer* floatingWindowAncestor(QObject* object);
void applyWindowsFrameAttributes(QWidget* widget);

} // namespace cakery
