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


vidio_format_converter_ffmpeg::~vidio_format_converter_ffmpeg()
{
  avcodec_free_context(&m_context);
  av_frame_free(&m_decodedFrame);
  //av_packet_free(&m_pkt);
}


vidio_error* vidio_format_converter_ffmpeg::init(enum AVCodecID codecId)
{
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
  res = avcodec_receive_frame(m_context, m_decodedFrame);

  av_packet_free(&pkt);

  // convert to vidio_frame

  int w = input->get_width();
  int h = input->get_height();

  vidio_frame* out_frame = new vidio_frame();
  out_frame->set_format(vidio_pixel_format_RGB8, w, h);
  out_frame->add_raw_plane(vidio_color_channel_interleaved, w, h, 24);

  uint8_t* out;
  int out_stride;
  out = out_frame->get_plane(vidio_color_channel_interleaved, &out_stride);

  for (int y = 0; y < h; y++)
    for (int x = 0; x < w; x++) {
      int yy = m_decodedFrame->data[0][y * m_decodedFrame->linesize[0] + x] - 16;
      int u = m_decodedFrame->data[1][y * m_decodedFrame->linesize[1] + x / 2] - 128;
      int v = m_decodedFrame->data[2][y * m_decodedFrame->linesize[2] + x / 2] - 128;

      out[y * out_stride + 3 * x + 0] = clip8(1.164 * yy + 1.1596 * v);
      out[y * out_stride + 3 * x + 1] = clip8(1.164 * yy - 0.392 * u - 0.813 * v);
      out[y * out_stride + 3 * x + 2] = clip8(1.164 * yy + 2.017 * u);
    }

  push_decoded_frame(out_frame);
}
