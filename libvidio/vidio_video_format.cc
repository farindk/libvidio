/*
 * VidIO library
 * Copyright (c) 2023 Dirk Farin <dirk.farin@gmail.com>
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

#include "vidio_video_format.h"

#include "nlohmann/json.hpp"
#include "v4l/v4l.h"

const vidio_video_format* vidio_video_format::deserialize(const std::string& jsonStr, vidio_serialization_format serialformat)
{
#if WITH_JSON
  if (serialformat == vidio_serialization_format_json) {
    nlohmann::json json = nlohmann::json::parse(jsonStr);

    if (!json.contains("class")) {
      return nullptr;
    }

    std::string cl = json["class"];
    if (cl == "v4l2") {
      return new vidio_video_format_v4l(json);
    }
  }
#endif

  return nullptr;
}


const vidio_video_format* vidio_video_format::find_best_match(const std::vector<const vidio_video_format*>& formats,
                                                              const vidio_video_format* requested_format,
                                                              vidio_device_match* out_score)
{
  int score = 0;
  const vidio_video_format* best_match = nullptr;

  for (auto* format : formats) {
    int s = requested_format->format_match_score(format);
    if (s > score) {
      score = s;
      best_match = format;
    }
  }

  if (out_score) {
    if (score == 0) {
      *out_score = vidio_device_match_none;
    }
    else if (score == 100) {
      *out_score = vidio_device_match_exact;
    }
    else {
      *out_score = vidio_device_match_approx;
    }
  }

  return best_match;
}


int vidio_fraction_compare(const vidio_fraction& a, const vidio_fraction& b)
{
  int aa = a.numerator * b.denominator;
  int bb = b.numerator * a.denominator;
  assert(a.denominator > 0);
  assert(b.denominator > 0);
  if (aa > bb) { return 1; }
  if (aa < bb) { return -1; }
  return 0;
}


int vidio_video_format::generic_format_match_score(const vidio_video_format* f) const
{
  if (f->get_width() != get_width() ||
      f->get_height() != get_height()) {
    return 0;
  }

  if (f->get_pixel_format() != get_pixel_format() ||
      vidio_fraction_compare(f->get_framerate(), get_framerate()) != 0) {
    return 50;
  }

  return 100;
}

