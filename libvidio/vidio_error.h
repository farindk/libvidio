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

#ifndef LIBVIDIO_VIDIO_ERROR_H
#define LIBVIDIO_VIDIO_ERROR_H

#include <libvidio/vidio.h>
#include <string>
#include <map>
#include <algorithm>


struct vidio_error
{
public:
  vidio_error() = default;

  vidio_error(vidio_error_code code, std::string msg) : m_code(code), m_msg(msg) {}

  ~vidio_error() {
    delete m_reason;
  }

  static const vidio_error* from_errno();

  void set_code(enum vidio_error_code code) { m_code = code; }

  void set_message_template(std::string msg) { m_msg = msg; }

  void set_arg(int n, std::string arg)
  {
    m_args[n] = arg;
    m_maxArg = std::max(m_maxArg, n + 1);
  }

  vidio_error_code get_code() const { return m_code; }

  const std::string& get_message_template() const { return m_msg; }

  std::string get_formatted_message() const;

  std::string get_arg(int n) const
  {
    auto iter = m_args.find(n);
    if (iter == m_args.end()) {
      return {};
    }
    else {
      return iter->second;
    }
  }

  int get_number_of_args() const
  {
    return m_maxArg;
  }

  void set_reason(const vidio_error* error) {
    delete m_reason;
    m_reason = error;
  }

  [[nodiscard]] const vidio_error* get_reason() const { return m_reason; }

private:
  vidio_error_code m_code = vidio_error_code_success;
  std::string m_msg;
  std::map<int, std::string> m_args;
  int m_maxArg = 0;

  const vidio_error* m_reason = nullptr;
};

#endif //LIBVIDIO_VIDIO_ERROR_H
