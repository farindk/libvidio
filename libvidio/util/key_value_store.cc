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

#include "key_value_store.h"
#include "libvidio/vidio.h"
#include <sstream>


std::string key_value_store::serialize_kv() const
{
  std::stringstream sstr;
  sstr << "kv\n";

  for (const auto& entry : m_items) {
    sstr << entry.first << '=';

    auto* n = std::get_if<uint32_t>(&entry.second);
    if (n) {
      sstr << *n << '\n';
      continue;
    }

    auto* s = std::get_if<std::string>(&entry.second);
    if (s) {
      sstr << *s << '\n';
      continue;
    }
  }

  return sstr.str();
}


std::string key_value_store::serialize_json() const
{
  std::stringstream sstr;
  sstr << "{ ";

  bool first = true;
  for (const auto& entry : m_items) {
    if (first) {
      first = false;
    }
    else {
      sstr << ", ";
    }

    sstr << '"' << entry.first << "\":";

    auto* n = std::get_if<uint32_t>(&entry.second);
    if (n) {
      sstr << *n;
      continue;
    }

    auto* s = std::get_if<std::string>(&entry.second);
    if (s) {
      sstr << '"' << *s << '"';
      continue;
    }
  }

  sstr << " }";

  return sstr.str();
}


vidio_error* key_value_store::deserialize(const std::string& str)
{
  m_items.clear();

  std::stringstream sstr(str);

  std::string header;
  std::getline(sstr, header);
  if (sstr.eof())
    return nullptr; // TODO error

  if (header != "kv") {
    return nullptr; // TODO error
  }

  for (;;) {
    std::string line;
    std::getline(sstr, line);
    if (sstr.eof())
      break;

    std::string::size_type p = line.find_first_of('=');
    if (p == std::string::npos) {
      return nullptr; // TODO error
    }

    std::string key = line.substr(0, p);
    std::string value = line.substr(p + 1);

    bool is_number = true;
    for (size_t i = 0; i < value.length(); i++) {
      if (!::isdigit(value[i])) {
        is_number = false;
        break;
      }
    }

    if (is_number) {
      m_items[key] = (uint32_t) std::stol(value);
    }
    else {
      m_items[key] = value;
    }
  }

  return nullptr;
}
