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

#ifndef LIBVIDIO_COMMON_H
#define LIBVIDIO_COMMON_H

#include <cstdint>

inline uint8_t clip8(int v)
{
  if (v <= 0) return 0;
  if (v >= 255) return 255;
  return static_cast<uint8_t>(v);
}

inline uint8_t clip8(double v)
{
  return clip8((int) (v + 0.5));
}

#endif //LIBVIDIO_COMMON_H
