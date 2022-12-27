/*
 * This file is part of PokéFinder
 * Copyright (C) 2017-2022 by Admiral_Fish, bumba, and EzPzStreamz
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

#include "IDsFilter.hpp"
#include "ui_IDsFilter.h"
#include <Core/Parents/Filters/IDFilter.hpp>
#include <QRegularExpression>

IDsFilter::IDsFilter(QWidget *parent) : QWidget(parent), ui(new Ui::IDsFilter)
{
    ui->setupUi(this);
    connect(ui->plainTextEditIDs, &QPlainTextEdit::textChanged, this, &IDsFilter::textEditIDsTextChanged);
    connect(ui->plainTextEditTSV, &QPlainTextEdit::textChanged, this, &IDsFilter::textEditTSVTextChanged);
}

IDsFilter::~IDsFilter()
{
    delete ui;
}

IDFilter IDsFilter::getFilter() const
{
    std::vector<u16> tidFilter;
    std::vector<u16> sidFilter;
    std::vector<u16> tsvFilter;
    std::vector<u32> displayFilter;

    QStringList inputs = ui->plainTextEditIDs->toPlainText().split('\n');
    if (ui->radioButtonTID->isChecked())
    {
        for (const QString &input : inputs)
        {
            if (!input.isEmpty())
            {
                tidFilter.emplace_back(input.toUShort());
            }
        }
    }
    else if (ui->radioButtonSID->isChecked())
    {
        for (const QString &input : inputs)
        {
            if (!input.isEmpty())
            {
                sidFilter.emplace_back(input.toUShort());
            }
        }
    }
    else if (ui->radioButtonTIDSID->isChecked())
    {
        for (const QString &input : inputs)
        {
            if (!input.isEmpty())
            {
                QStringList ids = input.split('/');
                tidFilter.emplace_back(ids[0].toUShort());
                sidFilter.emplace_back(ids[1].toUShort());
            }
        }
    }
    else
    {
        for (const QString &input : inputs)
        {
            if (!input.isEmpty())
            {
                displayFilter.emplace_back(input.toUInt());
            }
        }
    }

    inputs = ui->plainTextEditTSV->toPlainText().split('\n');
    for (const QString &input : inputs)
    {
        if (!input.isEmpty())
        {
            sidFilter.emplace_back(input.toUShort());
        }
    }

    return IDFilter(tidFilter, sidFilter, tsvFilter, displayFilter);
}

void IDsFilter::enableDisplayTID(bool flag)
{
    ui->radioButtonDisplayTID->setVisible(flag);
}

void IDsFilter::textEditIDsTextChanged()
{
    QStringList inputs = ui->plainTextEditIDs->toPlainText().split('\n');
    if (ui->radioButtonTID->isChecked() || ui->radioButtonSID->isChecked())
    {
        QRegularExpression filter("[^0-9]");
        for (QString &input : inputs)
        {
            input.remove(filter);
            if (!input.isEmpty())
            {
                bool flag;
                u16 val = input.toUShort(&flag);
                val = flag ? val : 65535;
                input = QString::number(val);
            }
        }
    }
    else if (ui->radioButtonDisplayTID->isChecked())
    {
        QRegularExpression filter("[^0-9]");
        for (QString &input : inputs)
        {
            input.remove(filter);
            if (!input.isEmpty())
            {
                bool flag;
                u32 val = input.toUShort(&flag);
                val = (flag && val <= 999999) ? val : 999999;
                input = QString::number(val);
            }
        }
    }
    else
    {
        QRegularExpression filter = QRegularExpression("[^0-9/]");
        for (QString &input : inputs)
        {
            input.remove(filter);
            // Only allow a single '/' character
            while (input.count('/') > 1)
            {
                input.remove(input.lastIndexOf('/'), 1);
            }

            if (!input.isEmpty())
            {
                QStringList ids = input.split('/');

                bool flag;
                u16 tid = ids[0].toUShort(&flag);
                tid = flag ? tid : 65535;

                if (ids.size() == 1)
                {
                    if (input.contains('/'))
                    {
                        input = QString("%1/").arg(tid);
                    }
                    else
                    {
                        input = QString::number(tid);
                    }
                }
                else if (!ids[1].isEmpty())
                {
                    u16 sid = ids[1].toUShort(&flag);
                    sid = flag ? sid : 65535;
                    input = QString("%1/%2").arg(tid).arg(sid);
                }
            }
        }
    }

    // Block signals so this function doesn't get called recursively
    // We also grab the cursor position and restore as setting the text resets it
    ui->plainTextEditIDs->blockSignals(true);
    QTextCursor cursor = ui->plainTextEditIDs->textCursor();
    ui->plainTextEditIDs->setPlainText(inputs.join('\n'));
    ui->plainTextEditIDs->setTextCursor(cursor);
    ui->plainTextEditIDs->blockSignals(false);
}

void IDsFilter::textEditTSVTextChanged()
{
    QStringList inputs = ui->plainTextEditTSV->toPlainText().split('\n');
    QRegularExpression filter("[^0-9]");
    for (QString &input : inputs)
    {
        input.remove(filter);
        if (!input.isEmpty())
        {
            bool flag;
            u16 tsv = input.toUShort(&flag);
            tsv = (flag && tsv <= 8191) ? tsv : 8191;
            input = QString::number(tsv);
        }
    }

    // Block signals so this function doesn't get called recursively
    // We also grab the cursor position and restore as setting the text resets it
    ui->plainTextEditTSV->blockSignals(true);
    QTextCursor cursor = ui->plainTextEditTSV->textCursor();
    ui->plainTextEditTSV->setPlainText(inputs.join('\n'));
    ui->plainTextEditTSV->setTextCursor(cursor);
    ui->plainTextEditTSV->blockSignals(false);
}
