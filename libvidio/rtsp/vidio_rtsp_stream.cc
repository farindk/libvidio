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

#include "vidio_rtsp_stream.h"
#include "vidio_input_device_rtsp.h"
#include <libvidio/vidio_frame.h>


vidio_rtsp_stream::vidio_rtsp_stream()
{
}


vidio_rtsp_stream::~vidio_rtsp_stream()
{
  disconnect();
}


void vidio_rtsp_stream::set_credentials(const std::string& username, const std::string& password)
{
  m_username = username;
  m_password = password;
}


std::string vidio_rtsp_stream::build_url_with_credentials() const
{
  if (m_username.empty()) {
    return m_url;
  }

  // Insert credentials into URL: rtsp://user:pass@host/path
  std::string result = m_url;
  const std::string rtsp_prefix = "rtsp://";

  if (result.find(rtsp_prefix) == 0) {
    std::string credentials = m_username;
    if (!m_password.empty()) {
      credentials += ":" + m_password;
    }
    credentials += "@";
    result.insert(rtsp_prefix.length(), credentials);
  }

  return result;
}


const vidio_error* vidio_rtsp_stream::connect()
{
  if (m_av_format_context) {
    disconnect();
  }

  m_av_format_context = avformat_alloc_context();
  if (!m_av_format_context) {
    return new vidio_error(vidio_error_code_internal_error, "Failed to allocate AVFormatContext");
  }

  AVDictionary* opts = nullptr;

  // Set transport protocol
  switch (m_transport) {
    case vidio_rtsp_transport_tcp:
      av_dict_set(&opts, "rtsp_transport", "tcp", 0);
      break;
    case vidio_rtsp_transport_udp:
      av_dict_set(&opts, "rtsp_transport", "udp", 0);
      break;
    case vidio_rtsp_transport_auto:
    default:
      // Let FFmpeg decide
      break;
  }

  // Set timeout (FFmpeg expects microseconds)
  std::string timeout_str = std::to_string(static_cast<int64_t>(m_timeout_seconds) * 1000000);
  av_dict_set(&opts, "stimeout", timeout_str.c_str(), 0);

  // Set reasonable buffer sizes for real-time streaming
  av_dict_set(&opts, "buffer_size", "1024000", 0);

  std::string url = build_url_with_credentials();

  int ret = avformat_open_input(&m_av_format_context, url.c_str(), nullptr, &opts);
  av_dict_free(&opts);

  if (ret < 0) {
    char errbuf[AV_ERROR_MAX_STRING_SIZE];
    av_strerror(ret, errbuf, sizeof(errbuf));

    auto* err = new vidio_error(vidio_error_code_rtsp_connection_failed,
                                "Failed to open RTSP stream: {0}");
    err->set_arg(0, errbuf);

    m_av_format_context = nullptr;
    return err;
  }

  // Find stream info
  ret = avformat_find_stream_info(m_av_format_context, nullptr);
  if (ret < 0) {
    char errbuf[AV_ERROR_MAX_STRING_SIZE];
    av_strerror(ret, errbuf, sizeof(errbuf));

    auto* err = new vidio_error(vidio_error_code_rtsp_stream_not_found,
                                "Failed to find stream info: {0}");
    err->set_arg(0, errbuf);

    avformat_close_input(&m_av_format_context);
    return err;
  }

  // Find the video stream
  m_video_stream_index = -1;
  for (unsigned int i = 0; i < m_av_format_context->nb_streams; i++) {
    if (m_av_format_context->streams[i]->codecpar->codec_type == AVMEDIA_TYPE_VIDEO) {
      m_video_stream_index = static_cast<int>(i);
      break;
    }
  }

  if (m_video_stream_index < 0) {
    auto* err = new vidio_error(vidio_error_code_rtsp_stream_not_found,
                                "No video stream found in RTSP stream");
    avformat_close_input(&m_av_format_context);
    return err;
  }

  // Get stream parameters
  AVCodecParameters* codecpar = m_av_format_context->streams[m_video_stream_index]->codecpar;
  m_width = codecpar->width;
  m_height = codecpar->height;
  m_pixel_format = codec_id_to_pixel_format(codecpar->codec_id);

  if (m_pixel_format == vidio_pixel_format_undefined) {
    auto* err = new vidio_error(vidio_error_code_rtsp_unsupported_codec,
                                "Unsupported video codec in RTSP stream");
    avformat_close_input(&m_av_format_context);
    return err;
  }

  // Get framerate
  AVRational frame_rate = m_av_format_context->streams[m_video_stream_index]->avg_frame_rate;
  if (frame_rate.num > 0 && frame_rate.den > 0) {
    m_framerate.numerator = frame_rate.num;
    m_framerate.denominator = frame_rate.den;
  }
  else {
    // Try r_frame_rate as fallback
    frame_rate = m_av_format_context->streams[m_video_stream_index]->r_frame_rate;
    if (frame_rate.num > 0 && frame_rate.den > 0) {
      m_framerate.numerator = frame_rate.num;
      m_framerate.denominator = frame_rate.den;
    }
    else {
      m_framerate.numerator = 0;
      m_framerate.denominator = 1;
    }
  }

  return nullptr;
}


void vidio_rtsp_stream::disconnect()
{
  if (m_av_format_context) {
    avformat_close_input(&m_av_format_context);
    m_av_format_context = nullptr;
  }

  m_video_stream_index = -1;
  m_width = 0;
  m_height = 0;
  m_framerate = {0, 1};
  m_pixel_format = vidio_pixel_format_undefined;
}


void vidio_rtsp_stream::start_capturing_blocking(vidio_input_device_rtsp* device)
{
  if (!m_av_format_context || m_video_stream_index < 0) {
    return;
  }

  m_stop_capturing = false;

  AVPacket* pkt = av_packet_alloc();
  if (!pkt) {
    return;
  }

  while (!m_stop_capturing) {
    int ret = av_read_frame(m_av_format_context, pkt);

    if (ret < 0) {
      if (ret == AVERROR_EOF || ret == AVERROR(EAGAIN)) {
        // End of stream or would block - wait a bit and retry
        av_packet_unref(pkt);
        continue;
      }

      // Connection lost or other error
      break;
    }

    if (pkt->stream_index == m_video_stream_index) {
      // Create a vidio_frame with the compressed data
      auto* frame = new vidio_frame();
      frame->set_format(m_pixel_format, m_width, m_height);

      // Determine channel format based on codec
      vidio_channel_format channel_format;
      switch (m_pixel_format) {
        case vidio_pixel_format_H264:
          channel_format = vidio_channel_format_compressed_H264;
          break;
        case vidio_pixel_format_H265:
          channel_format = vidio_channel_format_compressed_H265;
          break;
        case vidio_pixel_format_MJPEG:
          channel_format = vidio_channel_format_compressed_MJPEG;
          break;
        default:
          channel_format = vidio_channel_format_undefined;
          break;
      }

      frame->add_compressed_plane(vidio_color_channel_compressed,
                                  channel_format, 8,
                                  pkt->data, pkt->size,
                                  m_width, m_height);

      // Set timestamp if available
      if (pkt->pts != AV_NOPTS_VALUE) {
        AVRational time_base = m_av_format_context->streams[m_video_stream_index]->time_base;
        int64_t pts_us = av_rescale_q(pkt->pts, time_base, {1, 1000000});
        frame->set_timestamp_us(static_cast<uint64_t>(pts_us));
      }

      device->push_frame_into_queue(frame);
    }

    av_packet_unref(pkt);
  }

  av_packet_free(&pkt);
}


void vidio_rtsp_stream::stop_capturing()
{
  m_stop_capturing = true;
}


vidio_pixel_format vidio_rtsp_stream::codec_id_to_pixel_format(AVCodecID codec_id) const
{
  switch (codec_id) {
    case AV_CODEC_ID_H264:
      return vidio_pixel_format_H264;
    case AV_CODEC_ID_HEVC:
      return vidio_pixel_format_H265;
    case AV_CODEC_ID_MJPEG:
      return vidio_pixel_format_MJPEG;
    default:
      return vidio_pixel_format_undefined;
  }
}
