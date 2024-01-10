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

#include "converter.h"
#include <cassert>

#include "yuv2rgb.h"
#include "mjpeg.h"


static vidio_frame* convert_to_rgb8(const vidio_frame* input)
{
  vidio_pixel_format inputFormat = input->get_pixel_format();

  switch (inputFormat) {
    case vidio_pixel_format_YUV422_YUYV:
      return yuyv_to_rgb8(input);
    case vidio_pixel_format_MJPEG:
      return mjpeg_to_rgb8_ffmpeg(input);
    default:
      assert(false);
      return nullptr;
  }
}


vidio_frame* convert_frame(const vidio_frame* input, vidio_pixel_format format)
{
  switch (format) {
    case vidio_pixel_format_RGB8:
      return convert_to_rgb8(input);
    default:
      assert(false);
      return nullptr;
  }
}
