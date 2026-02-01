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

#ifndef LIBVIDIO_VIDIO_RTSP_STREAM_H
#define LIBVIDIO_VIDIO_RTSP_STREAM_H

#include <libvidio/vidio.h>
#include <libvidio/vidio_error.h>
#include <string>
#include <atomic>

extern "C" {
#include <libavformat/avformat.h>
#include <libavcodec/avcodec.h>
}

struct vidio_frame;
struct vidio_input_device_rtsp;


class vidio_rtsp_stream
{
public:
  vidio_rtsp_stream();
  ~vidio_rtsp_stream();

  void set_url(const std::string& url) { m_url = url; }
  void set_credentials(const std::string& username, const std::string& password);
  void set_transport(vidio_rtsp_transport transport) { m_transport = transport; }
  void set_timeout_seconds(int timeout_seconds) { m_timeout_seconds = timeout_seconds; }

  const vidio_error* connect();
  void disconnect();

  bool is_connected() const { return m_av_format_context != nullptr; }

  int get_width() const { return m_width; }
  int get_height() const { return m_height; }
  vidio_fraction get_framerate() const { return m_framerate; }
  vidio_pixel_format get_pixel_format() const { return m_pixel_format; }

  void start_capturing_blocking(vidio_input_device_rtsp* device);
  void stop_capturing();

private:
  // --- Configuration (set before connect()) ---

  std::string m_url;
  std::string m_username;
  std::string m_password;
  vidio_rtsp_transport m_transport = vidio_rtsp_transport_auto;
  int m_timeout_seconds = 10;  // connection timeout in seconds

  // --- FFmpeg state (valid after connect()) ---

  AVFormatContext* m_av_format_context = nullptr;
  int m_video_stream_index = -1;  // index of video stream within m_av_format_context

  // --- Stream properties (populated by connect() from RTSP stream) ---

  int m_width = 0;
  int m_height = 0;
  vidio_fraction m_framerate{0, 1};
  vidio_pixel_format m_pixel_format = vidio_pixel_format_undefined;

  // --- Capturing state ---

  std::atomic<bool> m_stop_capturing{false};  // signals capture thread to stop

  // --- Helper methods ---

  std::string build_url_with_credentials() const;
  vidio_pixel_format codec_id_to_pixel_format(AVCodecID codec_id) const;
};

#endif //LIBVIDIO_VIDIO_RTSP_STREAM_H
