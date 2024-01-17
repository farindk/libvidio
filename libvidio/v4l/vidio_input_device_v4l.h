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

#ifndef LIBVIDIO_VIDIO_INPUT_DEVICE_V4L_H
#define LIBVIDIO_VIDIO_INPUT_DEVICE_V4L_H

#include <string>
#include <vector>
#include <memory>
#include <libvidio/vidio.h>
#include <libvidio/vidio_input.h>
#include <linux/videodev2.h>
#include "vidio_video_format_v4l.h"
#include <deque>
#include <thread>
#include <mutex>

#include "libvidio/vidio_error.h"


struct vidio_v4l_raw_device;

struct vidio_input_device_v4l : public vidio_input_device
{
public:
  explicit vidio_input_device_v4l(vidio_v4l_raw_device* d) { m_v4l_capture_devices.emplace_back(d); }

  std::string get_display_name() const override;

  vidio_input_source get_source() const override { return vidio_input_source_Video4Linux2; }

  bool matches_v4l_raw_device(const vidio_v4l_raw_device*) const;

  void add_v4l_raw_device(vidio_v4l_raw_device* dev)
  {
    m_v4l_capture_devices.emplace_back(dev);
  }

  std::vector<vidio_video_format*> get_video_formats() const override;

  const vidio_error* set_capture_format(const vidio_video_format* requested_format,
                                        const vidio_video_format** out_actual_format) override;

  const vidio_error* start_capturing() override;

  const vidio_error* stop_capturing() override;

  const vidio_frame* peek_next_frame() const override;

  void pop_next_frame() override;

  std::string serialize(vidio_serialization_format serialformat) const override;

#if WITH_JSON

  static vidio_input_device_v4l* find_matching_device(const std::vector<vidio_input*>& inputs, const nlohmann::json& json);

  int spec_match_score(const std::string& businfo, const std::string& card, const std::string& device_file) const;

#endif

private:
  std::vector<vidio_v4l_raw_device*> m_v4l_capture_devices;

  vidio_v4l_raw_device* m_active_device = nullptr;

  std::thread m_capturing_thread;

  std::deque<const vidio_frame*> m_frame_queue;
  mutable std::mutex m_queue_mutex;

  static const int cMaxFrameQueueLength = 20;

  void push_frame_into_queue(const vidio_frame*);

  friend struct vidio_v4l_raw_device; // to be able to access push_frame_into_queue()
};


vidio_result<std::vector<vidio_input_device_v4l*>> v4l_list_input_devices(const struct vidio_input_device_filter* filter);

#endif //LIBVIDIO_VIDIO_INPUT_DEVICE_V4L_H
