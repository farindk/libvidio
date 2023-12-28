/*
 * VidIO library
 * Copyright (c) 2023 Dirk Farin <dirk.farin@gmail.com>
 *
 * This file is part of libvidio.
 *
 * libvidio is free software: you can redistribute it and/or modify
 * it under the terms of the GNU Lesser General Public License as
 * published by the Free Software Foundation, either version 3 of
 * the License, or (at your option) any later version.
 *
 * libvidio is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with libvidio.  If not, see <http://www.gnu.org/licenses/>.
 */

#include "libvidio/vidio.h"
#include "api_structs.h"
#include <cassert>
#include <cstring>

#if WITH_VIDEO4LINUX2
#include "v4l/v4l.h"
#endif

static uint8_t vidio_version_major = VIDIO_VERSION_MAJOR;
static uint8_t vidio_version_minor = VIDIO_VERSION_MINOR;
static uint8_t vidio_version_patch = VIDIO_VERSION_PATCH;

#define xstr(s) str(s)
#define str(s) #s

static const char* vidio_version_string = xstr(VIDIO_VERSION_MAJOR) "." xstr(VIDIO_VERSION_MINOR) "." xstr(VIDIO_VERSION_PATCH);

const char* vidio_get_version(void)
{
  return vidio_version_string;
}

static constexpr int encode_bcd(int v)
{
  assert(v <= 99);
  assert(v >= 0);

  return (v / 10) * 16 + (v % 10);
}

uint32_t vidio_get_version_number(void)
{
  return ((encode_bcd(vidio_version_major) << 16) |
          (encode_bcd(vidio_version_minor) << 8) |
          (encode_bcd(vidio_version_patch)));
}

int vidio_get_version_number_major(void)
{
  return vidio_version_major;
}

int vidio_get_version_number_minor(void)
{
  return vidio_version_minor;
}

int vidio_get_version_number_patch(void)
{
  return vidio_version_patch;
}


void vidio_free_string(const char* s)
{
  delete[] s;
}



size_t vidio_list_input_devices(const struct vidio_input_device_info*** out_devices, const struct vidio_input_device_filter* filter)
{
  std::vector<std::shared_ptr<VidioInputDeviceInfo>> devices;

#if WITH_VIDEO4LINUX2
  std::vector<std::shared_ptr<VidioInputDeviceInfo_V4L>> devs;
  devs = v4l_list_input_devices(filter);

  for (auto d : devs) {
    devices.emplace_back(d);
  }
#endif

  auto devlist = new vidio_input_device_info*[devices.size()];
  for (size_t i=0;i<devices.size();i++) {
    devlist[i] = new vidio_input_device_info();
    devlist[i]->device_info = devices[i];
  }

  *out_devices = (const struct vidio_input_device_info**)devlist;

  return devices.size();
}

const char* make_vidio_string(const std::string& s)
{
  char* out = new char[s.length() + 1];
  strcpy(out, s.c_str());
  return out;
}

const char* vidio_input_device_info_get_name(const struct vidio_input_device_info* device)
{
  return make_vidio_string(device->device_info->get_device_name());
}

enum vidio_input_device_backend vidio_input_device_info_get_input_device_backend(const struct vidio_input_device_info* device)
{
  return device->device_info->get_backend();
}
