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

#include "mjpeg.h"
#include "common.h"
#include "libvidio/vidio_frame.h"
#include "libvidio/third-party/jpeg_decoder.h"
#include <cassert>

extern "C"
{
#include <libavcodec/avcodec.h>
}


vidio_frame* mjpeg_to_rgb8_small(const vidio_frame* input)
{
  int w = input->get_width();
  int h = input->get_height();

  vidio_frame* out_frame = new vidio_frame();
  out_frame->set_format(vidio_pixel_format_RGB8, w, h);
  out_frame->add_raw_plane(vidio_color_channel_interleaved, w, h, 24);

  const uint8_t* in;
  int in_stride;
  in = input->get_plane(vidio_color_channel_compressed, &in_stride);

  uint8_t* out;
  int out_stride;
  out = out_frame->get_plane(vidio_color_channel_interleaved, &out_stride);

  Jpeg::Decoder decoder(in, in_stride);
  auto result = decoder.GetResult();
  if (result == Jpeg::Decoder::DecodeResult::OK) {
    if (decoder.IsColor()) {
      const uint8_t* imgData = decoder.GetImage();
      int inWidth = decoder.GetWidth();

      for (int y = 0; y < h; y++) {
        memcpy(out + y * out_stride,
               imgData + y * inWidth * 3,
               inWidth * 3);
      }
    }
    else {
      assert(false);
    }
  }

  return out_frame;
}


vidio_frame* mjpeg_to_rgb8_ffmpeg(const vidio_frame* input)
{
  int w = input->get_width();
  int h = input->get_height();

  vidio_frame* out_frame = new vidio_frame();
  out_frame->set_format(vidio_pixel_format_RGB8, w, h);
  out_frame->add_raw_plane(vidio_color_channel_interleaved, w, h, 24);

  const uint8_t* in;
  int in_stride;
  in = input->get_plane(vidio_color_channel_compressed, &in_stride);

  uint8_t* out;
  int out_stride;
  out = out_frame->get_plane(vidio_color_channel_interleaved, &out_stride);


  // AVCodec

  const AVCodec* codec = avcodec_find_decoder(AV_CODEC_ID_MJPEG);
  if (!codec) {
    return nullptr;
  }

  // AVContext

  AVCodecContext* codecCtx = avcodec_alloc_context3(codec);
  if (!codecCtx) {
    return nullptr; //Error
  }

  if (avcodec_open2(codecCtx, codec, nullptr) < 0) {
    avcodec_free_context(&codecCtx);
    return nullptr;
  }

  // AVPacket

  AVPacket* pkt = av_packet_alloc();
  if (!pkt) {
    avcodec_free_context(&codecCtx);
    return nullptr;
  }

  int res = av_new_packet(pkt, in_stride);
  if (res != 0) {
    return nullptr;
  }

  memcpy(pkt->data, in, in_stride);

  // AVFrame

  AVFrame* decodedFrame = av_frame_alloc();
  if (!decodedFrame) {
    avcodec_free_context(&codecCtx);
    return nullptr;
  }

  // decode frame

  res = avcodec_send_packet(codecCtx, pkt);
  res = avcodec_receive_frame(codecCtx, decodedFrame);

  av_packet_free(&pkt);

  // convert to vidio_frame

  for (int y = 0; y < h; y++)
    for (int x = 0; x < w; x++) {
      int yy = decodedFrame->data[0][y * decodedFrame->linesize[0] + x] - 16;
      int u = decodedFrame->data[1][y * decodedFrame->linesize[1] + x / 2] - 128;
      int v = decodedFrame->data[2][y * decodedFrame->linesize[2] + x / 2] - 128;

      out[y * out_stride + 3 * x + 0] = clip8(1.164 * yy + 1.1596 * v);
      out[y * out_stride + 3 * x + 1] = clip8(1.164 * yy - 0.392 * u - 0.813 * v);
      out[y * out_stride + 3 * x + 2] = clip8(1.164 * yy + 2.017 * u);
    }

  return out_frame;
}
