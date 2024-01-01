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

#ifndef LIBVIDIO_VIDIO_VIDEO_FORMAT_H
#define LIBVIDIO_VIDIO_VIDEO_FORMAT_H

#include <libvidio/vidio.h>
#include <cinttypes>
#include <string>

struct vidio_video_format
{
public:
  virtual ~vidio_video_format() = default;

  virtual uint32_t get_width() const = 0;

  virtual uint32_t get_height() const = 0;

  virtual vidio_fraction get_framerate() const = 0;

  virtual std::string get_user_description() const = 0;

  virtual vidio_pixel_format_class get_pixel_format_class() const = 0;
};


#endif //LIBVIDIO_VIDIO_VIDEO_FORMAT_H
