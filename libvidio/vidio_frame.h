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

#ifndef LIBVIDIO_VIDIO_FRAME_H
#define LIBVIDIO_VIDIO_FRAME_H

#include "vidio.h"
#include <map>


struct vidio_frame
{
public:
  ~vidio_frame();

  void set_format(vidio_pixel_format format, int w, int h);

  // size is auto-computed
  void add_raw_plane(vidio_color_channel channel, int bpp);

  // custom size (mainly for auxiliary planes like 'depth')
  void add_raw_plane(vidio_color_channel channel, int w, int h, int bpp);

  void copy_raw_plane(vidio_color_channel channel, const void* mem, size_t length);

  // vidio_frame will reuse the existing memory. It has to remain allocated while used.
  void add_external_raw_plane(vidio_color_channel channel,
                              uint8_t* mem, int w, int h, int bpp, int stride);

  void add_compressed_plane(vidio_color_channel channel,
                            vidio_channel_format format, int bpp,
                            const uint8_t* mem, int memorySize, int w, int h);

  int get_width() const { return m_width; }

  int get_height() const { return m_height; }

  enum vidio_pixel_format get_pixel_format() const { return m_format; }

  bool has_plane(vidio_color_channel) const;

  uint8_t* get_plane(vidio_color_channel, int* stride);

  const uint8_t* get_plane(vidio_color_channel, int* stride) const;

private:
  int m_width, m_height;
  vidio_pixel_format m_format;

  struct Plane
  {
    int w=0, h=0;
    int stride=0;
    vidio_channel_format format = vidio_channel_format_undefined;
    int bpp=0;

    uint8_t* mem = nullptr;
    bool memory_owned = false;
  };

  std::map<vidio_color_channel, Plane> m_planes;

  void get_chroma_size(int& cw, int& ch) const;

  static const int cDefaultStride = 16;
};


#endif //LIBVIDIO_VIDIO_FRAME_H
