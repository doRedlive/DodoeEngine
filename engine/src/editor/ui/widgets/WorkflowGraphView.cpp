// do@Redlive

#include "ui/common/QtStringInterop.h"

#include "dopch.h"

#include "ui/widgets/WorkflowGraphView.h"

#include <QBrush>
#include <QFont>
#include <QGraphicsPathItem>
#include <QGraphicsSceneMouseEvent>
#include <QGraphicsTextItem>
#include <QMouseEvent>
#include <QPainterPath>
#include <QPen>
#include <QScrollBar>
#include <QWheelEvent>

#include <functional>
#include <string>
#include <unordered_set>

namespace cakery {

namespace {

constexpr double kColumnWidth = 180.0;
constexpr double kNodeWidth = 150.0;
constexpr double kNodeHeight = 50.0;
constexpr double kLaneHeight = 110.0;
constexpr double kMarginX = 160.0;
constexpr double kMarginY = 24.0;

String UuidFromData(const Json& node)
{
    if (node.contains("data") && node["data"].is_object()) {
        const auto it = node["data"].find("uuid");
        if (it != node["data"].end() && it->is_string()) {
            return it->get<String>();
        }
    }
    return {};
}

class WorkflowNodeItem final : public QGraphicsRectItem {
public:
    explicit WorkflowNodeItem(const QRectF& rect, QGraphicsItem* parent = nullptr)
        : QGraphicsRectItem(rect, parent) {}

    std::function<void()> onPressed;
    std::function<void()> onDoublePressed;

protected:
    void mousePressEvent(QGraphicsSceneMouseEvent* event) override {
        if (event->button() == Qt::LeftButton && onPressed) {
            onPressed();
        }
        QGraphicsRectItem::mousePressEvent(event);
    }
    void mouseDoubleClickEvent(QGraphicsSceneMouseEvent* event) override {
        if (event->button() == Qt::LeftButton && onDoublePressed) {
            onDoublePressed();
        }
        QGraphicsRectItem::mouseDoubleClickEvent(event);
    }
};

} // anonymous namespace

WorkflowGraphView::WorkflowGraphView(QWidget* parent)
    : QGraphicsView(parent)
{
    m_scene = new QGraphicsScene(this);
    setScene(m_scene);
    setRenderHint(QPainter::Antialiasing);
    // Let the viewport use the application palette and workspace stylesheet.
    setTransformationAnchor(AnchorUnderMouse);
}

void WorkflowGraphView::wheelEvent(QWheelEvent* event)
{
    const double factor = event->angleDelta().y() > 0 ? 1.15 : 1.0 / 1.15;
    scale(factor, factor);
    QGraphicsView::wheelEvent(event);
}

void WorkflowGraphView::highlightNode(const UInt64 sequence)
{
    for (const auto& [seq, item] : m_itemsBySeq) {
        const auto penIt = m_normalPens.find(seq);
        if (penIt != m_normalPens.end()) {
            item->setPen(penIt->second);
        }
    }
    m_selectedSequence = sequence;
    const auto it = m_itemsBySeq.find(sequence);
    if (it != m_itemsBySeq.end()) {
        it->second->setPen(QPen(palette().highlight().color(), 2.4));
    }
}

void WorkflowGraphView::setGraph(const Json& graph)
{
    const QScrollBar* hscroll = horizontalScrollBar();
    const bool wasAtRight = !hscroll || hscroll->value() >= hscroll->maximum() - 20;

    m_scene->clear();
    m_itemsBySeq.clear();
    m_normalPens.clear();

    const bool hasContent = graph.is_object() &&
        graph.contains("nodes") && graph["nodes"].is_array() && !graph["nodes"].empty();

    if (!hasContent) {
        auto* hint = m_scene->addText(tr("Run commands to build the workflow graph"));
        hint->setDefaultTextColor(palette().placeholderText().color());
        setSceneRect(hint->boundingRect().adjusted(-40, -20, 40, 20));
        fitInView(sceneRect(), Qt::KeepAspectRatio);
        m_hadContent = false;
        return;
    }

    const Json& nodes = graph["nodes"];
    const Json edges = graph.value("edges", Json::array());
    const Json sessions = graph.value("sessions", Json::array());

    UnorderedMap<String, int> laneOf;
    laneOf.emplace(String(), 0);
    int laneCount = 1;
    for (const auto& node : nodes) {
        const String session = node.value("session", String());
        if (!laneOf.contains(session)) {
            laneOf.emplace(session, laneCount++);
        }
    }

    const double width = kMarginX + static_cast<double>(nodes.size()) * kColumnWidth;
    const double height = kMarginY + static_cast<double>(laneCount) * kLaneHeight;

    for (int lane = 0; lane < laneCount; ++lane) {
        if (lane % 2 == 1) {
            auto* band = m_scene->addRect(kMarginX - 150.0, kMarginY + lane * kLaneHeight,
                                          width, kLaneHeight, QPen(Qt::NoPen),
                                          QBrush(QColor(255, 255, 255, 8)));
            band->setZValue(-2);
        }
        QString label = QStringLiteral("User");
        if (lane > 0) {
            for (const auto& session : sessions) {
                if (!session.is_object()) {
                    continue;
                }
                const auto laneIt = laneOf.find(session.value("id", String()));
                if (laneIt != laneOf.end() && laneIt->second == lane) {
                    label = QStringLiteral("AI: ") + ToQString(session.value("id", String()));
                    break;
                }
            }
        }
        auto* laneLabel = m_scene->addText(label);
        laneLabel->setDefaultTextColor(palette().placeholderText().color());
        laneLabel->setPos(8.0, kMarginY + lane * kLaneHeight + kLaneHeight / 2.0 - 10.0);
        laneLabel->setZValue(-1);
    }

    struct NodePosition {
        double x = 0.0;
        double y = 0.0;
    };
    UnorderedMap<UInt64, NodePosition> positions;
    Size_t index = 0;
    for (const auto& node : nodes) {
        const UInt64 seq = node.value("sequence", UInt64(0));
        const int lane = laneOf[node.value("session", String())];
        positions[seq] = {kMarginX + static_cast<double>(index) * kColumnWidth,
                          kMarginY + lane * kLaneHeight + (kLaneHeight - kNodeHeight) / 2.0};
        ++index;
    }

    for (const auto& edge : edges) {
        if (!edge.is_object()) {
            continue;
        }
        const auto fromIt = positions.find(edge.value("from", UInt64(0)));
        const auto toIt = positions.find(edge.value("to", UInt64(0)));
        if (fromIt == positions.end() || toIt == positions.end()) {
            continue;
        }
        const QPointF start(fromIt->second.x + kNodeWidth, fromIt->second.y + kNodeHeight / 2.0);
        const QPointF end(toIt->second.x, toIt->second.y + kNodeHeight / 2.0);
        const double midX = (start.x() + end.x()) / 2.0;
        QPainterPath path(start);
        path.cubicTo(QPointF(midX, start.y()), QPointF(midX, end.y()), end);
        auto* edgeItem = m_scene->addPath(path, QPen(palette().midlight().color(), 1.5));
        edgeItem->setZValue(-1);
        edgeItem->setToolTip(QStringLiteral("artifact ") +
                             ToQString(edge.value("artifact", String())));
    }

    for (const auto& node : nodes) {
        const UInt64 seq = node.value("sequence", UInt64(0));
        const auto posIt = positions.find(seq);
        if (posIt == positions.end()) {
            continue;
        }
        const String command = node.value("command", String());
        const bool ok = node.value("ok", false);
        const bool marker = command == "session.begin" || command == "session.end";

        auto* item = new WorkflowNodeItem(
            QRectF(posIt->second.x, posIt->second.y, kNodeWidth, kNodeHeight));
        QPen pen(marker ? palette().placeholderText().color()
                        : (ok ? QColor("#50fa7b") : QColor("#ff5555")));
        pen.setWidthF(1.4);
        pen.setStyle(marker ? Qt::DashLine : Qt::SolidLine);
        item->setPen(pen);
        item->setBrush(palette().button());
        QString tooltip = ToQString(command);
        if (node.contains("message") && node["message"].is_string()) {
            tooltip += QStringLiteral("\n") + ToQString(node.value("message", String()));
        }
        item->setToolTip(tooltip);
        m_scene->addItem(item);
        m_normalPens[seq] = pen;

        auto* label = new QGraphicsTextItem(item);
        label->setPlainText(ToQString(command));
        label->setDefaultTextColor(marker ? palette().placeholderText().color() : palette().text().color());
        QFont font = label->font();
        font.setPointSizeF(font.pointSizeF() - 1.0);
        label->setFont(font);
        label->setTextWidth(kNodeWidth - 12.0);
        label->setPos(6.0, 4.0);

        auto* sub = new QGraphicsTextItem(item);
        sub->setPlainText(ToQString(node.value("author", String())) +
                          QStringLiteral("  ·  ") +
                          QString::number(node.value("duration_ms", 0.0), 'f', 1) + QStringLiteral(" ms"));
        sub->setDefaultTextColor(palette().placeholderText().color());
        QFont subFont = sub->font();
        subFont.setPointSizeF(subFont.pointSizeF() - 2.0);
        sub->setFont(subFont);
        sub->setPos(6.0, 26.0);

        const String uuid = UuidFromData(node);
        item->onPressed = [this, seq]() { emit nodeSelected(seq); };
        item->onDoublePressed = [this, seq, uuid]() {
            emit nodeSelected(seq);
            if (!uuid.empty()) {
                emit nodeActivated(seq, ToQString(uuid));
            }
        };
        m_itemsBySeq[seq] = item;
    }

    setSceneRect(0.0, 0.0, width + 40.0, height + 20.0);
    if (!m_hadContent) {
        fitInView(sceneRect(), Qt::KeepAspectRatio);
    } else if (wasAtRight) {
        horizontalScrollBar()->setValue(horizontalScrollBar()->maximum());
    }
    m_hadContent = true;
    if (m_selectedSequence != 0) {
        highlightNode(m_selectedSequence);
    }
}

} // namespace cakery
