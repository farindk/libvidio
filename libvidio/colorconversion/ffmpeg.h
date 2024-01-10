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

#ifndef LIBVIDIO_FFMPEG_H
#define LIBVIDIO_FFMPEG_H

#include "libvidio/vidio_format_converter.h"

extern "C"
{
#include <libavcodec/avcodec.h>
}

struct vidio_format_converter_ffmpeg : public vidio_format_converter
{
public:
  vidio_error* init(enum AVCodecID);

  ~vidio_format_converter_ffmpeg() override;

  void push(const vidio_frame* in) override;

private:
  const AVCodec* m_codec = nullptr;
  AVCodecContext* m_context = nullptr;
  //AVPacket* m_pkt = nullptr;
  AVFrame* m_decodedFrame = nullptr;
};


#endif //LIBVIDIO_FFMPEG_H
