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

#include "vidio_error.h"
#include <regex>
#include <errno.h>

std::string vidio_error::get_formatted_message() const
{
  if (m_maxArg==0) {
    return m_msg;
  }

  std::string msg = m_msg;

  for (int i=0;i<m_maxArg;i++) {
    std::stringstream sstr;
    sstr << "\\{" << i << "\\}";
    msg = std::regex_replace(msg, std::regex(sstr.str()), get_arg(i));
  }

  return msg;
}


const vidio_error* vidio_error::from_errno()
{
  auto* err = new vidio_error(vidio_error_code_errno, strerror(errno));
  return err;
}
