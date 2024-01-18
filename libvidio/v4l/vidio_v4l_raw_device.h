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

#ifndef LIBVIDIO_VIDIO_V4L_RAW_DEVICE_H
#define LIBVIDIO_VIDIO_V4L_RAW_DEVICE_H

#include "vidio_video_format_v4l.h"
#include <libvidio/vidio_error.h>
#include <mutex>


struct vidio_v4l_raw_device
{
public:
  ~vidio_v4l_raw_device();

  vidio_result<bool> query_device(const char* filename);

  std::string get_bus_info() const { return {(char*) &m_caps.bus_info[0]}; }

  std::string get_display_name() const { return {(char*) &m_caps.card[0]}; }

  const v4l2_capability& get_v4l_capabilities() const { return m_caps; }

  std::string get_device_file() const { return m_device_file; }

  bool has_video_capture_capability() const;

  std::vector<vidio_video_format_v4l*> get_video_formats() const;

  bool supports_pixel_format(__u32 pixelformat) const;

  const vidio_error* set_capture_format(const vidio_video_format_v4l* requested_format,
                                        const vidio_video_format_v4l** out_actual_format);

  const vidio_error* start_capturing_blocking(struct vidio_input_device_v4l*);

  const vidio_error* stop_capturing();

  const vidio_error* open();

  void close();

  bool is_open() const { return m_fd != -1; }

private:
  std::string m_device_file;
  int m_fd = -1; // < 0 if closed

  bool m_capturing_active = false;

  bool m_supports_framerate = false;
  struct v4l2_capability m_caps;

  struct framesize_v4l
  {
    v4l2_frmsizeenum m_framesize;
    std::vector<v4l2_frmivalenum> m_frameintervals;
  };

  struct format_v4l
  {
    v4l2_fmtdesc m_fmtdesc;
    std::vector<framesize_v4l> m_framesizes;
  };

  std::vector<format_v4l> m_formats;

  // type = V4L2_BUF_TYPE_VIDEO_CAPTURE or type = V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE
  vidio_result<std::vector<v4l2_fmtdesc>> list_v4l_formats(__u32 type) const;

  vidio_result<std::vector<v4l2_frmsizeenum>> list_v4l_framesizes(__u32 pixel_type) const;

  vidio_result<std::vector<v4l2_frmivalenum>> list_v4l_frameintervals(__u32 pixel_type, __u32 width, __u32 height) const;


  const vidio_video_format_v4l* m_capture_format = nullptr;  // this is a copy, it has to be freed

  __u32 m_capture_pixel_format;
  vidio_pixel_format m_capture_vidio_pixel_format;
  uint32_t m_capture_width;
  uint32_t m_capture_height;

  std::mutex m_mutex_loop_control;

  struct buffer
  {
    void* start;
    size_t length;
  };

  std::vector<buffer> m_buffers;
};

#endif //LIBVIDIO_VIDIO_V4L_RAW_DEVICE_H
