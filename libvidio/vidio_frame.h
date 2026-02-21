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
#include <vector>


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

  // --- metadata ---

  void copy_metadata_from(const vidio_frame* source);

  void set_timestamp_us(uint64_t ts) { m_timestamp_us = ts; }

  uint64_t get_timestamp_us() const { return m_timestamp_us; }

  // --- keyframe ---

  void set_keyframe(bool is_keyframe) { m_is_keyframe = is_keyframe; }

  bool is_keyframe() const { return m_is_keyframe; }

  // --- DTS (decode timestamp) ---

  void set_dts_us(int64_t dts_us) { m_dts_us = dts_us; m_has_dts = true; }

  bool has_dts() const { return m_has_dts; }

  int64_t get_dts_us() const { return m_dts_us; }

  // --- codec extradata (SPS/PPS/VPS for H264/H265) ---

  void set_codec_extradata(const uint8_t* data, int size);

  bool has_codec_extradata() const { return !m_codec_extradata.empty(); }

  const uint8_t* get_codec_extradata() const { return m_codec_extradata.data(); }

  int get_codec_extradata_size() const { return static_cast<int>(m_codec_extradata.size()); }

  // --- clone ---

  vidio_frame* clone() const;

private:
  int m_width = 0, m_height = 0;
  vidio_pixel_format m_format = vidio_pixel_format_undefined;

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

  uint64_t m_timestamp_us = 0;
  bool m_is_keyframe = true;  // default true: uncompressed frames are always independently decodable
  bool m_has_dts = false;
  int64_t m_dts_us = 0;
  std::vector<uint8_t> m_codec_extradata;

  void get_chroma_size(int& cw, int& ch) const;

  static const int cDefaultStride = 16;
};


#endif //LIBVIDIO_VIDIO_FRAME_H
