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

#include "vidio_input_file.h"
#include <libvidio/vidio_frame.h>
#include <chrono>
#include <thread>


vidio_input_file::vidio_input_file(const std::string& filepath)
    : m_filepath(filepath), m_reader(std::make_unique<vidio_file_reader>())
{
}


vidio_input_file::~vidio_input_file()
{
  // Force full stop regardless of stop mode
  m_stop_requested = true;
  m_reader->stop();
  if (m_capturing_thread.joinable()) {
    m_capturing_thread.join();
  }
  m_reader->close();

  std::lock_guard<std::mutex> lock(m_queue_mutex);
  for (auto* frame : m_frame_queue) {
    delete frame;
  }
  m_frame_queue.clear();
}


vidio_input_file* vidio_input_file::create(const std::string& filepath)
{
  return new vidio_input_file(filepath);
}


std::string vidio_input_file::get_display_name() const
{
  // Return just the filename, not the full path
  auto pos = m_filepath.find_last_of("/\\");
  if (pos != std::string::npos) {
    return m_filepath.substr(pos + 1);
  }
  return m_filepath;
}


std::vector<vidio_video_format*> vidio_input_file::get_video_formats() const
{
  std::vector<vidio_video_format*> formats;

  if (m_opened && m_current_format) {
    formats.push_back(m_current_format->clone());
  }

  return formats;
}


const vidio_error* vidio_input_file::set_capture_format(const vidio_video_format* requested_format,
                                                         const vidio_video_format** out_actual_format)
{
  if (!m_opened) {
    const vidio_error* err = m_reader->open(m_filepath);
    if (err) {
      return err;
    }
    m_opened = true;

    vidio_fraction framerate = m_reader->get_framerate();
    std::optional<vidio_fraction> opt_framerate;
    if (framerate.numerator > 0) {
      opt_framerate = framerate;
    }

    m_current_format = std::make_unique<vidio_video_format_file>(
        static_cast<uint32_t>(m_reader->get_width()),
        static_cast<uint32_t>(m_reader->get_height()),
        m_reader->get_pixel_format(),
        opt_framerate);
  }

  if (out_actual_format && m_current_format) {
    *out_actual_format = m_current_format->clone();
  }

  return nullptr;
}


const vidio_error* vidio_input_file::start_capturing()
{
  if (!m_opened) {
    const vidio_error* err = m_reader->open(m_filepath);
    if (err) {
      return err;
    }
    m_opened = true;

    vidio_fraction framerate = m_reader->get_framerate();
    std::optional<vidio_fraction> opt_framerate;
    if (framerate.numerator > 0) {
      opt_framerate = framerate;
    }

    m_current_format = std::make_unique<vidio_video_format_file>(
        static_cast<uint32_t>(m_reader->get_width()),
        static_cast<uint32_t>(m_reader->get_height()),
        m_reader->get_pixel_format(),
        opt_framerate);
  }

  // In continue mode, thread may still be running — don't start a second one
  if (m_capturing_thread.joinable()) {
    return nullptr;
  }

  m_capturing_thread = std::thread(&vidio_input_file::capturing_thread_func, this);

  return nullptr;
}


void vidio_input_file::capturing_thread_func()
{
  using clock = std::chrono::steady_clock;

  auto wall_start = clock::now();
  uint64_t pts_start = 0;
  bool pts_start_set = false;

  // Compute frame duration from framerate as fallback
  vidio_fraction fr = m_reader->get_framerate();
  auto frame_duration = std::chrono::microseconds(
      fr.numerator > 0
          ? static_cast<int64_t>(1000000) * fr.denominator / fr.numerator
          : 40000);  // 25fps default

  for (;;) {
    if (m_stop_requested) {
      break;
    }

    vidio_frame* frame = m_reader->read_next_frame();

    if (!frame) {
      // EOF
      if (m_loop) {
        if (!m_reader->seek_to_beginning()) {
          break;
        }

        // Reset timing
        pts_start_set = false;
        continue;
      }
      else {
        send_callback_message(vidio_input_message_end_of_stream);
        break;
      }
    }

    // Real-time pacing based on PTS
    uint64_t frame_pts = frame->get_timestamp_us();

    if (!pts_start_set) {
      wall_start = clock::now();
      pts_start = frame_pts;
      pts_start_set = true;
    }
    else if (frame_pts > pts_start) {
      auto target_walltime = wall_start + std::chrono::microseconds(frame_pts - pts_start);
      auto now = clock::now();

      if (target_walltime > now) {
        // Sleep in small increments so we can check for stop
        while (clock::now() < target_walltime) {
          auto remaining = std::chrono::duration_cast<std::chrono::milliseconds>(target_walltime - clock::now());
          if (remaining.count() <= 0) {
            break;
          }

          auto sleep_time = std::min(remaining, std::chrono::milliseconds(50));
          std::this_thread::sleep_for(sleep_time);

          if (m_reader->is_open() == false) {
            // reader was closed (stop was called)
            delete frame;
            return;
          }
        }
      }
    }

    push_frame_into_queue(frame);
  }
}


const vidio_error* vidio_input_file::stop_capturing()
{
  if (m_stop_mode == vidio_file_stop_mode_continue) {
    // Continue mode: don't stop the thread, just let frames overflow and discard
    return nullptr;
  }

  // Pause mode: stop thread, keep reader open at current position
  m_stop_requested = true;
  m_reader->stop();

  if (m_capturing_thread.joinable()) {
    m_capturing_thread.join();
    send_callback_message(vidio_input_message_end_of_stream);
  }

  // Reset stop flags so reader can be reused — but DON'T close the reader
  m_reader->resume();
  m_stop_requested = false;

  // Clear stale frames from queue
  {
    std::lock_guard<std::mutex> lock(m_queue_mutex);
    for (auto* frame : m_frame_queue) {
      delete frame;
    }
    m_frame_queue.clear();
  }

  return nullptr;
}


const vidio_frame* vidio_input_file::peek_next_frame() const
{
  std::lock_guard<std::mutex> lock(m_queue_mutex);

  if (m_frame_queue.empty()) {
    return nullptr;
  }
  else {
    return m_frame_queue.front();
  }
}


void vidio_input_file::pop_next_frame()
{
  std::lock_guard<std::mutex> lock(m_queue_mutex);
  delete m_frame_queue.front();
  m_frame_queue.pop_front();
}


void vidio_input_file::push_frame_into_queue(const vidio_frame* f)
{
  bool overflow = false;

  {
    std::lock_guard<std::mutex> lock(m_queue_mutex);

    if (m_frame_queue.size() < cMaxFrameQueueLength) {
      m_frame_queue.push_back(f);
    }
    else {
      overflow = true;
      delete f;
    }
  }

  if (overflow) {
    send_callback_message(vidio_input_message_input_overflow);
  }
  else {
    send_callback_message(vidio_input_message_new_frame);
  }
}


std::string vidio_input_file::serialize(vidio_serialization_format serialformat) const
{
#if WITH_JSON
  if (serialformat == vidio_serialization_format_json) {
    nlohmann::json json{
        {"class", "file"},
        {"path", m_filepath}
    };

    return json.dump();
  }
#endif

  return {};
}


#if WITH_JSON
vidio_input_file* vidio_input_file::find_matching_device(
    const std::vector<vidio_input*>& inputs,
    const nlohmann::json& json)
{
  if (!json.contains("path")) {
    return nullptr;
  }

  std::string path = json["path"];
  return create(path);
}
#endif
