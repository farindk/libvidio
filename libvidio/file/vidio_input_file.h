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

#ifndef LIBVIDIO_VIDIO_INPUT_FILE_H
#define LIBVIDIO_VIDIO_INPUT_FILE_H

#include <string>
#include <vector>
#include <memory>
#include <libvidio/vidio.h>
#include <libvidio/vidio_input.h>
#include "vidio_file_reader.h"
#include "vidio_video_format_file.h"
#include <atomic>
#include <deque>
#include <thread>
#include <mutex>

#if WITH_JSON
#include "nlohmann/json.hpp"
#endif


struct vidio_input_file : public vidio_input
{
public:
  static vidio_input_file* create(const std::string& filepath);

  ~vidio_input_file() override;

  std::string get_display_name() const override;

  vidio_input_source get_source() const override { return vidio_input_source_File; }

  std::vector<vidio_video_format*> get_video_formats() const override;

  const vidio_error* set_capture_format(const vidio_video_format* requested_format,
                                        const vidio_video_format** out_actual_format) override;

  const vidio_error* start_capturing() override;

  const vidio_error* stop_capturing() override;

  const vidio_frame* peek_next_frame() const override;

  void pop_next_frame() override;

  std::string serialize(vidio_serialization_format serialformat) const override;

  void set_loop(bool loop) { m_loop = loop; }

  void set_stop_mode(vidio_file_stop_mode mode) { m_stop_mode = mode; }

#if WITH_JSON
  static vidio_input_file* find_matching_device(const std::vector<vidio_input*>& inputs,
                                                const nlohmann::json& json);
#endif

  void push_frame_into_queue(const vidio_frame* f);

private:
  explicit vidio_input_file(const std::string& filepath);

  std::string m_filepath;

  std::unique_ptr<vidio_file_reader> m_reader;

  std::thread m_capturing_thread;

  std::deque<const vidio_frame*> m_frame_queue;
  mutable std::mutex m_queue_mutex;

  static const int cMaxFrameQueueLength = 20;

  bool m_opened = false;
  bool m_loop = true;
  vidio_file_stop_mode m_stop_mode = vidio_file_stop_mode_pause;
  std::atomic<bool> m_stop_requested{false};
  std::unique_ptr<vidio_video_format_file> m_current_format;

  void capturing_thread_func();
};

#endif //LIBVIDIO_VIDIO_INPUT_FILE_H
