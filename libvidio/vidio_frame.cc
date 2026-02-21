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

#include "vidio_frame.h"
#include <cassert>
#include <cstring>


vidio_frame::~vidio_frame()
{
  for (auto& plane : m_planes) {
    if (plane.second.memory_owned) {
      delete[] plane.second.mem;
    }
  }
}


void vidio_frame::set_format(vidio_pixel_format format, int w, int h)
{
  m_format = format;
  m_width = w;
  m_height = h;
}

void vidio_frame::add_raw_plane(vidio_color_channel channel, int bpp)
{
  switch (channel) {
    case vidio_color_channel_undefined:
    case vidio_color_channel_compressed:
      assert(false);
      break;

    case vidio_color_channel_R:
    case vidio_color_channel_G:
    case vidio_color_channel_B:
    case vidio_color_channel_Y:
    case vidio_color_channel_alpha:
    case vidio_color_channel_depth:
    case vidio_color_channel_interleaved:
      add_raw_plane(channel, m_width, m_height, bpp);
      break;

    case vidio_color_channel_U:
    case vidio_color_channel_V: {
      int cw, ch;
      get_chroma_size(cw, ch);
      add_raw_plane(channel, cw, ch, bpp);
      break;
    }
  }
}

void vidio_frame::get_chroma_size(int& cw, int& ch) const
{
  switch (m_format) {
    case vidio_pixel_format_undefined:
    case vidio_pixel_format_RGB8:
    case vidio_pixel_format_RGB8_planar:
    case vidio_pixel_format_MJPEG:
    case vidio_pixel_format_H264:
    case vidio_pixel_format_H265:
    case vidio_pixel_format_RGGB8:
      assert(false);
      cw = ch = 0;
      return;

    case vidio_pixel_format_YUV420_planar:
      cw = (m_width + 1) / 2;
      ch = (m_height + 1) / 2;
      break;

    case vidio_pixel_format_YUV422_YUYV:
      cw = (m_width + 1) / 2;
      ch = m_height;
      break;
  }
}


static int align_up(int w, int stride)
{
  if ((w % stride) == 0) {
    return w;
  }
  else {
    return w + stride - (w % stride);
  }
}


void vidio_frame::add_raw_plane(vidio_color_channel channel, int w, int h, int bpp)
{
  assert(m_planes.find(channel) == m_planes.end());

  int bytes_per_pixel = (bpp + 7) / 8;
  int memWidth = align_up(w * bytes_per_pixel, cDefaultStride);

  Plane p;
  p.w = w;
  p.h = h;
  p.stride = memWidth;
  p.format = vidio_channel_format_pixels;
  p.bpp = bpp;

  p.mem = new uint8_t[memWidth * h];
  p.memory_owned = true;

  m_planes[channel] = p;
}

void vidio_frame::add_external_raw_plane(vidio_color_channel channel,
                                         uint8_t* mem, int w, int h, int bpp, int stride)
{
  assert(m_planes.find(channel) == m_planes.end());

  Plane p;
  p.w = w;
  p.h = h;
  p.stride = stride;
  p.format = vidio_channel_format_pixels;
  p.mem = mem;
  p.memory_owned = false;

  m_planes[channel] = p;
}

void vidio_frame::add_compressed_plane(vidio_color_channel channel,
                                       vidio_channel_format format, int bpp,
                                       const uint8_t* mem, int memorySize, int w, int h)
{
  assert(m_planes.find(channel) == m_planes.end());

  Plane p;
  p.w = w;
  p.h = h;
  p.stride = memorySize;
  p.format = format;
  p.bpp = bpp;
  p.mem = new uint8_t[memorySize];
  p.memory_owned = true;
  memcpy(p.mem, mem, memorySize);

  m_planes[channel] = p;
}

bool vidio_frame::has_plane(vidio_color_channel channel) const
{
  return m_planes.find(channel) != m_planes.end();
}

uint8_t* vidio_frame::get_plane(vidio_color_channel channel, int* stride)
{
  assert(has_plane(channel));
  assert(stride);

  *stride = m_planes[channel].stride;
  return m_planes[channel].mem;
}

const uint8_t* vidio_frame::get_plane(vidio_color_channel channel, int* stride) const
{
  auto iter = m_planes.find(channel);

  assert(iter != m_planes.end());
  assert(stride);

  *stride = iter->second.stride;
  return iter->second.mem;
}


void vidio_frame::copy_raw_plane(vidio_color_channel channel, const void* mem, size_t length)
{
  auto iter = m_planes.find(channel);
  assert(iter != m_planes.end());

  auto& plane = iter->second;

  int bytes_per_pixel = (plane.bpp + 7) / 8;

  for (int y = 0; y < plane.h; y++) {
    memcpy(plane.mem + y * plane.stride,
           ((uint8_t*) mem) + y * plane.w * bytes_per_pixel,
           plane.w * bytes_per_pixel);
  }
}


void vidio_frame::copy_metadata_from(const vidio_frame* source)
{
  m_timestamp_us = source->get_timestamp_us();
  m_is_keyframe = source->is_keyframe();
  m_has_dts = source->has_dts();
  m_dts_us = source->get_dts_us();
  if (source->has_codec_extradata()) {
    set_codec_extradata(source->get_codec_extradata(), source->get_codec_extradata_size());
  }
}


void vidio_frame::set_codec_extradata(const uint8_t* data, int size)
{
  if (data && size > 0) {
    m_codec_extradata.assign(data, data + size);
  }
  else {
    m_codec_extradata.clear();
  }
}


vidio_frame* vidio_frame::clone() const
{
  auto* f = new vidio_frame();
  f->set_format(m_format, m_width, m_height);

  for (const auto& [channel, plane] : m_planes) {
    if (plane.format == vidio_channel_format_pixels) {
      f->add_raw_plane(channel, plane.w, plane.h, plane.bpp);
      int dst_stride;
      uint8_t* dst = f->get_plane(channel, &dst_stride);
      int bytes_per_pixel = (plane.bpp + 7) / 8;
      int row_bytes = plane.w * bytes_per_pixel;
      for (int y = 0; y < plane.h; y++) {
        memcpy(dst + y * dst_stride, plane.mem + y * plane.stride, row_bytes);
      }
    }
    else {
      // Compressed plane: stride holds memory size
      f->add_compressed_plane(channel, plane.format, plane.bpp,
                              plane.mem, plane.stride,
                              plane.w, plane.h);
    }
  }

  f->copy_metadata_from(this);
  return f;
}
