/*
 * VidIO library
 * Copyright (c) 2023 Dirk Farin <dirk.farin@gmail.com>
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

#ifndef LIBVIDIO_V4L_H
#define LIBVIDIO_V4L_H

#include <linux/videodev2.h>
#include <string>
#include <vector>
#include <memory>
#include <libvidio/vidio.h>
#include <libvidio/vidio_input.h>
#include <libvidio/vidio_video_format.h>


struct vidio_video_format_v4l : public vidio_video_format
{
public:
  vidio_video_format_v4l(v4l2_fmtdesc fmt,
                         uint32_t width, uint32_t height,
                         vidio_fraction framerate);

  uint32_t get_width() const override { return m_width; }

  uint32_t get_height() const override { return m_height; }

  vidio_fraction get_framerate() const override { return m_framerate; }

  std::string get_user_description() const override { return {(const char*) m_format.description}; }

  vidio_pixel_format_class get_pixel_format_class() const override { return m_format_class; }

  __u32 get_pixel_format() const { return m_format.pixelformat; }

private:
  v4l2_fmtdesc m_format;
  uint32_t m_width, m_height;
  vidio_fraction m_framerate;

  vidio_pixel_format_class m_format_class;
};


struct vidio_v4l_raw_device
{
public:
  bool query_device(const char* filename);

  std::string get_bus_info() const { return {(char*) &m_caps.bus_info[0]}; }

  std::string get_display_name() const { return {(char*) &m_caps.card[0]}; }

  const v4l2_capability& get_v4l_capabilities() const { return m_caps; }

  bool has_video_capture_capability() const;

  std::vector<vidio_video_format_v4l*> get_video_formats() const;

  bool supports_pixel_format(__u32 pixelformat) const;

  vidio_error* set_capture_format(const vidio_video_format_v4l* requested_format,
                                  vidio_video_format_v4l** out_actual_format);

  vidio_error* start_capturing_blocking(void (*callback)(const vidio_frame*));

  vidio_error* open();

  void close();

  bool is_open() const { return m_fd != -1; }

private:
  std::string m_device_file;
  int m_fd = -1; // < 0 if closed

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
  std::vector<v4l2_fmtdesc> list_v4l_formats(__u32 type) const;

  std::vector<v4l2_frmsizeenum> list_v4l_framesizes(__u32 pixel_type) const;

  std::vector<v4l2_frmivalenum> list_v4l_frameintervals(__u32 pixel_type, __u32 width, __u32 height) const;


  struct buffer {
    void   *start;
    size_t  length;
  };

  std::vector<buffer> m_buffers;
};


struct vidio_input_device_v4l : public vidio_input_device
{
public:
  explicit vidio_input_device_v4l(vidio_v4l_raw_device* d) { m_v4l_capture_devices.emplace_back(d); }

  std::string get_display_name() const override { return m_v4l_capture_devices[0]->get_display_name(); }

  vidio_input_source get_source() const override { return vidio_input_source_Video4Linux2; }

  bool matches_v4l_raw_device(const vidio_v4l_raw_device*) const;

  void add_v4l_raw_device(vidio_v4l_raw_device* dev)
  {
    m_v4l_capture_devices.emplace_back(dev);
  }

  std::vector<vidio_video_format*> get_video_formats() const override;

  vidio_error* set_capture_format(const vidio_video_format* requested_format,
                                  vidio_video_format** out_actual_format) override;

  vidio_error* start_capturing_blocking(void (*callback)(const vidio_frame*)) override;

private:
  std::vector<vidio_v4l_raw_device*> m_v4l_capture_devices;

  vidio_v4l_raw_device* m_active_device = nullptr;
};


std::vector<vidio_input_device_v4l*> v4l_list_input_devices(const struct vidio_input_device_filter* filter);

#endif //LIBVIDIO_V4L_H
