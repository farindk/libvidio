/*
 * VidIO library
 * Copyright (c) 2026 Dirk Farin <dirk.farin@gmail.com>
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

#ifndef LIBVIDIO_VIDIO_VIDEO_FORMAT_RTSP_H
#define LIBVIDIO_VIDIO_VIDEO_FORMAT_RTSP_H

#include <libvidio/vidio_video_format.h>
#include <optional>

#if WITH_JSON
#include "nlohmann/json.hpp"
#endif


struct vidio_video_format_rtsp : public vidio_video_format
{
public:
  vidio_video_format_rtsp(uint32_t width, uint32_t height,
                          vidio_pixel_format pixel_format,
                          std::optional<vidio_fraction> framerate);

  vidio_video_format_rtsp(const vidio_video_format_rtsp& fmt);

#if WITH_JSON
  explicit vidio_video_format_rtsp(const nlohmann::json& json);
#endif

  vidio_video_format* clone() const override;

  uint32_t get_width() const override { return m_width; }

  uint32_t get_height() const override { return m_height; }

  bool has_fixed_framerate() const override { return !!m_framerate; }

  vidio_fraction get_framerate() const override {
    if (m_framerate) {
      return *m_framerate;
    }
    else {
      return {0, 1};
    }
  }

  std::string get_user_description() const override;

  vidio_pixel_format_class get_pixel_format_class() const override { return m_format_class; }

  vidio_pixel_format get_pixel_format() const override { return m_pixel_format; }

  int format_match_score(const vidio_video_format* f) const override;

  std::string serialize(vidio_serialization_format) const override;

private:
  uint32_t m_width, m_height;
  vidio_pixel_format m_pixel_format;
  std::optional<vidio_fraction> m_framerate;
  vidio_pixel_format_class m_format_class;

  static vidio_pixel_format_class pixel_format_to_class(vidio_pixel_format format);
};

#endif //LIBVIDIO_VIDIO_VIDEO_FORMAT_RTSP_H
