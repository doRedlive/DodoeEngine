// do@Redlive

#include "MemoryPanel.h"
#include "framework/EditorContext.h"
#include "runtime/core/memory/memory.h"

#include <QVBoxLayout>
#include <QHeaderView>

namespace cakery {

static const char* kCategoryNames[] = {
    "Object",
    "Texture",
    "RenderCmd",
    "Resource",
    "String",
    "Container",
    "Misc"
};

MemoryPanel::MemoryPanel(EditorContext& ctx, QWidget* parent)
    : Panel(ctx, parent)
{
    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(3, 3, 3, 3);

    m_table = new QTableWidget(this);
    m_table->setColumnCount(4);
    m_table->setHorizontalHeaderLabels({"Category", "Current(B)", "Peak(B)", "Allocs"});
    m_table->horizontalHeader()->setStretchLastSection(true);
    m_table->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_table->setSelectionBehavior(QAbstractItemView::SelectRows);

    int catCount = static_cast<int>(dodoe::AllocCategory::Count);
    m_table->setRowCount(catCount);
    for (int i = 0; i < catCount; ++i) {
        m_table->setItem(i, 0, new QTableWidgetItem(kCategoryNames[i]));
        m_table->setItem(i, 1, new QTableWidgetItem("0"));
        m_table->setItem(i, 2, new QTableWidgetItem("0"));
        m_table->setItem(i, 3, new QTableWidgetItem("0"));
    }

    layout->addWidget(m_table);

    m_timer = new QTimer(this);
    QObject::connect(m_timer, &QTimer::timeout, this, &MemoryPanel::refresh);
    m_timer->start(1000);

    refresh();
}

void MemoryPanel::refresh() {
    int catCount = static_cast<int>(dodoe::AllocCategory::Count);
    for (int i = 0; i < catCount; ++i) {
        auto& stats = dodoe::Memory::GetStats(static_cast<dodoe::AllocCategory>(i));
        Size_t cur = stats.current_bytes.load(std::memory_order_relaxed);
        Size_t peak = stats.peak_bytes.load(std::memory_order_relaxed);
        UInt64 allocs = stats.alloc_count.load(std::memory_order_relaxed);

        m_table->item(i, 1)->setText(QString::number(cur));
        m_table->item(i, 2)->setText(QString::number(peak));
        m_table->item(i, 3)->setText(QString::number(allocs));
    }
}

} // namespace cakery
