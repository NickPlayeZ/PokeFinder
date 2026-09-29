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

#ifndef PHENOMENONITEM_HPP
#define PHENOMENONITEM_HPP

#include <Core/Enum/Encounter.hpp>
#include <QWidget>
#include <vector>

class PhenomenonArea;
class PhenomenonSearcherModel5;
class Profile5;
class SortFilterProxyModel;

namespace Ui
{
    class PhenomenonItem;
}

/**
 * @brief Provides settings and filters to RNG phenomenon in Gen 5
 */
class PhenomenonItem : public QWidget
{
    Q_OBJECT
signals:
    /**
     * @brief Emits that the profiles have been changed
     */
    void profilesChanged(int);

    /**
     * @brief Requests opening the normal phenomenon generator for a selected result
     */
    void openGenerator(const Profile5 &profile, Encounter encounter, u8 location, u64 seed);

public:
    /**
     * @brief Construct a new Phenomenon object
     *
     * @param parent Parent widget, which takes memory ownership
     */
    PhenomenonItem(QWidget *parent = nullptr);

    /**
     * @brief Destroy the Phenomenon object
     */
    ~PhenomenonItem() override;

    /**
     * @brief Determines if any profiles exist
     *
     * @return true At least 1 profile exists
     * @return false 0 profiles exist
     */
    bool hasProfiles() const;

public slots:
    /**
     * @brief Reloads profiles
     */
    void updateProfiles();

private:
    Ui::PhenomenonItem *ui;

    PhenomenonSearcherModel5 *searcherModel;
    const Profile5 *currentProfile;
    SortFilterProxyModel *proxyModel;
    std::vector<PhenomenonArea> encounter;

private slots:
    /**
     * @brief Searches phenomenon encounters from date range
     */
    void search();

    /**
     * @brief Opens adjacent seeds for the selected search result
     */
    void openAdjacentSeeds();

    /**
     * @brief Opens the selected result in the normal phenomenon generator
     */
    void openPhenomenonGenerator();

    /**
     * @brief Updates the location list for the selected encounter type
     *
     * @param index Encounter index
     */
    void searcherEncounterIndexChanged(int index);

    /**
     * @brief Updates the phenomenon listed
     *
     * @param index Location index
     */
    void searcherLocationIndexChanged(int index);

    /**
     * @brief Updates showing profile related information
     *
     * @param profile Selected profile
     */
    void profileChanged(const Profile5 &profile);

};

#endif // PHENOMENONITEM_HPP
