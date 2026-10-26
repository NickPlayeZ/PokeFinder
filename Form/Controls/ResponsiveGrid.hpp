#ifndef RESPONSIVEGRID_HPP
#define RESPONSIVEGRID_HPP

#include <algorithm>
#include <vector>
#include <QEvent>
#include <QLayout>
#include <QResizeEvent>
#include <QShowEvent>
#include <QTimer>
#include <QWidget>

class ResponsiveGrid final : public QWidget
{
public:
    ResponsiveGrid(int rows, int columns, int maximumCellSize, int minimumCellSize, QWidget *parent = nullptr) :
        QWidget(parent), rows(rows), columns(columns), maximumCellSize(maximumCellSize), minimumCellSize(minimumCellSize),
        cellSize(maximumCellSize)
    {
        setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
        setFixedSize(gridSize(maximumCellSize));
    }

    void addWidget(QWidget *widget, int row, int column)
    {
        widget->setParent(this);
        widgets.push_back({ widget, row, column });
        positionWidget(widgets.back(), cellSize);
    }

    QSize sizeHint() const override { return gridSize(cellSize); }
    QSize minimumSizeHint() const override { return gridSize(minimumCellSize); }

    void resetSize()
    {
        setCellSize(maximumCellSize);
        if (QWidget *topLevel = window()) resizeWindow(topLevel);
    }

protected:
    bool eventFilter(QObject *watched, QEvent *event) override
    {
        if (watched == window() && event->type() == QEvent::Resize && !updating && !resizingWindow)
            updateForWindow(static_cast<QResizeEvent *>(event)->size());
        return QWidget::eventFilter(watched, event);
    }

    void showEvent(QShowEvent *event) override
    {
        QWidget::showEvent(event);
        QWidget *topLevel = window();
        if (!topLevel) return;
        extraWidth = std::max(0, topLevel->sizeHint().width() - sizeHint().width());
        extraHeight = std::max(0, topLevel->sizeHint().height() - sizeHint().height());
        if (topLevel->layout()) topLevel->layout()->setSizeConstraint(QLayout::SetNoConstraint);
        topLevel->setMinimumSize(extraWidth + minimumSizeHint().width(), extraHeight + minimumSizeHint().height());
        QTimer::singleShot(0, this, [this, topLevel] {
            setCellSize(maximumCellSize);
            resizeWindow(topLevel);
            topLevel->installEventFilter(this);
        });
    }

private:
    struct GridWidget
    {
        QWidget *widget;
        int row;
        int column;
    };

    QSize gridSize(int size) const
    {
        return { 12 + columns * size + (columns - 1), 12 + rows * size + (rows - 1) };
    }

    void positionWidget(const GridWidget &entry, int size)
    {
        entry.widget->setFixedSize(size, size);
        entry.widget->move(6 + entry.column * (size + 1), 6 + entry.row * (size + 1));
    }

    void setCellSize(int size)
    {
        size = std::clamp(size, minimumCellSize, maximumCellSize);
        if (size == cellSize && this->size() == gridSize(size)) return;
        updating = true;
        cellSize = size;
        setFixedSize(gridSize(size));
        for (const GridWidget &widget : widgets) positionWidget(widget, size);
        updating = false;
    }

    void updateForWindow(const QSize &windowSize)
    {
        const int availableWidth = windowSize.width() - extraWidth - 12 - (columns - 1);
        const int availableHeight = windowSize.height() - extraHeight - 12 - (rows - 1);
        setCellSize(std::min(availableWidth / columns, availableHeight / rows));
    }

    void resizeWindow(QWidget *topLevel)
    {
        const QSize target(extraWidth + width(), extraHeight + height());
        if (topLevel->size() == target) return;
        resizingWindow = true;
        topLevel->resize(target);
        resizingWindow = false;
    }

    int rows;
    int columns;
    int maximumCellSize;
    int minimumCellSize;
    int cellSize;
    int extraWidth = 0;
    int extraHeight = 0;
    std::vector<GridWidget> widgets;
    bool updating = false;
    bool resizingWindow = false;
};

#endif // RESPONSIVEGRID_HPP
