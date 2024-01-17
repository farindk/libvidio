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

#ifndef LIBVIDIO_VIDIO_VIDEO_FORMAT_V4L_H
#define LIBVIDIO_VIDIO_VIDEO_FORMAT_V4L_H

#include <libvidio/vidio_video_format.h>
#include <linux/videodev2.h>

#if WITH_JSON
#include "nlohmann/json.hpp"
#endif


struct vidio_video_format_v4l : public vidio_video_format
{
public:
  vidio_video_format_v4l(v4l2_fmtdesc fmt,
                         uint32_t width, uint32_t height,
                         vidio_fraction framerate);

#if WITH_JSON

  vidio_video_format_v4l(const nlohmann::json& json);

#endif

  uint32_t get_width() const override { return m_width; }

  uint32_t get_height() const override { return m_height; }

  vidio_fraction get_framerate() const override { return m_framerate; }

  std::string get_user_description() const override { return {(const char*) m_format.description}; }

  vidio_pixel_format_class get_pixel_format_class() const override { return m_format_class; }

  __u32 get_v4l2_pixel_format() const { return m_format.pixelformat; }

  vidio_pixel_format get_pixel_format() const override;

  int format_match_score(const vidio_video_format* f) const override;

  std::string serialize(vidio_serialization_format) const override;

private:
  v4l2_fmtdesc m_format{};
  uint32_t m_width, m_height;
  vidio_fraction m_framerate;

  vidio_pixel_format_class m_format_class;
};

#endif //LIBVIDIO_VIDIO_VIDEO_FORMAT_V4L_H
