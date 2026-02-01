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

#include "vidio_input_device_rtsp.h"
#include <libvidio/vidio_frame.h>
#include <algorithm>


vidio_input_device_rtsp::vidio_input_device_rtsp(const std::string& url)
    : m_url(url), m_stream(std::make_unique<vidio_rtsp_stream>())
{
  m_stream->set_url(url);
}


vidio_input_device_rtsp::vidio_input_device_rtsp(const std::string& url,
                                                 const std::string& username,
                                                 const std::string& password)
    : m_url(url), m_username(username), m_password(password),
      m_stream(std::make_unique<vidio_rtsp_stream>())
{
  m_stream->set_url(url);
  m_stream->set_credentials(username, password);
}


vidio_input_device_rtsp::~vidio_input_device_rtsp()
{
  stop_capturing();

  // Clear any remaining frames in the queue
  std::lock_guard<std::mutex> lock(m_queue_mutex);
  for (auto* frame : m_frame_queue) {
    delete frame;
  }
  m_frame_queue.clear();
}


vidio_input_device_rtsp* vidio_input_device_rtsp::create(const std::string& url)
{
  return new vidio_input_device_rtsp(url);
}


vidio_input_device_rtsp* vidio_input_device_rtsp::create(const std::string& url,
                                                         const std::string& username,
                                                         const std::string& password)
{
  return new vidio_input_device_rtsp(url, username, password);
}


std::string vidio_input_device_rtsp::get_display_name() const
{
  // Return URL without credentials for display
  return m_url;
}


std::vector<vidio_video_format*> vidio_input_device_rtsp::get_video_formats() const
{
  std::vector<vidio_video_format*> formats;

  // If we're connected, return the actual format
  if (m_connected && m_current_format) {
    formats.push_back(m_current_format->clone());
  }

  // For RTSP, we typically don't know the formats until we connect
  // Return empty list if not connected
  return formats;
}


const vidio_error* vidio_input_device_rtsp::set_capture_format(const vidio_video_format* requested_format,
                                                               const vidio_video_format** out_actual_format)
{
  // For RTSP, we accept the format from the stream.
  // The requested_format parameter is currently ignored as RTSP streams
  // don't typically support format negotiation.

  // Connect to get actual format info
  if (!m_connected) {
    const vidio_error* err = m_stream->connect();
    if (err) {
      return err;
    }
    m_connected = true;

    // Create format from stream info
    vidio_fraction framerate = m_stream->get_framerate();
    std::optional<vidio_fraction> opt_framerate;
    if (framerate.numerator > 0) {
      opt_framerate = framerate;
    }

    m_current_format = std::make_unique<vidio_video_format_rtsp>(
        static_cast<uint32_t>(m_stream->get_width()),
        static_cast<uint32_t>(m_stream->get_height()),
        m_stream->get_pixel_format(),
        opt_framerate);
  }

  if (out_actual_format && m_current_format) {
    *out_actual_format = m_current_format->clone();
  }

  return nullptr;
}


const vidio_error* vidio_input_device_rtsp::start_capturing()
{
  // Connect if not already connected
  if (!m_connected) {
    const vidio_error* err = m_stream->connect();
    if (err) {
      return err;
    }
    m_connected = true;

    // Create format from stream info
    vidio_fraction framerate = m_stream->get_framerate();
    std::optional<vidio_fraction> opt_framerate;
    if (framerate.numerator > 0) {
      opt_framerate = framerate;
    }

    m_current_format = std::make_unique<vidio_video_format_rtsp>(
        static_cast<uint32_t>(m_stream->get_width()),
        static_cast<uint32_t>(m_stream->get_height()),
        m_stream->get_pixel_format(),
        opt_framerate);
  }

  // Start capturing in a separate thread
  m_capturing_thread = std::thread(&vidio_rtsp_stream::start_capturing_blocking,
                                   m_stream.get(), this);

  return nullptr;
}


const vidio_error* vidio_input_device_rtsp::stop_capturing()
{
  m_stream->stop_capturing();

  if (m_capturing_thread.joinable()) {
    m_capturing_thread.join();
    send_callback_message(vidio_input_message_end_of_stream);
  }

  m_stream->disconnect();
  m_connected = false;

  return nullptr;
}


const vidio_frame* vidio_input_device_rtsp::peek_next_frame() const
{
  std::lock_guard<std::mutex> lock(m_queue_mutex);

  if (m_frame_queue.empty()) {
    return nullptr;
  }
  else {
    return m_frame_queue.front();
  }
}


void vidio_input_device_rtsp::pop_next_frame()
{
  std::lock_guard<std::mutex> lock(m_queue_mutex);
  delete m_frame_queue.front();
  m_frame_queue.pop_front();
}


void vidio_input_device_rtsp::push_frame_into_queue(const vidio_frame* f)
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


std::string vidio_input_device_rtsp::serialize(vidio_serialization_format serialformat) const
{
#if WITH_JSON
  if (serialformat == vidio_serialization_format_json) {
    nlohmann::json json{
        {"class", "rtsp"},
        {"url", m_url}
    };

    // Note: We intentionally don't serialize credentials for security

    return json.dump();
  }
#endif

  return {};
}


void vidio_input_device_rtsp::set_transport(vidio_rtsp_transport transport)
{
  m_stream->set_transport(transport);
}


void vidio_input_device_rtsp::set_timeout_seconds(int timeout_seconds)
{
  m_stream->set_timeout_seconds(timeout_seconds);
}


#if WITH_JSON
vidio_input_device_rtsp* vidio_input_device_rtsp::find_matching_device(
    const std::vector<vidio_input*>& inputs,
    const nlohmann::json& json)
{
  if (!json.contains("url")) {
    return nullptr;
  }

  std::string url = json["url"];

  // For RTSP, we create a new device rather than matching existing ones
  // since RTSP devices are not discoverable like physical devices
  return create(url);
}
#endif
