#pragma once

#include <QWidget>
#include <QDragEnterEvent>
#include <QDragMoveEvent>
#include <QDropEvent>
#include <QMimeData>

namespace cakery {

class CameraController;

class SceneWidget : public QWidget {
    Q_OBJECT
public:
    explicit SceneWidget(QWidget* parent = nullptr);
    ~SceneWidget() override;

protected:
    void showEvent(QShowEvent* event) override;
    void paintEvent(QPaintEvent* event) override;
    void resizeEvent(QResizeEvent* event) override;

    void mousePressEvent(QMouseEvent* event) override;
    void mouseReleaseEvent(QMouseEvent* event) override;
    void mouseMoveEvent(QMouseEvent* event) override;
    void wheelEvent(QWheelEvent* event) override;

    void dragEnterEvent(QDragEnterEvent* event) override;
    void dragMoveEvent(QDragMoveEvent* event) override;
    void dropEvent(QDropEvent* event) override;

private:
    void setupCamera();
    QStringList extractAssetPaths(const QMimeData* mime) const;
    void createEntityFromAsset(const QString& filePath);

    CameraController* m_camera = nullptr;
    bool m_firstShow = true;
};

} // namespace cakery
