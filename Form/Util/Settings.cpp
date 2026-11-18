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

#include "Settings.hpp"
#include "ui_Settings.h"
#include <Core/Parents/ProfileLoader.hpp>
#include <Form/Controls/TableView.hpp>
#include <QApplication>
#include <QColorDialog>
#include <QFileDialog>
#include <QHeaderView>
#include <QMessageBox>
#include <QProcess>
#include <QSettings>
#include <QThread>

Settings::Settings(QWidget *parent) : QWidget(parent), ui(new Ui::Settings)
{
    ui->setupUi(this);
    setAttribute(Qt::WA_QuitOnClose, false);
    setAttribute(Qt::WA_DeleteOnClose);

    QSettings setting;
    setting.beginGroup("settings");

    // Language
    QString language = setting.value("locale").toString();
    QStringList languages = { "zh", "en", "fr", "de", "it", "ja", "ko", "es" };
    for (int i = 0; i < languages.size(); i++)
    {
        const QString &lang = languages[i];
        ui->comboBoxLanguage->setItemData(i, lang);
        if (language == lang)
        {
            ui->comboBoxLanguage->setCurrentIndex(i);
        }
    }

    // Profiles
    QString profile = setting.value("profiles").toString();
    ui->lineEditProfiles->setText(profile);

    // Style
    QString style = setting.value("style").toString();
    QStringList styles = { "auto", "dark", "light" };
    for (int i = 0; i < styles.size(); i++)
    {
        const QString &sty = styles[i];
        ui->comboBoxStyle->setItemData(i, sty);
        if (style == sty)
        {
            ui->comboBoxStyle->setCurrentIndex(i);
        }
    }

    // Table header size
    auto size = setting.value("headerSize").value<QHeaderView::ResizeMode>();
    std::array<QHeaderView::ResizeMode, 2> sizes = { QHeaderView::ResizeToContents, QHeaderView::Stretch };
    for (int i = 0; i < sizes.size(); i++)
    {
        auto s = sizes[i];
        ui->comboBoxTableHeaderSize->setItemData(i, s);
        if (size == s)
        {
            ui->comboBoxTableHeaderSize->setCurrentIndex(i);
        }
    }

    // Threads
    int threads = setting.value("threads").toInt();
    for (int i = 1; i <= QThread::idealThreadCount(); i++)
    {
        ui->comboBoxThreads->addItem(QString::number(i), i);
        if (i == threads)
        {
            ui->comboBoxThreads->setCurrentIndex(i - 1);
        }
    }

    // Target mark
    ui->comboBoxTargetMarkEnabled->setItemData(0, true);
    ui->comboBoxTargetMarkEnabled->setItemData(1, false);
    bool targetMarkEnabled = setting.value("targetMarkEnabled", true).toBool();
    ui->comboBoxTargetMarkEnabled->setCurrentIndex(targetMarkEnabled ? 0 : 1);
    int targetMarkAlpha = setting.value("targetMarkAlpha", 128).toInt();
    ui->spinBoxTargetMarkTransparency->setValue(qRound((255 - targetMarkAlpha) * 100.0 / 255.0));
    ui->pushButtonTargetMarkColor->setEnabled(targetMarkEnabled);
    ui->spinBoxTargetMarkTransparency->setEnabled(targetMarkEnabled);
    updateTargetMarkButton();

    setting.endGroup();

    connect(ui->comboBoxLanguage, &QComboBox::currentIndexChanged, this, &Settings::languageIndexChanged);
    connect(ui->pushButtonProfile, &QPushButton::clicked, this, &Settings::changeProfiles);
    connect(ui->comboBoxStyle, &QComboBox::currentIndexChanged, this, &Settings::styleIndexChanged);
    connect(ui->comboBoxTableHeaderSize, &QComboBox::currentIndexChanged, this, &Settings::tableHeaderSizeIndexChanged);
    connect(ui->comboBoxThreads, &QComboBox::currentIndexChanged, this, &Settings::threadsIndexChanged);
    connect(ui->pushButtonTargetMarkColor, &QPushButton::clicked, this, &Settings::changeTargetMarkColor);
    connect(ui->comboBoxTargetMarkEnabled, &QComboBox::currentIndexChanged, this, &Settings::targetMarkEnabledChanged);
    connect(ui->spinBoxTargetMarkTransparency, &QSpinBox::valueChanged, this, &Settings::targetMarkTransparencyChanged);

    if (setting.contains("settingsForm/geometry"))
    {
        this->restoreGeometry(setting.value("settingsForm/geometry").toByteArray());
    }
}

void Settings::updateTargetMarkButton()
{
    QSettings setting;
    QColor color = setting.value("settings/targetMarkColor", QColor(Qt::red)).value<QColor>();
    ui->pushButtonTargetMarkColor->setStyleSheet(
        QStringLiteral("QPushButton { background-color: %1; }").arg(color.name(QColor::HexRgb)));
}

void Settings::updateTargetMarkTables()
{
    for (QWidget *widget : QApplication::allWidgets())
    {
        if (auto *tableView = qobject_cast<TableView *>(widget))
        {
            tableView->viewport()->update();
        }
    }
}

void Settings::changeTargetMarkColor()
{
    QSettings setting;
    QColor current = setting.value("settings/targetMarkColor", QColor(Qt::red)).value<QColor>();
    QColor color = QColorDialog::getColor(current, this, tr("Change Mark Color"));
    if (color.isValid())
    {
        setting.setValue("settings/targetMarkColor", color);
        updateTargetMarkButton();
        updateTargetMarkTables();
    }
}

void Settings::targetMarkEnabledChanged(int index)
{
    bool enabled = ui->comboBoxTargetMarkEnabled->itemData(index).toBool();
    QSettings setting;
    setting.setValue("settings/targetMarkEnabled", enabled);
    ui->pushButtonTargetMarkColor->setEnabled(enabled);
    ui->spinBoxTargetMarkTransparency->setEnabled(enabled);
    updateTargetMarkTables();
}

void Settings::targetMarkTransparencyChanged(int transparency)
{
    QSettings setting;
    setting.setValue("settings/targetMarkAlpha", qRound((100 - transparency) * 255.0 / 100.0));
    updateTargetMarkTables();
}

Settings::~Settings()
{
    QSettings setting;
    setting.setValue("settingsForm/geometry", this->saveGeometry());

    delete ui;
}

void Settings::changeProfiles()
{
    QString fileName = QFileDialog::getSaveFileName(this, "Select Profile json", QDir::currentPath(), "json (*.json)");
    if (!fileName.isEmpty())
    {
        if (!QFile::exists(fileName))
        {
            QFile f(fileName);
            if (!f.open(QIODevice::WriteOnly))
            {
                QMessageBox msg(QMessageBox::Information, tr("Profile File"), tr("There was a problem creating the file"));
                msg.exec();
                return;
            }
        }

        QSettings setting;
        setting.setValue("settings/profiles", fileName);

        ProfileLoader::init(fileName.toStdWString());

        ui->lineEditProfiles->setText(fileName);
    }
}

void Settings::languageIndexChanged(int index)
{
    if (index >= 0)
    {
        QSettings setting;
        QString currentLanguage = setting.value("settings/locale").toString();
        QString language = ui->comboBoxLanguage->currentData().toString();

        if (currentLanguage != language)
        {
            setting.setValue("settings/locale", language);

            QMessageBox msg(QMessageBox::Question, tr("Language update"), tr("Restart for changes to take effect. Restart now?"),
                            QMessageBox::Yes | QMessageBox::No);
            if (msg.exec() == QMessageBox::Yes)
            {
                QProcess::startDetached(QApplication::applicationFilePath());
                QApplication::quit();
            }
        }
    }
}

void Settings::styleIndexChanged(int index)
{
    if (index >= 0)
    {
        QSettings setting;
        QString currentStyle = setting.value("settings/style").toString();
        QString style = ui->comboBoxStyle->currentData().toString();
        if (currentStyle != style)
        {
            setting.setValue("settings/style", style);

            QMessageBox msg(QMessageBox::Question, tr("Style change"), tr("Restart for changes to take effect. Restart now?"),
                            QMessageBox::Yes | QMessageBox::No);
            if (msg.exec() == QMessageBox::Yes)
            {
                QProcess::startDetached(QApplication::applicationFilePath());
                QApplication::quit();
            }
        }
    }
}

void Settings::tableHeaderSizeIndexChanged(int index)
{
    if (index >= 0)
    {
        QSettings setting;
        setting.setValue("settings/headerSize", ui->comboBoxTableHeaderSize->currentData());
    }
}

void Settings::threadsIndexChanged(int index)
{
    if (index >= 0)
    {
        QSettings setting;
        setting.setValue("settings/threads", ui->comboBoxThreads->currentData().toInt());
    }
}
