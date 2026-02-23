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

#ifndef LIBVIDIO_VIDIO_FILE_READER_H
#define LIBVIDIO_VIDIO_FILE_READER_H

#include <libvidio/vidio.h>
#include <libvidio/vidio_error.h>
#include <string>
#include <atomic>

extern "C" {
#include <libavformat/avformat.h>
#include <libavcodec/avcodec.h>
#include <libavcodec/bsf.h>
#include <libswscale/swscale.h>
}

struct vidio_frame;
struct vidio_input_file;


class vidio_file_reader
{
public:
  vidio_file_reader();
  ~vidio_file_reader();

  const vidio_error* open(const std::string& filepath);
  void close();

  bool is_open() const { return m_av_format_context != nullptr; }

  int get_width() const { return m_width; }
  int get_height() const { return m_height; }
  vidio_fraction get_framerate() const { return m_framerate; }
  vidio_pixel_format get_pixel_format() const { return m_pixel_format; }

  // Returns true if codec is H264/H265/MJPEG (compressed pass-through).
  // Otherwise frames are decoded internally and delivered as raw pixels.
  bool is_compressed_passthrough() const { return m_compressed_passthrough; }

  // Read next frame. Returns nullptr on EOF. Caller owns the returned frame.
  vidio_frame* read_next_frame();

  // Seek to the beginning of the file for looping.
  bool seek_to_beginning();

  void stop() { m_stop = true; }

  void resume() { m_stop = false; }

private:
  AVFormatContext* m_av_format_context = nullptr;
  int m_video_stream_index = -1;

  // Decoder state (only used for non-passthrough codecs)
  AVCodecContext* m_codec_context = nullptr;
  SwsContext* m_sws_context = nullptr;

  int m_width = 0;
  int m_height = 0;
  vidio_fraction m_framerate{0, 1};
  vidio_pixel_format m_pixel_format = vidio_pixel_format_undefined;
  bool m_compressed_passthrough = false;

  AVBSFContext* m_bsf_context = nullptr;

  std::atomic<bool> m_stop{false};

  static bool is_passthrough_codec(AVCodecID codec_id);
  vidio_pixel_format codec_id_to_pixel_format(AVCodecID codec_id) const;
  vidio_frame* create_compressed_frame(AVPacket* pkt);
  vidio_frame* decode_frame(AVPacket* pkt);
  vidio_frame* flush_decoder();
};

#endif //LIBVIDIO_VIDIO_FILE_READER_H
