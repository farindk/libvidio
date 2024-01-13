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

#if WITH_JSON
#include "nlohmann/json.hpp"
#include "v4l/v4l.h"

const vidio_video_format* vidio_video_format::deserialize(const std::string& jsonStr)
{
  nlohmann::json json = nlohmann::json::parse(jsonStr);

  if (!json.contains("class")) {
    return nullptr;
  }

  std::string cl = json["class"];
  if (cl == "v4l2") {
    return new vidio_video_format_v4l(json);
  }

  return nullptr;
}
#endif
