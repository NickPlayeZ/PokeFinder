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

#ifndef STACKVECTOR_HPP
#define STACKVECTOR_HPP

#include <cstddef>

// Stand-in until std::inplace_vector
template <typename T, size_t N>
class StackVector
{
public:
    using value_type = T;
    using reference = T &;
    using const_reference = const T &;
    using size_type = size_t;

    auto begin()
    {
        return data;
    }

    auto begin() const
    {
        return data;
    }

    size_t capacity() const
    {
        return N;
    }

    auto end()
    {
        return data + m_size;
    }

    auto end() const
    {
        return data + m_size;
    }

    void fill(const T &value)
    {
        for (T &item : data)
        {
            item = value;
        }
        m_size = N;
    }

    void push_back(const T &value)
    {
        data[m_size++] = value;
    }

    size_t size() const
    {
        return m_size;
    }

    T &operator[](size_t index)
    {
        return data[index];
    }

    const T &operator[](size_t index) const
    {
        return data[index];
    }

private:
    T data[N];
    size_t m_size = 0;
};

#endif // STACKVECTOR_HPP
