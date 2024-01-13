/*
 * VidIO library
 * Copyright (c) 2024 Dirk Farin <dirk.farin@gmail.com>
 *
 * This file is part of libvidio.
 *
 * libvidio is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as
 * published by the Free Software Foundation, either version 3 of
 * the License, or (at your option) any later version.
 *
 * libvidio is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program. If not, see <https://www.gnu.org/licenses/>.
 */

#ifndef LIBVIDIO_KEY_VALUE_STORE_H
#define LIBVIDIO_KEY_VALUE_STORE_H

#include <string>
#include <variant>
#include <map>

struct vidio_error;


class key_value_store
{
public:
  void set(const std::string& key, const std::string& value) { m_items[key] = value; }

  void set(const std::string& key, uint32_t value) { m_items[key] = value; }

  bool has(const std::string& key) const {
    return m_items.find(key) != m_items.end();
  }

  template <typename T> std::string get(const std::string& key) const {
    auto iter = m_items.find(key);
    if (iter == m_items.end()) {
      return {};
    }
    else {
      return std::get<T>(iter->second);
    }
  }

  std::string serialize_kv() const;

  std::string serialize_json() const;

  vidio_error* deserialize(const std::string&);

private:
  typedef std::variant<std::string, uint32_t> item;

  std::map<std::string, item> m_items;
};


#endif //LIBVIDIO_KEY_VALUE_STORE_H
