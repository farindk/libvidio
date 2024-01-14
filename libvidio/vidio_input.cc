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

#include <libvidio/vidio_input.h>
#include <vector>

#if WITH_VIDEO4LINUX2
#include "v4l/v4l.h"
#endif

#if WITH_JSON
#include "nlohmann/json.hpp"
#endif


vidio_input_device_v4l* vidio_input::find_matching_device(const std::vector<vidio_input*>& inputs, const std::string& serializedString,
                                                          vidio_serialization_format serialformat)
{
#if WITH_JSON
  if (serialformat == vidio_serialization_format_json) {
    nlohmann::json json = nlohmann::json::parse(serializedString);

    if (!json.contains("class")) {
      return nullptr;
    }

    std::string cl = json["class"];
    if (cl == "v4l2") {
      return vidio_input_device_v4l::find_matching_device(inputs, json);
    }
  }
#endif

  return nullptr;
}


std::vector<vidio_input_device*> vidio_input_device::list_input_devices(const struct vidio_input_device_filter* filter)
{
  std::vector<vidio_input_device*> devices;

#if WITH_VIDEO4LINUX2
  std::vector<vidio_input_device_v4l*> devs;
  devs = v4l_list_input_devices(filter);

  devices.insert(devices.end(), devs.begin(), devs.end());
#endif

  return devices;
}

