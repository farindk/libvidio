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

#include "vidio_video_format_v4l.h"
#include "libvidio/util/key_value_store.h"


static vidio_pixel_format_class v4l_pixelformat_to_pixel_format_class(__u32 pixelformat)
{
  switch (pixelformat) {
    case V4L2_PIX_FMT_MJPEG:
      return vidio_pixel_format_class_MJPEG;
    case V4L2_PIX_FMT_H264:
    case V4L2_PIX_FMT_H264_MVC:
    case V4L2_PIX_FMT_H264_NO_SC:
    case V4L2_PIX_FMT_H264_SLICE:
      return vidio_pixel_format_class_H264;
    case V4L2_PIX_FMT_HEVC:
      return vidio_pixel_format_class_H265;
    case V4L2_PIX_FMT_YUYV:
      return vidio_pixel_format_class_YUV;
    case V4L2_PIX_FMT_SRGGB8:
      return vidio_pixel_format_class_RGB;
    default:
      return vidio_pixel_format_class_unknown;
  }
}


static vidio_pixel_format v4l_pixelformat_to_pixel_format(__u32 pixelformat)
{
  switch (pixelformat) {
    case V4L2_PIX_FMT_MJPEG:
      return vidio_pixel_format_MJPEG;
    case V4L2_PIX_FMT_H264:
    case V4L2_PIX_FMT_H264_MVC:
    case V4L2_PIX_FMT_H264_NO_SC:
    case V4L2_PIX_FMT_H264_SLICE:
      return vidio_pixel_format_H264;
    case V4L2_PIX_FMT_HEVC:
      return vidio_pixel_format_H265;
    case V4L2_PIX_FMT_YUYV:
      return vidio_pixel_format_YUV422_YUYV;
    case V4L2_PIX_FMT_SRGGB8:
      return vidio_pixel_format_RGB8;
    default:
      return vidio_pixel_format_undefined;
  }
}


vidio_video_format_v4l::vidio_video_format_v4l(v4l2_fmtdesc fmt,
                                               uint32_t width, uint32_t height,
                                               std::optional<vidio_fraction> framerate)
{
  m_format = fmt;
  m_width = width;
  m_height = height;
  m_framerate = framerate;

  m_format_class = v4l_pixelformat_to_pixel_format_class(fmt.pixelformat);
}

#if WITH_JSON

vidio_video_format_v4l::vidio_video_format_v4l(const nlohmann::json& json)
{
  // v4l2_fmtdesc

  m_format.type = json["format_type"]; // V4L2_BUF_TYPE_VIDEO_CAPTURE or V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE (TODO: in the future)
  m_format.flags = json["format_flags"];
  m_format.pixelformat = json["format_pixelformat"];
  m_format.mbus_code = json["format_mbus_code"];

  std::string descr = json["format_description"];
  strncpy((char*) m_format.description, descr.c_str(), 32);
  m_format.description[31] = 0;

  // resolution / frame-rate

  m_width = json["width"];
  m_height = json["height"];
  if (json.contains("framerate_numerator")) {
    vidio_fraction framerate{};
    framerate.numerator = json["framerate_numerator"];
    framerate.denominator = json["framerate_denominator"];
    m_framerate = framerate;
  }
}

#endif

std::string vidio_video_format_v4l::serialize(vidio_serialization_format serialformat) const
{
  if (serialformat == vidio_serialization_format_json) {
#if WITH_JSON
    nlohmann::json json{
        {"class",              "v4l2"},
        {"format_type",        m_format.type},
        {"format_flags",       m_format.flags},
        {"format_pixelformat", m_format.pixelformat},
        {"format_mbus_code",   m_format.mbus_code},
        {"format_description", (const char*) (m_format.description)},
        {"width",              m_width},
        {"height",             m_height}
    };

    if (m_framerate) {
        json["framerate_numerator"] = m_framerate->numerator;
        json["framerate_denominator"] = m_framerate->denominator;
    };

    return json.dump();
#endif
  }

  if (serialformat == vidio_serialization_format_keyvalue) {
    key_value_store store;
    // TODO
  }

  return {};
}


vidio_video_format_v4l::vidio_video_format_v4l(const vidio_video_format_v4l& fmt)
{
  *this = fmt;
}


vidio_video_format* vidio_video_format_v4l::clone() const
{
  return new vidio_video_format_v4l(*this);
}


vidio_pixel_format vidio_video_format_v4l::get_pixel_format() const
{
  return v4l_pixelformat_to_pixel_format(m_format.pixelformat);;
}


int vidio_video_format_v4l::format_match_score(const vidio_video_format* f) const
{
  auto* format = dynamic_cast<const vidio_video_format*>(f);
  if (format == nullptr) {
    return generic_format_match_score(f);
  }

  // actually, there is no special handling for this format type (yet)

  return generic_format_match_score(f);
}
