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

#include "vidio_file_reader.h"
#include <libvidio/vidio_frame.h>

extern "C" {
#include <libavutil/imgutils.h>
}


vidio_file_reader::vidio_file_reader()
{
}


vidio_file_reader::~vidio_file_reader()
{
  close();
}


bool vidio_file_reader::is_passthrough_codec(AVCodecID codec_id)
{
  switch (codec_id) {
    case AV_CODEC_ID_H264:
    case AV_CODEC_ID_HEVC:
    case AV_CODEC_ID_MJPEG:
      return true;
    default:
      return false;
  }
}


vidio_pixel_format vidio_file_reader::codec_id_to_pixel_format(AVCodecID codec_id) const
{
  switch (codec_id) {
    case AV_CODEC_ID_H264:
      return vidio_pixel_format_H264;
    case AV_CODEC_ID_HEVC:
      return vidio_pixel_format_H265;
    case AV_CODEC_ID_MJPEG:
      return vidio_pixel_format_MJPEG;
    default:
      // For non-passthrough codecs, we decode to YUV420
      return vidio_pixel_format_YUV420_planar;
  }
}


const vidio_error* vidio_file_reader::open(const std::string& filepath)
{
  m_stop = false;

  if (m_av_format_context) {
    close();
  }

  int ret = avformat_open_input(&m_av_format_context, filepath.c_str(), nullptr, nullptr);
  if (ret < 0) {
    char errbuf[AV_ERROR_MAX_STRING_SIZE];
    av_strerror(ret, errbuf, sizeof(errbuf));

    if (ret == AVERROR(ENOENT)) {
      auto* err = new vidio_error(vidio_error_code_file_not_found,
                                  "File not found: {0}");
      err->set_arg(0, filepath);
      return err;
    }

    auto* err = new vidio_error(vidio_error_code_file_read_error,
                                "Failed to open file: {0}");
    err->set_arg(0, errbuf);
    return err;
  }

  ret = avformat_find_stream_info(m_av_format_context, nullptr);
  if (ret < 0) {
    char errbuf[AV_ERROR_MAX_STRING_SIZE];
    av_strerror(ret, errbuf, sizeof(errbuf));

    auto* err = new vidio_error(vidio_error_code_file_read_error,
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
    auto* err = new vidio_error(vidio_error_code_file_no_video_stream,
                                "No video stream found in file");
    avformat_close_input(&m_av_format_context);
    return err;
  }

  AVCodecParameters* codecpar = m_av_format_context->streams[m_video_stream_index]->codecpar;
  m_width = codecpar->width;
  m_height = codecpar->height;

  m_compressed_passthrough = is_passthrough_codec(codecpar->codec_id);
  m_pixel_format = codec_id_to_pixel_format(codecpar->codec_id);

  // For H264/H265 from MP4, set up bitstream filter to convert AVCC → Annex B
  if (m_compressed_passthrough && codecpar->codec_id != AV_CODEC_ID_MJPEG) {
    const char* bsf_name = (codecpar->codec_id == AV_CODEC_ID_H264)
        ? "h264_mp4toannexb"
        : "hevc_mp4toannexb";

    const AVBitStreamFilter* bsf = av_bsf_get_by_name(bsf_name);
    if (bsf) {
      av_bsf_alloc(bsf, &m_bsf_context);
      avcodec_parameters_copy(m_bsf_context->par_in, codecpar);
      m_bsf_context->time_base_in = m_av_format_context->streams[m_video_stream_index]->time_base;
      av_bsf_init(m_bsf_context);
    }
  }

  // For non-passthrough codecs, set up decoder
  if (!m_compressed_passthrough) {
    const AVCodec* codec = avcodec_find_decoder(codecpar->codec_id);
    if (!codec) {
      auto* err = new vidio_error(vidio_error_code_file_unsupported_codec,
                                  "No decoder found for codec");
      avformat_close_input(&m_av_format_context);
      return err;
    }

    m_codec_context = avcodec_alloc_context3(codec);
    if (!m_codec_context) {
      auto* err = new vidio_error(vidio_error_code_internal_error,
                                  "Failed to allocate codec context");
      avformat_close_input(&m_av_format_context);
      return err;
    }

    ret = avcodec_parameters_to_context(m_codec_context, codecpar);
    if (ret < 0) {
      auto* err = new vidio_error(vidio_error_code_internal_error,
                                  "Failed to copy codec parameters");
      avcodec_free_context(&m_codec_context);
      avformat_close_input(&m_av_format_context);
      return err;
    }

    ret = avcodec_open2(m_codec_context, codec, nullptr);
    if (ret < 0) {
      char errbuf[AV_ERROR_MAX_STRING_SIZE];
      av_strerror(ret, errbuf, sizeof(errbuf));

      auto* err = new vidio_error(vidio_error_code_file_unsupported_codec,
                                  "Failed to open codec: {0}");
      err->set_arg(0, errbuf);
      avcodec_free_context(&m_codec_context);
      avformat_close_input(&m_av_format_context);
      return err;
    }
  }

  // Get framerate
  AVRational frame_rate = m_av_format_context->streams[m_video_stream_index]->avg_frame_rate;
  if (frame_rate.num > 0 && frame_rate.den > 0) {
    m_framerate.numerator = frame_rate.num;
    m_framerate.denominator = frame_rate.den;
  }
  else {
    frame_rate = m_av_format_context->streams[m_video_stream_index]->r_frame_rate;
    if (frame_rate.num > 0 && frame_rate.den > 0) {
      m_framerate.numerator = frame_rate.num;
      m_framerate.denominator = frame_rate.den;
    }
    else {
      m_framerate.numerator = 25;
      m_framerate.denominator = 1;
    }
  }

  return nullptr;
}


void vidio_file_reader::close()
{
  if (m_bsf_context) {
    av_bsf_free(&m_bsf_context);
  }

  if (m_sws_context) {
    sws_freeContext(m_sws_context);
    m_sws_context = nullptr;
  }

  if (m_codec_context) {
    avcodec_free_context(&m_codec_context);
  }

  if (m_av_format_context) {
    avformat_close_input(&m_av_format_context);
  }

  m_video_stream_index = -1;
  m_width = 0;
  m_height = 0;
  m_framerate = {0, 1};
  m_pixel_format = vidio_pixel_format_undefined;
  m_compressed_passthrough = false;
}


vidio_frame* vidio_file_reader::create_compressed_frame(AVPacket* pkt)
{
  // Convert AVCC → Annex B if bitstream filter is active
  if (m_bsf_context) {
    av_bsf_send_packet(m_bsf_context, pkt);
    int ret = av_bsf_receive_packet(m_bsf_context, pkt);
    if (ret < 0) {
      return nullptr;
    }
  }

  auto* frame = new vidio_frame();
  frame->set_format(m_pixel_format, m_width, m_height);

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

  frame->set_keyframe((pkt->flags & AV_PKT_FLAG_KEY) != 0);

  AVRational time_base = m_av_format_context->streams[m_video_stream_index]->time_base;
  if (pkt->pts != AV_NOPTS_VALUE) {
    int64_t pts_us = av_rescale_q(pkt->pts, time_base, {1, 1000000});
    frame->set_timestamp_us(static_cast<uint64_t>(pts_us));
  }

  if (pkt->dts != AV_NOPTS_VALUE) {
    int64_t dts_us = av_rescale_q(pkt->dts, time_base, {1, 1000000});
    frame->set_dts_us(dts_us);
  }

  AVCodecParameters* extradata_source = m_bsf_context
      ? m_bsf_context->par_out
      : m_av_format_context->streams[m_video_stream_index]->codecpar;
  if (frame->is_keyframe() && extradata_source->extradata && extradata_source->extradata_size > 0) {
    frame->set_codec_extradata(extradata_source->extradata, extradata_source->extradata_size);
  }

  return frame;
}


vidio_frame* vidio_file_reader::decode_frame(AVPacket* pkt)
{
  int ret = avcodec_send_packet(m_codec_context, pkt);
  if (ret < 0) {
    return nullptr;
  }

  AVFrame* av_frame = av_frame_alloc();
  if (!av_frame) {
    return nullptr;
  }

  ret = avcodec_receive_frame(m_codec_context, av_frame);
  if (ret < 0) {
    av_frame_free(&av_frame);
    return nullptr;
  }

  // Convert to YUV420 planar via swscale if needed
  AVPixelFormat src_format = static_cast<AVPixelFormat>(av_frame->format);
  AVPixelFormat dst_format = AV_PIX_FMT_YUV420P;

  if (src_format != dst_format) {
    if (!m_sws_context) {
      m_sws_context = sws_getContext(
          av_frame->width, av_frame->height, src_format,
          av_frame->width, av_frame->height, dst_format,
          SWS_BILINEAR, nullptr, nullptr, nullptr);
    }

    if (!m_sws_context) {
      av_frame_free(&av_frame);
      return nullptr;
    }

    AVFrame* dst_frame = av_frame_alloc();
    dst_frame->width = av_frame->width;
    dst_frame->height = av_frame->height;
    dst_frame->format = dst_format;
    av_image_alloc(dst_frame->data, dst_frame->linesize,
                   dst_frame->width, dst_frame->height, dst_format, 1);

    sws_scale(m_sws_context,
              av_frame->data, av_frame->linesize,
              0, av_frame->height,
              dst_frame->data, dst_frame->linesize);

    // Copy PTS from source
    dst_frame->pts = av_frame->pts;
    av_frame_free(&av_frame);
    av_frame = dst_frame;
  }

  // Build vidio_frame from decoded AVFrame
  auto* frame = new vidio_frame();
  frame->set_format(vidio_pixel_format_YUV420_planar, av_frame->width, av_frame->height);

  frame->add_raw_plane(vidio_color_channel_Y, 8);
  frame->add_raw_plane(vidio_color_channel_U, 8);
  frame->add_raw_plane(vidio_color_channel_V, 8);

  // Copy Y plane
  int y_stride = 0;
  uint8_t* y_dst = frame->get_plane(vidio_color_channel_Y, &y_stride);
  for (int row = 0; row < av_frame->height; row++) {
    memcpy(y_dst + row * y_stride,
           av_frame->data[0] + row * av_frame->linesize[0],
           static_cast<size_t>(av_frame->width));
  }

  // Copy U and V planes (half dimensions for YUV420)
  int chroma_h = (av_frame->height + 1) / 2;
  int chroma_w = (av_frame->width + 1) / 2;

  int u_stride = 0;
  uint8_t* u_dst = frame->get_plane(vidio_color_channel_U, &u_stride);
  for (int row = 0; row < chroma_h; row++) {
    memcpy(u_dst + row * u_stride,
           av_frame->data[1] + row * av_frame->linesize[1],
           static_cast<size_t>(chroma_w));
  }

  int v_stride = 0;
  uint8_t* v_dst = frame->get_plane(vidio_color_channel_V, &v_stride);
  for (int row = 0; row < chroma_h; row++) {
    memcpy(v_dst + row * v_stride,
           av_frame->data[2] + row * av_frame->linesize[2],
           static_cast<size_t>(chroma_w));
  }

  // Set timestamp
  AVRational time_base = m_av_format_context->streams[m_video_stream_index]->time_base;
  if (av_frame->pts != AV_NOPTS_VALUE) {
    int64_t pts_us = av_rescale_q(av_frame->pts, time_base, {1, 1000000});
    frame->set_timestamp_us(static_cast<uint64_t>(pts_us));
  }

  // Decoded frames are always independently displayable
  frame->set_keyframe(true);

  if (src_format != dst_format) {
    // We allocated with av_image_alloc, need to free data[0]
    av_freep(&av_frame->data[0]);
  }
  av_frame_free(&av_frame);

  return frame;
}


vidio_frame* vidio_file_reader::flush_decoder()
{
  if (!m_codec_context) {
    return nullptr;
  }

  // Send flush packet
  avcodec_send_packet(m_codec_context, nullptr);

  AVFrame* av_frame = av_frame_alloc();
  if (!av_frame) {
    return nullptr;
  }

  int ret = avcodec_receive_frame(m_codec_context, av_frame);
  if (ret < 0) {
    av_frame_free(&av_frame);
    return nullptr;
  }

  // Reuse decode path for conversion
  // We need to wrap this in a packet-like call; instead, build frame directly
  auto* frame = new vidio_frame();
  frame->set_format(vidio_pixel_format_YUV420_planar, av_frame->width, av_frame->height);

  // Same conversion logic as decode_frame
  AVPixelFormat src_format = static_cast<AVPixelFormat>(av_frame->format);
  AVPixelFormat dst_format = AV_PIX_FMT_YUV420P;

  if (src_format != dst_format && m_sws_context) {
    AVFrame* dst_frame = av_frame_alloc();
    dst_frame->width = av_frame->width;
    dst_frame->height = av_frame->height;
    dst_frame->format = dst_format;
    av_image_alloc(dst_frame->data, dst_frame->linesize,
                   dst_frame->width, dst_frame->height, dst_format, 1);

    sws_scale(m_sws_context,
              av_frame->data, av_frame->linesize,
              0, av_frame->height,
              dst_frame->data, dst_frame->linesize);

    dst_frame->pts = av_frame->pts;
    av_frame_free(&av_frame);
    av_frame = dst_frame;
  }

  frame->add_raw_plane(vidio_color_channel_Y, 8);
  frame->add_raw_plane(vidio_color_channel_U, 8);
  frame->add_raw_plane(vidio_color_channel_V, 8);

  int y_stride = 0;
  uint8_t* y_dst = frame->get_plane(vidio_color_channel_Y, &y_stride);
  for (int row = 0; row < av_frame->height; row++) {
    memcpy(y_dst + row * y_stride,
           av_frame->data[0] + row * av_frame->linesize[0],
           static_cast<size_t>(av_frame->width));
  }

  int chroma_h = (av_frame->height + 1) / 2;
  int chroma_w = (av_frame->width + 1) / 2;

  int u_stride = 0;
  uint8_t* u_dst = frame->get_plane(vidio_color_channel_U, &u_stride);
  for (int row = 0; row < chroma_h; row++) {
    memcpy(u_dst + row * u_stride,
           av_frame->data[1] + row * av_frame->linesize[1],
           static_cast<size_t>(chroma_w));
  }

  int v_stride = 0;
  uint8_t* v_dst = frame->get_plane(vidio_color_channel_V, &v_stride);
  for (int row = 0; row < chroma_h; row++) {
    memcpy(v_dst + row * v_stride,
           av_frame->data[2] + row * av_frame->linesize[2],
           static_cast<size_t>(chroma_w));
  }

  AVRational time_base = m_av_format_context->streams[m_video_stream_index]->time_base;
  if (av_frame->pts != AV_NOPTS_VALUE) {
    int64_t pts_us = av_rescale_q(av_frame->pts, time_base, {1, 1000000});
    frame->set_timestamp_us(static_cast<uint64_t>(pts_us));
  }

  frame->set_keyframe(true);

  if (src_format != dst_format) {
    av_freep(&av_frame->data[0]);
  }
  av_frame_free(&av_frame);

  return frame;
}


vidio_frame* vidio_file_reader::read_next_frame()
{
  if (!m_av_format_context || m_video_stream_index < 0) {
    return nullptr;
  }

  AVPacket* pkt = av_packet_alloc();
  if (!pkt) {
    return nullptr;
  }

  vidio_frame* result = nullptr;

  while (!m_stop && !result) {
    int ret = av_read_frame(m_av_format_context, pkt);

    if (ret < 0) {
      // EOF or error
      av_packet_free(&pkt);

      // Flush remaining decoded frames
      if (!m_compressed_passthrough) {
        return flush_decoder();
      }

      return nullptr;
    }

    if (pkt->stream_index == m_video_stream_index) {
      if (m_compressed_passthrough) {
        result = create_compressed_frame(pkt);
      }
      else {
        result = decode_frame(pkt);
      }
    }

    av_packet_unref(pkt);
  }

  av_packet_free(&pkt);
  return result;
}


bool vidio_file_reader::seek_to_beginning()
{
  if (!m_av_format_context) {
    return false;
  }

  int ret = av_seek_frame(m_av_format_context, m_video_stream_index, 0, AVSEEK_FLAG_BACKWARD);
  if (ret < 0) {
    return false;
  }

  // Flush decoder state after seek
  if (m_codec_context) {
    avcodec_flush_buffers(m_codec_context);
  }

  return true;
}
