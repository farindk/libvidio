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

#include "vidio_format_converter.h"
#include "colorconversion/yuv2rgb.h"
#include "colorconversion/mjpeg.h"
#include "colorconversion/ffmpeg.h"


struct vidio_format_converter_function : public vidio_format_converter
{
public:
  explicit vidio_format_converter_function(vidio_frame* (* func)(const vidio_frame*)) { m_func = func; }

  void push(const vidio_frame* f) override
  {
    auto* out = m_func(f);
    push_decoded_frame(out);
  }

private:
  vidio_frame* (* m_func)(const vidio_frame*);
};


vidio_format_converter* vidio_format_converter::create(vidio_pixel_format in, vidio_pixel_format out)
{
  if (in == vidio_pixel_format_MJPEG ||
      in == vidio_pixel_format_H264) {
    //return new vidio_format_converter_function(mjpeg_to_rgb8_ffmpeg);
    auto* converter = new vidio_format_converter_ffmpeg();
    converter->init(AV_CODEC_ID_H264, out);
    return converter;
  }
  else {
    //return new vidio_format_converter_function(yuyv_to_rgb8);
    return new vidio_format_converter_swscale(out);
  }
  /*
  else {
    return nullptr;
  }
  */
}
