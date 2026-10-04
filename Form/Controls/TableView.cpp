/*
 * This file is part of PokéFinder
 * Copyright (C) 2017-2024 by Admiral_Fish, bumba, and EzPzStreamz
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 3
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
 */

#include "TableView.hpp"
#include <QAction>
#include <QApplication>
#include <QBrush>
#include <QClipboard>
#include <QColor>
#include <QFileDialog>
#include <QHeaderView>
#include <QKeyEvent>
#include <QMenu>
#include <QSettings>
#include <QStyledItemDelegate>
#include <QTimer>

namespace
{
    class TargetMarkDelegate final : public QStyledItemDelegate
    {
    public:
        TargetMarkDelegate(TableView *tableView) : QStyledItemDelegate(tableView), tableView(tableView)
        {
        }

        void initStyleOption(QStyleOptionViewItem *option, const QModelIndex &index) const override
        {
            QStyledItemDelegate::initStyleOption(option, index);
            if (tableView->isTargetIndex(index))
            {
                QSettings settings;
                bool enabled = settings.value(QStringLiteral("settings/targetMarkEnabled"), true).toBool();
                if (enabled)
                {
                    QColor mark = settings.value(QStringLiteral("settings/targetMarkColor"), QColor(Qt::red)).value<QColor>();
                    int alpha = settings.value(QStringLiteral("settings/targetMarkAlpha"), 128).toInt();
                    bool alternate = tableView->alternatingRowColors() && index.row() % 2 != 0;
                    QColor base = option->palette.color(alternate ? QPalette::AlternateBase : QPalette::Base);
                    if (QVariant background = index.data(Qt::BackgroundRole); background.isValid())
                    {
                        base = qvariant_cast<QBrush>(background).color();
                    }

                    auto blend = [alpha](int foreground, int background) {
                        return (foreground * alpha + background * (255 - alpha)) / 255;
                    };
                    option->backgroundBrush = QColor(blend(mark.red(), base.red()), blend(mark.green(), base.green()),
                                                     blend(mark.blue(), base.blue()));
                }
            }
        }

    private:
        TableView *tableView;
    };
}

TableView::TableView(QWidget *parent) : QTableView(parent), primaryAction(nullptr), secondaryAction(nullptr)
{
    setItemDelegate(new TargetMarkDelegate(this));
    outputTXT = addAction(tr("Output Results to TXT"));
    outputCSV = addAction(tr("Output Results to CSV"));

    connect(outputTXT, &QAction::triggered, this, [this] { outputModel(); });
    connect(outputCSV, &QAction::triggered, this, [this] { outputModel(true); });

    QHeaderView *horizontal = this->horizontalHeader();
    horizontal->setSectionResizeMode(QHeaderView::Interactive);

    QHeaderView *vertical = this->verticalHeader();
    vertical->hide();

    QTimer::singleShot(200, this, [horizontal] {
        QSettings setting;
        horizontal->resizeSections(setting.value("settings/headerSize").value<QHeaderView::ResizeMode>());
    });
}

void TableView::setTargetAdvance(u32 advance)
{
    targetAdvance = advance;
    viewport()->update();
}

void TableView::clearTargetAdvance()
{
    targetAdvance.reset();
    viewport()->update();
}

bool TableView::isTargetIndex(const QModelIndex &index) const
{
    return targetAdvance.has_value() && index.isValid() && index.model()->index(index.row(), 0).data().toUInt() == *targetAdvance;
}

void TableView::setPrimaryAction(QAction *action)
{
    primaryAction = action;
}

void TableView::setSecondaryAction(QAction *action)
{
    secondaryAction = action;
}

void TableView::setModel(QAbstractItemModel *model)
{
    QTableView::setModel(model);
    connect(this->model(), &QAbstractItemModel::rowsInserted, this, [this] {
        QSettings setting;
        this->horizontalHeader()->resizeSections(setting.value("settings/headerSize").value<QHeaderView::ResizeMode>());
    });
}

void TableView::contextMenuEvent(QContextMenuEvent *event)
{
    if (model()->rowCount() != 0)
    {
        QModelIndex index = indexAt(event->pos());
        if (index.isValid())
        {
            selectRow(index.row());
            setCurrentIndex(index);
        }
        auto menuActions = actions();
        if (primaryAction != nullptr)
        {
            menuActions.removeAll(primaryAction);
            menuActions.removeAll(secondaryAction);
            menuActions.removeAll(outputTXT);
            menuActions.removeAll(outputCSV);
            menuActions.prepend(outputCSV);
            menuActions.prepend(outputTXT);
            if (secondaryAction != nullptr)
            {
                menuActions.prepend(secondaryAction);
            }
            menuActions.prepend(primaryAction);
        }
        QMenu::exec(menuActions, event->globalPos(), nullptr, this);
    }
}

void TableView::keyPressEvent(QKeyEvent *event)
{
    if ((event->key() == Qt::Key_C) && (event->modifiers() == Qt::ControlModifier))
    {
        setSelectionToClipBoard();
    }
    else
    {
        QTableView::keyPressEvent(event);
    }
}

void TableView::mouseDoubleClickEvent(QMouseEvent *event)
{
    if (event->type() == QMouseEvent::MouseButtonDblClick)
    {
        setSelectionToClipBoard();
    }
}

void TableView::outputModel(bool csv) const
{
    QString caption = tr(csv ? "Save Output to CSV" : "Save Output to TXT");
    QString filter = tr(csv ? "CSV File (*.csv);;All Files (*)" : "Text File (*.txt);;All Files (*)");

    QString fileName = QFileDialog::getSaveFileName(nullptr, caption, QDir::currentPath(), filter);

    if (fileName.isEmpty())
    {
        return;
    }

    QFile file(fileName);
    if (file.open(QIODevice::WriteOnly))
    {
        QAbstractItemModel *model = this->model();

        QTextStream ts(&file);
        int rows = model->rowCount();
        int columns = model->columnCount();

        for (int i = 0; i < columns; i++)
        {
            ts << model->headerData(i, Qt::Horizontal, Qt::DisplayRole).toString();
            if (i != columns - 1)
            {
                ts << (csv ? "," : "\t");
            }
        }
        ts << "\n";

        for (int i = 0; i < rows; i++)
        {
            for (int j = 0; j < columns; j++)
            {
                QString entry = model->data(model->index(i, j)).toString();
                ts << (entry.isEmpty() ? "-" : entry);
                if (j != columns - 1)
                {
                    ts << (csv ? "," : "\t");
                }
            }

            if (i != rows - 1)
            {
                ts << "\n";
            }
        }
    }
}

void TableView::resizeEvent(QResizeEvent *event)
{
    QSettings setting;
    this->horizontalHeader()->resizeSections(setting.value("settings/headerSize").value<QHeaderView::ResizeMode>());
    QTableView::resizeEvent(event);
}

void TableView::setSelectionToClipBoard()
{
    QModelIndexList indexes = this->selectionModel()->selectedIndexes();
    if (!indexes.isEmpty())
    {
        QString selectedText;

        for (auto i = 0; i < indexes.size(); i++)
        {
            QModelIndex current = indexes[i];
            QString text = current.data().toString();

            if (i + 1 < selectedIndexes().count())
            {
                QModelIndex next = indexes[i + 1];

                if (next.row() != current.row())
                {
                    text += "\n";
                }
                else
                {
                    text += "\t";
                }
            }
            selectedText += text;
        }

        QApplication::clipboard()->setText(selectedText);
    }
}
