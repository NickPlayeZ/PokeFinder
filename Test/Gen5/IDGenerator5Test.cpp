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

#include "IDGenerator5Test.hpp"
#include <Core/Enum/Game.hpp>
#include <Core/Gen5/Generators/IDGenerator5.hpp>
#include <Core/Gen5/Profile5.hpp>
#include <Core/Parents/States/IDState.hpp>
#include <QTest>
#include <Test/Data.hpp>
#include <Test/Enum.hpp>

static bool operator==(const IDState &left, const json &right)
{
    return left.getAdvances() == right["advances"].get<u32>() && left.getSID() == right["sid"].get<u16>()
        && left.getTID() == right["tid"].get<u16>() && left.getTSV() == right["tsv"].get<u16>();
}

void IDGenerator5Test::generate_data()
{
    QTest::addColumn<u64>("seed");
    QTest::addColumn<Game>("version");
    QTest::addColumn<std::string>("results");

    json data = readData("id5", "generate");
    for (const auto &d : data)
    {
        QTest::newRow(d["name"].get<std::string>().data())
            << d["seed"].get<u64>() << d["version"].get<Game>() << d["results"].get<json>().dump();
    }
}

void IDGenerator5Test::generate()
{
    QFETCH(u64, seed);
    QFETCH(Game, version);
    QFETCH(std::string, results);

    json j = json::parse(results);

    Profile5 profile("-", version, 12345, 54321, "", "", 0, { false, false, false, false, false, false, false, false, false }, 0, 0, 0,
                     false, 0, 0, false, false, DSType::DS, Language::English);

    IDFilter filter({ }, { }, { }, { }, { }, { });
    IDGenerator5 generator(0, 9, 0, false, false, profile, filter);

    auto states = generator.generate(seed);
    QCOMPARE(states.size(), j.size());

    for (size_t i = 0; i < states.size(); i++)
    {
        const auto &state = states[i];
        QVERIFY(state == j[i]);
    }
}

void IDGenerator5Test::generateResidents()
{
    Profile5 profile("-", Game::White, 12345, 54321, "", "", 0,
                     { false, false, false, false, false, false, false, false, false }, 0, 0, 0, false, 0, 0, false,
                     false, DSType::DS, Language::English);

    IDFilter filter({ }, { }, { }, { }, { }, { });
    IDGenerator5 generator(0, 0, 0, false, false, profile, filter, {}, {}, true);

    const auto states = generator.generate(0x844171e3c8952bb5);
    QCOMPARE(states.size(), 1);
    QCOMPARE(states[0].getAdvances(), 32);
    QCOMPARE(states[0].getTID(), 32676);
    QCOMPARE(states[0].getSID(), 14901);

    const std::array<u8, 10> residents = { 25, 22, 28, 15, 23, 18, 4, 5, 0, 24 };
    const std::array<u8, 3> entralinkResidents = { 12, 10, 16 };
    QVERIFY(states[0].getResidents() == residents);
    QVERIFY(states[0].getEntralinkResidents() == entralinkResidents);

    const auto secondStates = generator.generate(0xf4e21772f04081d0);
    QCOMPARE(secondStates.size(), 1);
    QCOMPARE(secondStates[0].getAdvances(), 28);
    QCOMPARE(secondStates[0].getTID(), 11407);
    QCOMPARE(secondStates[0].getSID(), 57293);

    const std::array<u8, 10> secondResidents = { 15, 22, 16, 5, 7, 23, 0, 18, 12, 21 };
    const std::array<u8, 3> secondEntralinkResidents = { 20, 28, 6 };
    QVERIFY(secondStates[0].getResidents() == secondResidents);
    QVERIFY(secondStates[0].getEntralinkResidents() == secondEntralinkResidents);
}
