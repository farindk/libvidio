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

#ifndef LIBVIDIO_VIDIO_INPUT_DEVICE_RTSP_H
#define LIBVIDIO_VIDIO_INPUT_DEVICE_RTSP_H

#include <string>
#include <vector>
#include <memory>
#include <libvidio/vidio.h>
#include <libvidio/vidio_input.h>
#include "vidio_rtsp_stream.h"
#include "vidio_video_format_rtsp.h"
#include <deque>
#include <thread>
#include <mutex>

#if WITH_JSON
#include "nlohmann/json.hpp"
#endif


struct vidio_input_device_rtsp : public vidio_input
{
public:
  static vidio_input_device_rtsp* create(const std::string& url);
  static vidio_input_device_rtsp* create(const std::string& url,
                                         const std::string& username,
                                         const std::string& password);

  ~vidio_input_device_rtsp() override;

  std::string get_display_name() const override;

  vidio_input_source get_source() const override { return vidio_input_source_RTSP; }

  std::vector<vidio_video_format*> get_video_formats() const override;

  const vidio_error* set_capture_format(const vidio_video_format* requested_format,
                                        const vidio_video_format** out_actual_format) override;

  const vidio_error* start_capturing() override;

  const vidio_error* stop_capturing() override;

  const vidio_frame* peek_next_frame() const override;

  void pop_next_frame() override;

  std::string serialize(vidio_serialization_format serialformat) const override;

  // Configuration methods
  void set_transport(vidio_rtsp_transport transport);
  void set_timeout_seconds(int timeout_seconds);

#if WITH_JSON
  static vidio_input_device_rtsp* find_matching_device(const std::vector<vidio_input*>& inputs,
                                                       const nlohmann::json& json);
#endif

  void push_frame_into_queue(const vidio_frame* f);

private:
  explicit vidio_input_device_rtsp(const std::string& url);
  vidio_input_device_rtsp(const std::string& url,
                          const std::string& username,
                          const std::string& password);

  std::string m_url;
  std::string m_username;
  std::string m_password;

  std::unique_ptr<vidio_rtsp_stream> m_stream;

  std::thread m_capturing_thread;

  std::deque<const vidio_frame*> m_frame_queue;
  mutable std::mutex m_queue_mutex;

  static const int cMaxFrameQueueLength = 20;

  bool m_connected = false;
  std::unique_ptr<vidio_video_format_rtsp> m_current_format;

  friend class vidio_rtsp_stream;
};

#endif //LIBVIDIO_VIDIO_INPUT_DEVICE_RTSP_H
