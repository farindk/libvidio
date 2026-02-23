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

#include "vidio_video_format_file.h"
#include <sstream>


vidio_video_format_file::vidio_video_format_file(uint32_t width, uint32_t height,
                                                 vidio_pixel_format pixel_format,
                                                 std::optional<vidio_fraction> framerate)
    : m_width(width), m_height(height), m_pixel_format(pixel_format), m_framerate(framerate)
{
  m_format_class = pixel_format_to_class(pixel_format);
}


vidio_video_format_file::vidio_video_format_file(const vidio_video_format_file& fmt)
    : m_width(fmt.m_width), m_height(fmt.m_height),
      m_pixel_format(fmt.m_pixel_format), m_framerate(fmt.m_framerate),
      m_format_class(fmt.m_format_class)
{
}


#if WITH_JSON
vidio_video_format_file::vidio_video_format_file(const nlohmann::json& json)
{
  m_width = json["width"];
  m_height = json["height"];

  std::string format_str = json["pixel_format"];
  if (format_str == "H264") {
    m_pixel_format = vidio_pixel_format_H264;
  }
  else if (format_str == "H265") {
    m_pixel_format = vidio_pixel_format_H265;
  }
  else if (format_str == "MJPEG") {
    m_pixel_format = vidio_pixel_format_MJPEG;
  }
  else if (format_str == "YUV420") {
    m_pixel_format = vidio_pixel_format_YUV420_planar;
  }
  else if (format_str == "RGB8") {
    m_pixel_format = vidio_pixel_format_RGB8;
  }
  else {
    m_pixel_format = vidio_pixel_format_undefined;
  }

  if (json.contains("framerate_num") && json.contains("framerate_den")) {
    vidio_fraction fr{};
    fr.numerator = json["framerate_num"];
    fr.denominator = json["framerate_den"];
    m_framerate = fr;
  }

  m_format_class = pixel_format_to_class(m_pixel_format);
}
#endif


vidio_video_format* vidio_video_format_file::clone() const
{
  return new vidio_video_format_file(*this);
}


std::string vidio_video_format_file::get_user_description() const
{
  std::stringstream ss;

  switch (m_pixel_format) {
    case vidio_pixel_format_H264:
      ss << "H.264";
      break;
    case vidio_pixel_format_H265:
      ss << "H.265";
      break;
    case vidio_pixel_format_MJPEG:
      ss << "MJPEG";
      break;
    case vidio_pixel_format_YUV420_planar:
      ss << "YUV420";
      break;
    case vidio_pixel_format_RGB8:
      ss << "RGB";
      break;
    default:
      ss << "Unknown";
      break;
  }

  ss << " " << m_width << "x" << m_height;

  if (m_framerate) {
    double fps = static_cast<double>(m_framerate->numerator) / m_framerate->denominator;
    ss << " @ " << fps << " fps";
  }

  return ss.str();
}


int vidio_video_format_file::format_match_score(const vidio_video_format* f) const
{
  return generic_format_match_score(f);
}


std::string vidio_video_format_file::serialize(vidio_serialization_format serialformat) const
{
#if WITH_JSON
  if (serialformat == vidio_serialization_format_json) {
    nlohmann::json json{
        {"class", "file"},
        {"width", m_width},
        {"height", m_height}
    };

    switch (m_pixel_format) {
      case vidio_pixel_format_H264:
        json["pixel_format"] = "H264";
        break;
      case vidio_pixel_format_H265:
        json["pixel_format"] = "H265";
        break;
      case vidio_pixel_format_MJPEG:
        json["pixel_format"] = "MJPEG";
        break;
      case vidio_pixel_format_YUV420_planar:
        json["pixel_format"] = "YUV420";
        break;
      case vidio_pixel_format_RGB8:
        json["pixel_format"] = "RGB8";
        break;
      default:
        json["pixel_format"] = "unknown";
        break;
    }

    if (m_framerate) {
      json["framerate_num"] = m_framerate->numerator;
      json["framerate_den"] = m_framerate->denominator;
    }

    return json.dump();
  }
#endif

  return {};
}


vidio_pixel_format_class vidio_video_format_file::pixel_format_to_class(vidio_pixel_format format)
{
  switch (format) {
    case vidio_pixel_format_H264:
      return vidio_pixel_format_class_H264;
    case vidio_pixel_format_H265:
      return vidio_pixel_format_class_H265;
    case vidio_pixel_format_MJPEG:
      return vidio_pixel_format_class_MJPEG;
    case vidio_pixel_format_RGB8:
    case vidio_pixel_format_RGB8_planar:
      return vidio_pixel_format_class_RGB;
    case vidio_pixel_format_YUV420_planar:
    case vidio_pixel_format_YUV422_YUYV:
      return vidio_pixel_format_class_YUV;
    default:
      return vidio_pixel_format_class_unknown;
  }
}
