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

#ifndef LIBVIDIO_V4L_H
#define LIBVIDIO_V4L_H

#include <linux/videodev2.h>
#include <string>
#include <vector>
#include <memory>
#include <libvidio/vidio.h>
#include <libvidio/vidio_input.h>


struct vidio_v4l_raw_device {
public:
  bool query_device(const char* filename);

  std::string get_bus_info() const { return {(char*)&m_caps.bus_info[0]}; }

  std::string get_display_name() const { return {(char*)&m_caps.card[0]}; }

private:
  std::string m_device_file;

  struct v4l2_capability m_caps;
};


struct vidio_input_device_v4l : public vidio_input_device {
public:
  explicit vidio_input_device_v4l(vidio_v4l_raw_device* d) { m_v4l_capture_devices.emplace_back(d); }

  std::string get_display_name() const override { return m_v4l_capture_devices[0]->get_display_name(); }

  vidio_input_source get_source() const override { return vidio_input_source_Video4Linux2; }

  bool matches_v4l_raw_device(const vidio_v4l_raw_device*) const;

  void add_v4l_raw_device(vidio_v4l_raw_device* dev) {
    m_v4l_capture_devices.emplace_back(dev);
  }

private:
  std::vector<vidio_v4l_raw_device*> m_v4l_capture_devices;
};


std::vector<vidio_input_device_v4l*> v4l_list_input_devices(const struct vidio_input_device_filter* filter);

#endif //LIBVIDIO_V4L_H
