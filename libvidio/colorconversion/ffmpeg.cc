/*
 * VidIO library
 * Copyright (c) 2024 Dirk Farin <dirk.farin@gmail.com>
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

#include "ffmpeg.h"
#include "common.h"
#include <cassert>

extern "C"
{
#include <libswscale/swscale.h>
}


vidio_format_converter_ffmpeg::~vidio_format_converter_ffmpeg()
{
  avcodec_free_context(&m_context);
  av_frame_free(&m_decodedFrame);
  //av_packet_free(&m_pkt);
  sws_freeContext(m_swscaleContext);
}


vidio_error* vidio_format_converter_ffmpeg::init(enum AVCodecID codecId, vidio_pixel_format output_format)
{
  if (output_format != vidio_pixel_format_RGB8 &&
      output_format != vidio_pixel_format_YUV422_YUYV) {
    return nullptr;
  }

  // AVCodec

  m_codec = avcodec_find_decoder(codecId);
  if (!m_codec) {
    return nullptr;
  }

  // AVContext

  m_context = avcodec_alloc_context3(m_codec);
  if (!m_context) {
    return nullptr; //Error
  }

  if (avcodec_open2(m_context, m_codec, nullptr) < 0) {
    return nullptr;
  }

  // AVFrame

  m_decodedFrame = av_frame_alloc();
  if (!m_decodedFrame) {
    return nullptr;
  }

  m_output_format = output_format;

  return nullptr;
}


void vidio_format_converter_ffmpeg::push(const vidio_frame* input)
{
  // AVPacket

  AVPacket* pkt = av_packet_alloc();
  if (!pkt) {
    return;
  }

  const uint8_t* in;
  int in_stride;
  in = input->get_plane(vidio_color_channel_compressed, &in_stride);

  int res = av_new_packet(pkt, in_stride);
  if (res != 0) {
    return;
  }

  memcpy(pkt->data, in, in_stride);

  // decode frame

  res = avcodec_send_packet(m_context, pkt);
  if (res != 0) {
    return;
  }

  res = avcodec_receive_frame(m_context, m_decodedFrame);
  if (res != 0) {
    return;
  }

  av_packet_free(&pkt);

  // convert to vidio_frame

  vidio_frame* out_frame = convert_avframe_to_vidio_frame(m_context->pix_fmt, m_decodedFrame,
                                                          m_output_format);

  out_frame->copy_metadata_from(input);
  push_decoded_frame(out_frame);
}


vidio_frame* vidio_format_converter_ffmpeg::convert_avframe_to_vidio_frame(AVPixelFormat input_format,
                                                                           AVFrame* input,
                                                                           vidio_pixel_format output_format)
{
  int w = input->width;
  int h = input->height;

  AVPixelFormat output_av_format = AV_PIX_FMT_NONE;

  vidio_frame* out_frame = new vidio_frame();
  out_frame->set_format(output_format, w, h);

  uint8_t* out_data[3];
  int out_stride[3];

  switch (output_format) {
    case vidio_pixel_format_RGB8:
      output_av_format = AV_PIX_FMT_RGB24;
      out_frame->add_raw_plane(vidio_color_channel_interleaved, w, h, 24);
      out_data[0] = out_frame->get_plane(vidio_color_channel_interleaved, &out_stride[0]);
      break;
    case vidio_pixel_format_YUV422_YUYV:
      output_av_format = AV_PIX_FMT_YUYV422;
      out_frame->add_raw_plane(vidio_color_channel_interleaved, w, h, 16);
      out_data[0] = out_frame->get_plane(vidio_color_channel_interleaved, &out_stride[0]);
      break;
    default:
      printf("output format assert: %d\n", output_format);
      assert(false);
      break;
  }

  // SWScale conversion

  if (m_swscaleContext == nullptr) {
    m_swscaleContext = sws_getContext(w, h, m_context->pix_fmt,
                                      w, h, output_av_format,
                                      SWS_FAST_BILINEAR, NULL, NULL, NULL);
  }

  sws_scale(m_swscaleContext, m_decodedFrame->data, m_decodedFrame->linesize,
            0, h,
            out_data, out_stride);

  return out_frame;
}


vidio_format_converter_swscale::vidio_format_converter_swscale(vidio_pixel_format output_format)
{
  m_output_format = output_format;
}

vidio_format_converter_swscale::~vidio_format_converter_swscale()
{
  sws_freeContext(m_swscaleContext);
}

void vidio_format_converter_swscale::push(const vidio_frame* in_frame)
{
  int w = in_frame->get_width();
  int h = in_frame->get_height();

  AVPixelFormat input_av_format = AV_PIX_FMT_NONE;
  AVPixelFormat output_av_format = AV_PIX_FMT_NONE;

  const uint8_t* in_data[3];
  int in_stride[3];

  switch (in_frame->get_pixel_format()) {
    case vidio_pixel_format_RGB8:
      input_av_format = AV_PIX_FMT_RGB24;
      in_data[0] = in_frame->get_plane(vidio_color_channel_interleaved, &in_stride[0]);
      break;
    case vidio_pixel_format_YUV422_YUYV:
      input_av_format = AV_PIX_FMT_YUYV422;
      in_data[0] = in_frame->get_plane(vidio_color_channel_interleaved, &in_stride[0]);
      break;
    case vidio_pixel_format_RGGB8:
      input_av_format = AV_PIX_FMT_BAYER_RGGB8;
      in_data[0] = in_frame->get_plane(vidio_color_channel_interleaved, &in_stride[0]);
      break;
    default:
      assert(false);
      break;
  }


  vidio_frame* out_frame = new vidio_frame();
  out_frame->set_format(m_output_format, w, h);

  uint8_t* out_data[3];
  int out_stride[3];

  switch (m_output_format) {
    case vidio_pixel_format_RGB8:
      output_av_format = AV_PIX_FMT_RGB24;
      out_frame->add_raw_plane(vidio_color_channel_interleaved, w, h, 24);
      out_data[0] = out_frame->get_plane(vidio_color_channel_interleaved, &out_stride[0]);
      break;
    case vidio_pixel_format_YUV422_YUYV:
      output_av_format = AV_PIX_FMT_YUYV422;
      out_frame->add_raw_plane(vidio_color_channel_interleaved, w, h, 16);
      out_data[0] = out_frame->get_plane(vidio_color_channel_interleaved, &out_stride[0]);
      break;
    case vidio_pixel_format_RGGB8:
      output_av_format = AV_PIX_FMT_BAYER_RGGB8;
      out_frame->add_raw_plane(vidio_color_channel_interleaved, w, h, 8);
      out_data[0] = out_frame->get_plane(vidio_color_channel_interleaved, &out_stride[0]);
      break;
    default:
      assert(false);
      break;
  }

  // SWScale conversion

  if (m_swscaleContext == nullptr) {
    m_swscaleContext = sws_getContext(w, h, input_av_format,
                                      w, h, output_av_format,
                                      SWS_FAST_BILINEAR, NULL, NULL, NULL);
  }

  sws_scale(m_swscaleContext,
            in_data, in_stride,
            0, h,
            out_data, out_stride);

  out_frame->copy_metadata_from(in_frame);
  push_decoded_frame(out_frame);
}

