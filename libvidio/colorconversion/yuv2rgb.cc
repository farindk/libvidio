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

#include "yuv2rgb.h"
#include "common.h"
#include "libvidio/vidio_frame.h"


vidio_frame* yuyv_to_rgb8(const vidio_frame* input)
{
  int w = input->get_width();
  int h = input->get_height();

  vidio_frame* out_frame = new vidio_frame();
  out_frame->set_format(vidio_pixel_format_RGB8, w, h);
  out_frame->add_raw_plane(vidio_color_channel_interleaved, w, h, 24);

  const uint8_t* in;
  int in_stride;
  in = input->get_plane(vidio_color_channel_interleaved, &in_stride);

  uint8_t* out;
  int out_stride;
  out = out_frame->get_plane(vidio_color_channel_interleaved, &out_stride);

  for (int y = 0; y < h; y++) {
    for (int x = 0; x < w - 1; x += 2) {
      int y1 = in[y * in_stride + x * 2 + 0] - 16;
      int y2 = in[y * in_stride + x * 2 + 2] - 16;
      int u = in[y * in_stride + x * 2 + 1] - 128;
      int v = in[y * in_stride + x * 2 + 3] - 128;

      out[y * out_stride + 3 * x + 0] = clip8(1.164 * y1 + 1.1596 * v);
      out[y * out_stride + 3 * x + 1] = clip8(1.164 * y1 - 0.392 * u - 0.813 * v);
      out[y * out_stride + 3 * x + 2] = clip8(1.164 * y1 + 2.017 * u);

      out[y * out_stride + 3 * x + 3] = clip8(1.164 * y2 + 1.1596 * v);
      out[y * out_stride + 3 * x + 4] = clip8(1.164 * y2 - 0.392 * u - 0.813 * v);
      out[y * out_stride + 3 * x + 5] = clip8(1.164 * y2 + 2.017 * u);
    }

    if (w & 1) {
      int x = w - 1;

      int y1 = in[y * in_stride + x * 2 + 0] - 16;
      int u = in[y * in_stride + x * 2 + 1] - 128;
      int v = in[y * in_stride + x * 2 + 3] - 128;

      out[y * out_stride + 3 * x + 0] = clip8(1.164 * y1 + 1.1596 * v);
      out[y * out_stride + 3 * x + 1] = clip8(1.164 * y1 - 0.392 * u - 0.813 * v);
      out[y * out_stride + 3 * x + 2] = clip8(1.164 * y1 + 2.017 * u);
    }
  }

  out_frame->copy_metadata_from(input);
  return out_frame;
}
