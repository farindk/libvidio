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

#include "libvidio/v4l/v4l.h"
#include <dirent.h>
#include <cstdio>
#include <cstring>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/ioctl.h>
#include <fcntl.h>
#include <unistd.h>
#include <cassert>


static vidio_pixel_format_class v4l_pixelformat_to_pixel_format_class(__u32 pixelformat)
{
  switch (pixelformat) {
    case V4L2_PIX_FMT_MJPEG:
      return vidio_pixel_format_class_MJPEG;
    case V4L2_PIX_FMT_H264:
    case V4L2_PIX_FMT_H264_MVC:
    case V4L2_PIX_FMT_H264_NO_SC:
    case V4L2_PIX_FMT_H264_SLICE:
      return vidio_pixel_format_class_H264;
    case V4L2_PIX_FMT_HEVC:
      return vidio_pixel_format_class_H265;
    case V4L2_PIX_FMT_YUYV:
      return vidio_pixel_format_class_YUV;
    default:
      return vidio_pixel_format_class_unknown;
  }
}


vidio_video_format_v4l::vidio_video_format_v4l(v4l2_fmtdesc fmt,
                                               uint32_t width, uint32_t height,
                                               vidio_fraction framerate)
{
  m_format = fmt;
  m_width = width;
  m_height = height;
  m_framerate = framerate;

  m_format_class = v4l_pixelformat_to_pixel_format_class(fmt.pixelformat);
}


bool vidio_v4l_raw_device::query_device(const char* filename)
{
  m_fd = open(filename, 0);
  if (m_fd == -1) {
    return false;
  }

  int ret;

  ret = ioctl(m_fd, VIDIOC_QUERYCAP, &m_caps);
  if (ret == -1) {
    close(m_fd);
    m_fd = -1;
    return false;
  }

  m_device_file = filename;

  if (has_video_capture_capability()) {

    auto formats = list_v4l_formats(V4L2_BUF_TYPE_VIDEO_CAPTURE);
    for (auto f: formats) {
      format_v4l format;
      format.m_fmtdesc = f;

      auto frmsizes = list_v4l_framesizes(f.pixelformat);
      for (auto s: frmsizes) {
        framesize_v4l fsize;
        fsize.m_framesize = s;

        std::vector<v4l2_frmivalenum> frmintervals;
        if (s.type == V4L2_FRMSIZE_TYPE_DISCRETE) {
          frmintervals = list_v4l_frameintervals(f.pixelformat, s.discrete.width, s.discrete.height);
        } else {
          frmintervals = list_v4l_frameintervals(f.pixelformat, s.stepwise.max_width, s.stepwise.max_height);
        }

        fsize.m_frameintervals = frmintervals;

        format.m_framesizes.emplace_back(fsize);
      }

      m_formats.emplace_back(format);
    }
  }

  close(m_fd);
  m_fd = -1;

  return true;
}


std::vector<v4l2_fmtdesc> vidio_v4l_raw_device::list_v4l_formats(__u32 type) const
{
  std::vector<v4l2_fmtdesc> fmts;

  assert(m_fd >= 0);

  v4l2_fmtdesc fmtdesc{};
  fmtdesc.type = type;
  for (fmtdesc.index = 0;; fmtdesc.index++) {
    int ret = ioctl(m_fd, VIDIOC_ENUM_FMT, &fmtdesc);
    if (ret < 0) {
      // usually: errno == EINVAL
      break;
    }

    fmts.emplace_back(fmtdesc);
  }

  return fmts;
}


std::vector<v4l2_frmsizeenum> vidio_v4l_raw_device::list_v4l_framesizes(__u32 pixel_type) const
{
  std::vector<v4l2_frmsizeenum> frmsizes;

  assert(m_fd >= 0);

  v4l2_frmsizeenum framesize{};
  framesize.pixel_format = pixel_type;
  for (framesize.index = 0;; framesize.index++) {
    int ret = ioctl(m_fd, VIDIOC_ENUM_FRAMESIZES, &framesize);
    if (ret < 0) {
      // usually: errno == EINVAL
      break;
    }

    frmsizes.emplace_back(framesize);
  }

  return frmsizes;
}


std::vector<v4l2_frmivalenum>
vidio_v4l_raw_device::list_v4l_frameintervals(__u32 pixel_type, __u32 width, __u32 height) const
{
  std::vector<v4l2_frmivalenum> frmivals;

  assert(m_fd >= 0);

  v4l2_frmivalenum frameinterval{};
  frameinterval.pixel_format = pixel_type;
  frameinterval.width = width;
  frameinterval.height = height;

  for (frameinterval.index = 0;; frameinterval.index++) {
    int ret = ioctl(m_fd, VIDIOC_ENUM_FRAMEINTERVALS, &frameinterval);
    if (ret < 0) {
      // usually: errno == EINVAL
      break;
    }

    frmivals.emplace_back(frameinterval);
  }

  return frmivals;
}


bool vidio_v4l_raw_device::has_video_capture_capability() const
{
  return (m_caps.device_caps & (V4L2_CAP_VIDEO_CAPTURE | V4L2_CAP_VIDEO_CAPTURE_MPLANE)) != 0;
}


std::vector<vidio_video_format_v4l*> vidio_v4l_raw_device::get_video_formats() const
{
  std::vector<vidio_video_format_v4l*> formats;

  for (const auto& f: m_formats) {

    for (const auto& r: f.m_framesizes) {
      uint32_t w, h;
      if (r.m_framesize.type == V4L2_FRMSIZE_TYPE_DISCRETE) {
        w = r.m_framesize.discrete.width;
        h = r.m_framesize.discrete.height;
      } else {
        w = r.m_framesize.stepwise.max_width;
        h = r.m_framesize.stepwise.max_height;
      }

      for (const auto& i: r.m_frameintervals) {
        // swap num/den because frame-interval is in seconds/frame

        vidio_fraction framerate;
        if (i.type == V4L2_FRMIVAL_TYPE_DISCRETE) {
          framerate.numerator = i.discrete.denominator;
          framerate.denominator = i.discrete.numerator;
        } else {
          framerate.numerator = i.stepwise.max.denominator;
          framerate.denominator = i.stepwise.max.numerator;
        }

        auto format = new vidio_video_format_v4l(f.m_fmtdesc, w, h, framerate);
        formats.push_back(format);
      }
    }
  }

  return formats;
}


bool vidio_input_device_v4l::matches_v4l_raw_device(const vidio_v4l_raw_device* device) const
{
  assert(!m_v4l_capture_devices.empty());

  if (device->get_bus_info() != m_v4l_capture_devices[0]->get_bus_info()) {
    return false;
  }

  return true;
}


std::vector<vidio_input_device_v4l*> v4l_list_input_devices(const struct vidio_input_device_filter* filter)
{
  std::vector<vidio_v4l_raw_device*> rawdevices;
  std::vector<vidio_input_device_v4l*> devices;

  DIR* d;
  struct dirent* dir;
  d = opendir("/dev");
  if (d) {
    while ((dir = readdir(d)) != nullptr) {
      if (strncmp(dir->d_name, "video", 5) == 0) {
        auto device = new vidio_v4l_raw_device();

        if (device->query_device((std::string{"/dev/"} + dir->d_name).c_str())) {
          if (device->has_video_capture_capability()) {
            rawdevices.push_back(device);
          }
        }

        // if we didn't add it to the list, delete it again
        if (rawdevices.empty() || rawdevices.back() != device) {
          delete device;
        }
      }
    }
    closedir(d);
  }


  // --- group v4l devices that operate on the same hardware

  for (auto dev: rawdevices) {
    // If there is an existing vidio device for the same hardware device, add the v4l device to this,
    // otherwise create a new vidio device.

    vidio_input_device_v4l* captureDevice = nullptr;
    for (auto cd: devices) {
      if (cd->matches_v4l_raw_device(dev)) {
        captureDevice = cd;
        break;
      }
    }

    if (captureDevice) {
      captureDevice->add_v4l_raw_device(dev);
    } else {
      captureDevice = new vidio_input_device_v4l(dev);
      devices.emplace_back(captureDevice);
    }
  }

  return devices;
}


std::vector<vidio_video_format*> vidio_input_device_v4l::get_video_formats() const
{
  std::vector<vidio_video_format*> formats;

  for (auto dev: m_v4l_capture_devices) {
    auto f = dev->get_video_formats();
    formats.insert(formats.end(), f.begin(), f.end());
  }

  return formats;
}
