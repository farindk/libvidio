/*
 * VidIO library
 * Copyright (c) 2023 Dirk Farin <dirk.farin@gmail.com>
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

#ifndef LIBVIDIO_VIDIO_INPUT_H
#define LIBVIDIO_VIDIO_INPUT_H

#include <libvidio/vidio.h>
#include <string>
#include <vector>


struct vidio_input
{
public:
  virtual ~vidio_input() = default;

  virtual vidio_input_source get_source() const = 0;

  virtual std::string get_display_name() const = 0;
};


struct vidio_input_device : public vidio_input {
public:
  static std::vector<vidio_input_device*> list_input_devices(const struct vidio_input_device_filter*);
};

#endif //LIBVIDIO_VIDIO_INPUT_H
