/*
 * VidIO library
 * Copyright (c) 2023 Dirk Farin <dirk.farin@gmail.com>
 *
 * This file is part of libvidio.
 *
 * libvidio is free software: you can redistribute it and/or modify
 * it under the terms of the GNU Lesser General Public License as
 * published by the Free Software Foundation, either version 3 of
 * the License, or (at your option) any later version.
 *
 * libvidio is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with libvidio.  If not, see <http://www.gnu.org/licenses/>.
 */

#ifndef LIBVIDIO_API_STRUCTS_H
#define LIBVIDIO_API_STRUCTS_H

#include <string>
#include <memory>


struct vidio_input_device_info
{
  std::shared_ptr<class VidioInputDeviceInfo> device_info;
};

#endif //LIBVIDIO_API_STRUCTS_H
