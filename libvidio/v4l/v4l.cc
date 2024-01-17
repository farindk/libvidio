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
#include "libvidio/vidio_frame.h"
#include "libvidio/util/key_value_store.h"
#include <dirent.h>
#include <cstdio>
#include <cstring>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/ioctl.h>
#include <fcntl.h>
#include <unistd.h>
#include <cassert>
#include <algorithm>
#include <sys/mman.h>
#include "libvidio/vidio_error.h"


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
    case V4L2_PIX_FMT_SRGGB8:
      return vidio_pixel_format_class_RGB;
    default:
      return vidio_pixel_format_class_unknown;
  }
}


static vidio_pixel_format v4l_pixelformat_to_pixel_format(__u32 pixelformat)
{
  switch (pixelformat) {
    case V4L2_PIX_FMT_MJPEG:
      return vidio_pixel_format_MJPEG;
    case V4L2_PIX_FMT_H264:
    case V4L2_PIX_FMT_H264_MVC:
    case V4L2_PIX_FMT_H264_NO_SC:
    case V4L2_PIX_FMT_H264_SLICE:
      return vidio_pixel_format_H264;
    case V4L2_PIX_FMT_HEVC:
      return vidio_pixel_format_H265;
    case V4L2_PIX_FMT_YUYV:
      return vidio_pixel_format_YUV422_YUYV;
    case V4L2_PIX_FMT_SRGGB8:
      return vidio_pixel_format_RGB8;
    default:
      return vidio_pixel_format_undefined;
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

#if WITH_JSON

vidio_video_format_v4l::vidio_video_format_v4l(const nlohmann::json& json)
{
  // v4l2_fmtdesc

  m_format.type = json["format_type"]; // V4L2_BUF_TYPE_VIDEO_CAPTURE or V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE (TODO: in the future)
  m_format.flags = json["format_flags"];
  m_format.pixelformat = json["format_pixelformat"];
  m_format.mbus_code = json["format_mbus_code"];

  std::string descr = json["format_description"];
  strncpy((char*) m_format.description, descr.c_str(), 32);
  m_format.description[31] = 0;

  // resolution / frame-rate

  m_width = json["width"];
  m_height = json["height"];
  m_framerate.numerator = json["framerate_numerator"];
  m_framerate.denominator = json["framerate_denominator"];
}

#endif

std::string vidio_video_format_v4l::serialize(vidio_serialization_format serialformat) const
{
  if (serialformat == vidio_serialization_format_json) {
#if WITH_JSON
    nlohmann::json json{
        {"class",                 "v4l2"},
        {"format_type",           m_format.type},
        {"format_flags",          m_format.flags},
        {"format_pixelformat",    m_format.pixelformat},
        {"format_mbus_code",      m_format.mbus_code},
        {"format_description",    (const char*) (m_format.description)},
        {"width",                 m_width},
        {"height",                m_height},
        {"framerate_numerator",   m_framerate.numerator},
        {"framerate_denominator", m_framerate.denominator}
    };

    return json.dump();
#endif
  }

  if (serialformat == vidio_serialization_format_keyvalue) {
    key_value_store store;
    // TODO
  }

  return {};
}


vidio_pixel_format vidio_video_format_v4l::get_pixel_format() const
{
  return v4l_pixelformat_to_pixel_format(m_format.pixelformat);;
}


int vidio_video_format_v4l::format_match_score(const vidio_video_format* f) const
{
  auto* format = dynamic_cast<const vidio_video_format*>(f);
  if (format == nullptr) {
    return generic_format_match_score(f);
  }

  // actually, there is no special handling for this format type (yet)

  return generic_format_match_score(f);
}


vidio_result<bool> vidio_v4l_raw_device::query_device(const char* filename)
{
  m_fd = ::open(filename, O_RDWR);
  if (m_fd == -1) {
    if (errno == ENOENT) {
      return false;
    }

    auto* err = new vidio_error(vidio_error_code_cannot_open_camera, "Cannot open camera ({0})");
    err->set_arg(0, filename);
    err->set_reason(vidio_error::from_errno());
    return err;
  }

  int ret;

  ret = ioctl(m_fd, VIDIOC_QUERYCAP, &m_caps);
  if (ret == -1) {
    ::close(m_fd);
    m_fd = -1;

    auto* err = new vidio_error(vidio_error_code_cannot_query_device_capabilities, "Cannot query V4L2 device capabilities (VIDIOC_QUERYCAP) ({0})");
    err->set_arg(0, filename);
    err->set_reason(vidio_error::from_errno());
    return err;
  }

  m_device_file = filename;

  if (has_video_capture_capability()) {

    // --- check whether driver supports time/frame

    v4l2_streamparm streamparm{};
    streamparm.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    ret = ioctl(m_fd, VIDIOC_G_PARM, &streamparm);
    if (ret == -1) {
      ::close(m_fd);
      m_fd = -1;

      auto* err = new vidio_error(vidio_error_code_cannot_query_device_capabilities, "Cannot query V4L2 device parameters (VIDIOC_G_PARM) ({0})");
      err->set_arg(0, filename);
      err->set_reason(vidio_error::from_errno());
      return err;
    }
    m_supports_framerate = !!(streamparm.parm.capture.capability & V4L2_CAP_TIMEPERFRAME);

    // --- list supported formats

    auto formatsResult = list_v4l_formats(V4L2_BUF_TYPE_VIDEO_CAPTURE);
    if (formatsResult.error) {
      return formatsResult.error;
    }

    const auto& formats = formatsResult.value;
    for (auto f : formats) {
      format_v4l format;
      format.m_fmtdesc = f;

      auto frmsizesResult = list_v4l_framesizes(f.pixelformat);
      if (frmsizesResult.error) {
        return frmsizesResult.error;
      }

      const auto& frmsizes = frmsizesResult.value;
      for (auto s : frmsizes) {
        framesize_v4l fsize;
        fsize.m_framesize = s;

        if (m_supports_framerate) {
          vidio_result<std::vector<v4l2_frmivalenum>> frmintervalsResult;

          if (s.type == V4L2_FRMSIZE_TYPE_DISCRETE) {
            frmintervalsResult = list_v4l_frameintervals(f.pixelformat, s.discrete.width, s.discrete.height);
          }
          else {
            frmintervalsResult = list_v4l_frameintervals(f.pixelformat, s.stepwise.max_width, s.stepwise.max_height);
          }

          if (frmintervalsResult.error) {
            return frmintervalsResult.error;
          }

          fsize.m_frameintervals = frmintervalsResult.value;
        }

        format.m_framesizes.emplace_back(fsize);
      }

      m_formats.emplace_back(format);
    }
  }

  ::close(m_fd);
  m_fd = -1;

  return true;
}


vidio_result<std::vector<v4l2_fmtdesc>> vidio_v4l_raw_device::list_v4l_formats(__u32 type) const
{
  std::vector<v4l2_fmtdesc> fmts;

  assert(m_fd >= 0);

  v4l2_fmtdesc fmtdesc{};
  fmtdesc.type = type;
  for (fmtdesc.index = 0;; fmtdesc.index++) {
    int ret = ioctl(m_fd, VIDIOC_ENUM_FMT, &fmtdesc);
    if (ret < 0) {
      if (errno == EINVAL) {
        // we reached the end of the enumeration
        break;
      }

      auto* err = new vidio_error(vidio_error_code_cannot_query_device_capabilities, "Cannot query V4L2 device formats (VIDIOC_ENUM_FMT)");
      err->set_reason(vidio_error::from_errno());
      return err;
    }

    fmts.emplace_back(fmtdesc);
  }

  return fmts;
}


vidio_result<std::vector<v4l2_frmsizeenum>> vidio_v4l_raw_device::list_v4l_framesizes(__u32 pixel_type) const
{
  std::vector<v4l2_frmsizeenum> frmsizes;

  assert(m_fd >= 0);

  v4l2_frmsizeenum framesize{};
  framesize.pixel_format = pixel_type;
  for (framesize.index = 0;; framesize.index++) {
    int ret = ioctl(m_fd, VIDIOC_ENUM_FRAMESIZES, &framesize);
    if (ret < 0) {
      if (errno == EINVAL) {
        // we reached the end of the enumeration
        break;
      }

      auto* err = new vidio_error(vidio_error_code_cannot_query_device_capabilities, "Cannot query V4L2 device frame sizes (VIDIOC_ENUM_FRAMESIZES)");
      err->set_reason(vidio_error::from_errno());
      return err;
    }

    // check if there is a duplicate framesize (my SVPro camera lists one size twice)

    bool duplicate_found = false;
    for (auto& frmsize : frmsizes) {
      if (frmsize.type == framesize.type) {
        if (frmsize.type == V4L2_FRMSIZE_TYPE_DISCRETE &&
            frmsize.discrete.width == framesize.discrete.width &&
            frmsize.discrete.height == framesize.discrete.height) {
          duplicate_found = true;
          break;
        }
        else if (frmsize.stepwise.min_width == framesize.stepwise.min_width &&
                 frmsize.stepwise.max_width == framesize.stepwise.max_width &&
                 frmsize.stepwise.min_height == framesize.stepwise.min_height &&
                 frmsize.stepwise.max_height == framesize.stepwise.max_height &&
                 frmsize.stepwise.step_width == framesize.stepwise.step_width &&
                 frmsize.stepwise.step_height == framesize.stepwise.step_height) {
          duplicate_found = true;
          break;
        }
      }
    }

    if (!duplicate_found) {
      frmsizes.emplace_back(framesize);
    }
  }

  return frmsizes;
}


vidio_result<std::vector<v4l2_frmivalenum>>
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
      if (errno == EINVAL) {
        // we reached the end of the enumeration
        break;
      }

      auto* err = new vidio_error(vidio_error_code_cannot_query_device_capabilities, "Cannot query V4L2 frame intervals (VIDIOC_ENUM_FRAMEINTERVALS)");
      err->set_reason(vidio_error::from_errno());
      return err;
    }


    // check if there is a duplicate frame-interval (my SVPro camera lists some twice)

    bool duplicate_found = false;
    for (const auto& f0 : frmivals) {
      const auto& f1 = frameinterval;

      if (f0.type == f1.type) {
        if (f0.type == V4L2_FRMIVAL_TYPE_DISCRETE &&
            f0.discrete.numerator == f1.discrete.numerator &&
            f0.discrete.denominator == f1.discrete.denominator) {
          duplicate_found = true;
          break;
        }
        else if (f0.stepwise.min.numerator == f1.stepwise.min.numerator &&
                 f0.stepwise.min.denominator == f1.stepwise.min.denominator &&
                 f0.stepwise.max.numerator == f1.stepwise.max.numerator &&
                 f0.stepwise.max.denominator == f1.stepwise.max.denominator &&
                 f0.stepwise.step.numerator == f1.stepwise.step.numerator &&
                 f0.stepwise.step.denominator == f1.stepwise.step.denominator) {
          duplicate_found = true;
          break;
        }
      }
    }

    if (!duplicate_found) {
      frmivals.emplace_back(frameinterval);
    }
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

  for (const auto& f : m_formats) {

    for (const auto& r : f.m_framesizes) {
      uint32_t w, h;
      if (r.m_framesize.type == V4L2_FRMSIZE_TYPE_DISCRETE) {
        w = r.m_framesize.discrete.width;
        h = r.m_framesize.discrete.height;
      }
      else {
        w = r.m_framesize.stepwise.max_width;
        h = r.m_framesize.stepwise.max_height;
      }

      for (const auto& i : r.m_frameintervals) {
        // swap num/den because frame-interval is in seconds/frame

        vidio_fraction framerate;
        if (i.type == V4L2_FRMIVAL_TYPE_DISCRETE) {
          framerate.numerator = i.discrete.denominator;
          framerate.denominator = i.discrete.numerator;
        }
        else {
          framerate.numerator = i.stepwise.max.denominator;
          framerate.denominator = i.stepwise.max.numerator;
        }

        auto format = new vidio_video_format_v4l(f.m_fmtdesc, w, h, framerate);
        formats.push_back(format);
      }

      // There seem to be some devices that do not report a framerate.
      if (r.m_frameintervals.empty()) {
        auto format = new vidio_video_format_v4l(f.m_fmtdesc, w, h, {0, 1});
        formats.push_back(format);
      }
    }
  }

  return formats;
}


bool vidio_v4l_raw_device::supports_pixel_format(__u32 pixelformat) const
{
  return std::any_of(m_formats.begin(), m_formats.end(),
                     [pixelformat](const auto& f) {
                       return f.m_fmtdesc.pixelformat == pixelformat;
                     });
}


static vidio_pixel_format v4l2_pixelformat_to_vidio_format(__u32 v4l_format)
{
  switch (v4l_format) {
    case V4L2_PIX_FMT_YUYV:
      return vidio_pixel_format_YUV422_YUYV;
    case V4L2_PIX_FMT_MJPEG:
      return vidio_pixel_format_MJPEG;
    case V4L2_PIX_FMT_H264:
      return vidio_pixel_format_H264;

    default:
      return vidio_pixel_format_undefined;
  }
}

const vidio_error* vidio_v4l_raw_device::set_capture_format(const vidio_video_format_v4l* format_v4l,
                                                            const vidio_video_format_v4l** out_format)
{
  if (!is_open()) {
    auto* err = open();
    if (err) {
      return err;
    }
  }

  assert(m_fd >= 0);

  int ret;

  v4l2_format fmt{};
  fmt.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
  fmt.fmt.pix.width = format_v4l->get_width();
  fmt.fmt.pix.height = format_v4l->get_height();
  fmt.fmt.pix.pixelformat = format_v4l->get_v4l2_pixel_format();
  fmt.fmt.pix.field = V4L2_FIELD_ANY;

  ret = ioctl(m_fd, VIDIOC_S_FMT, &fmt);
  if (ret == -1) {
    auto* err = new vidio_error(vidio_error_code_cannot_set_camera_format, "Cannot set camera format (VIDIOC_S_FMT)");
    err->set_reason(vidio_error::from_errno());
    return err;
  }


  // TODO: can we assume that we got the requested format, or do we have to check what we really got?
  m_capture_width = format_v4l->get_width();
  m_capture_height = format_v4l->get_height();
  m_capture_pixel_format = format_v4l->get_v4l2_pixel_format();
  m_capture_vidio_pixel_format = v4l2_pixelformat_to_vidio_format(m_capture_pixel_format);


  // --- set framerate

  if (m_supports_framerate) {
    v4l2_streamparm param{};
    param.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    param.parm.capture.timeperframe.numerator = format_v4l->get_framerate().denominator;
    param.parm.capture.timeperframe.denominator = format_v4l->get_framerate().numerator;

    ret = ioctl(m_fd, VIDIOC_S_PARM, &param);
    if (ret == -1) {
      auto* err = new vidio_error(vidio_error_code_cannot_set_camera_format, "Cannot set camera format (VIDIOC_S_PARAM)");
      err->set_reason(vidio_error::from_errno());
      return err;
    }
  }

  return nullptr;
}


static std::string fourcc_to_string(uint32_t cc)
{
  std::string str("0000");
  str[0] = static_cast<char>(cc & 0xFF);
  str[1] = static_cast<char>((cc >> 8) & 0xFF);
  str[2] = static_cast<char>((cc >> 16) & 0xFF);
  str[3] = static_cast<char>((cc >> 24) & 0xFF);
  return str;
}


const vidio_error* vidio_v4l_raw_device::start_capturing_blocking(vidio_input_device_v4l* input_device)
{
  assert(m_fd != -1);
  assert(input_device != nullptr);

  // --- request buffers

  v4l2_requestbuffers req{};

  req.count = 4;
  req.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
  req.memory = V4L2_MEMORY_MMAP;

  int ret;

  if (-1 == (ret = ioctl(m_fd, VIDIOC_REQBUFS, &req))) {
    auto* err = new vidio_error(vidio_error_code_cannot_alloc_capturing_buffers, "Cannot get capturing buffers (VIDIOC_REQBUFS)");
    err->set_reason(vidio_error::from_errno());
    return err;
  }

  if (req.count <= 1) {
    auto* err = new vidio_error(vidio_error_code_cannot_alloc_capturing_buffers, "Cannot get enough capturing buffers (VIDIOC_REQBUFS count={0})");
    err->set_arg(0, std::to_string(req.count));
    err->set_reason(vidio_error::from_errno());
    return err;
  }

  m_buffers.resize(req.count);

  for (__u32 i = 0; i < req.count; i++) {
    v4l2_buffer buf{};

    buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    buf.memory = V4L2_MEMORY_MMAP;
    buf.index = i;

    if (-1 == ioctl(m_fd, VIDIOC_QUERYBUF, &buf)) {
      auto* err = new vidio_error(vidio_error_code_cannot_alloc_capturing_buffers, "Cannot query capturing buffers (VIDIOC_QUERYBUF index={0})");
      err->set_arg(0, std::to_string(req.count));
      err->set_reason(vidio_error::from_errno());
      return err;
    }

    m_buffers[i].length = buf.length;
    m_buffers[i].start =
        mmap(nullptr /* start anywhere */,
             buf.length,
             PROT_READ | PROT_WRITE /* required */,
             MAP_SHARED /* recommended */,
             m_fd, buf.m.offset);

    if (MAP_FAILED == m_buffers[i].start) {
      auto* err = new vidio_error(vidio_error_code_cannot_alloc_capturing_buffers, "Cannot map capturing buffer memory (mmap index={0})");
      err->set_arg(0, std::to_string(req.count));
      err->set_reason(vidio_error::from_errno());
      return err;
    }
  }

  // --- queue all buffers

  for (size_t i = 0; i < m_buffers.size(); i++) {
    v4l2_buffer buf{};

    buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    buf.memory = V4L2_MEMORY_MMAP;
    buf.index = (__u32) i;

    if (-1 == ioctl(m_fd, VIDIOC_QBUF, &buf)) {
      auto* err = new vidio_error(vidio_error_code_cannot_alloc_capturing_buffers, "Cannot queue buffer (VIDIOC_QBUF index={0})");
      err->set_arg(0, std::to_string(req.count));
      err->set_reason(vidio_error::from_errno());
      return err;
    }
  }

  // --- switch on streaming

  v4l2_buf_type type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
  if (-1 == ioctl(m_fd, VIDIOC_STREAMON, &type)) {
    auto* err = new vidio_error(vidio_error_code_cannot_start_capturing, "Cannot start capturing (VIDIOC_STREAMON)");
    err->set_reason(vidio_error::from_errno());
    return err;
  }

  m_capturing_active = true;

  while (true || m_capturing_active) {
    fd_set fds;
    FD_ZERO(&fds);
    FD_SET(m_fd, &fds);

    /* Timeout. */
    struct timeval tv{};
    tv.tv_sec = 2;
    tv.tv_usec = 0;

    if (m_capturing_active) {
      int r = select(m_fd + 1, &fds, nullptr, nullptr, &tv);
      if (r == -1) {
        if (errno == EINTR)
          continue;
        else {
          auto* err = new vidio_error(vidio_error_code_error_while_capturing, "Error while waiting for next frame");
          err->set_reason(vidio_error::from_errno());
          return err;
        }
      }
    }

    // get frame

    v4l2_buffer buf{};

    m_mutex_loop_control.lock();

    if (m_capturing_active) {
      buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
      buf.memory = V4L2_MEMORY_MMAP;

      ret = ioctl(m_fd, VIDIOC_DQBUF, &buf);
      m_mutex_loop_control.unlock();

      if (ret == -1) {
        auto* err = new vidio_error(vidio_error_code_error_while_capturing, "Cannot unqueue buffer (VIDIOC_DQBUF index={0})");
        err->set_arg(0, std::to_string(req.count));
        err->set_reason(vidio_error::from_errno());
        return err;
      }
    }
    else {
      m_mutex_loop_control.unlock();
      break;
    }

    const buffer& buffer = m_buffers[buf.index];

    // TODO: alternatively to copying the data into the vidio_frame, we could also offer a mode in which
    //  the vidio_frame only wraps the v4l2 buffer and this is passed directly to the client.
    //  The client code must be very quick, but it would eliminate one copy.
    //  Easiest implementation would be when the "add_plane()" functions had a mode switch (copy or wrap).

    auto* frame = new vidio_frame();
    switch (m_capture_pixel_format) {
      case V4L2_PIX_FMT_YUYV:
        frame->set_format(vidio_pixel_format_YUV422_YUYV, m_capture_width, m_capture_height);
        frame->add_raw_plane(vidio_color_channel_interleaved, 16);
        frame->copy_raw_plane(vidio_color_channel_interleaved, buffer.start, buf.bytesused);
        break;
      case V4L2_PIX_FMT_MJPEG:
        frame->set_format(vidio_pixel_format_MJPEG, m_capture_width, m_capture_height);
        frame->add_compressed_plane(vidio_color_channel_compressed, vidio_channel_format_compressed_MJPEG, 8,
                                    (const uint8_t*) buffer.start, buf.bytesused,
                                    m_capture_width, m_capture_height);
        break;
      case V4L2_PIX_FMT_H264:
      case V4L2_PIX_FMT_H264_MVC:
      case V4L2_PIX_FMT_H264_NO_SC:
      case V4L2_PIX_FMT_H264_SLICE:
        frame->set_format(vidio_pixel_format_H264, m_capture_width, m_capture_height);
        frame->add_compressed_plane(vidio_color_channel_compressed, vidio_channel_format_compressed_MJPEG, 8,
                                    (const uint8_t*) buffer.start, buf.bytesused,
                                    m_capture_width, m_capture_height);
        break;
      default: {
        auto* err = new vidio_error(vidio_error_code_internal_error, "Unsupported V4L2 pixel format ({0})");
        err->set_arg(0, fourcc_to_string(m_capture_pixel_format));
        return err;
        break;
      }
    }

    uint64_t timestamp = buf.timestamp.tv_sec * 1000000 + buf.timestamp.tv_usec;
    frame->set_timestamp_us(timestamp);

    input_device->push_frame_into_queue(frame);

    // --- re-queue buffer

    if (-1 == ioctl(m_fd, VIDIOC_QBUF, &buf)) {
      auto* err = new vidio_error(vidio_error_code_error_while_capturing, "Cannot queue buffer (VIDIOC_QBUF index={0})");
      err->set_arg(0, std::to_string(req.count));
      err->set_reason(vidio_error::from_errno());
      return err;
    }
  }

  // release capturing buffers

  for (auto& buffer : m_buffers) {
    if (munmap(buffer.start, buffer.length) == -1) {
      auto* err = new vidio_error(vidio_error_code_cannot_free_capturing_buffers, "Cannot unmap buffer (munmap)");
      err->set_reason(vidio_error::from_errno());
      return err;
    }
  }

  m_buffers.clear();

  return nullptr;
}


const vidio_error* vidio_v4l_raw_device::stop_capturing()
{
  if (m_capturing_active) {
    std::unique_lock<std::mutex> lock(m_mutex_loop_control);

    m_capturing_active = false;

    v4l2_buf_type type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    if (-1 == ioctl(m_fd, VIDIOC_STREAMOFF, &type)) {
      auto* err = new vidio_error(vidio_error_code_cannot_stop_capturing, "Cannot stop capturing (V4L2 STREAMOFF)");
      err->set_reason(vidio_error::from_errno());
      return err;
    }


    // release buffers (otherwise, S_FMT would return EBUSY

    v4l2_requestbuffers req{};
    req.count = 0;
    req.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    req.memory = V4L2_MEMORY_MMAP;

    if (-1 == ioctl(m_fd, VIDIOC_REQBUFS, &req)) {
      auto* err = new vidio_error(vidio_error_code_cannot_free_capturing_buffers, "Cannot free capturing buffers (V4L2 set REQBUFS count to 0)");
      err->set_reason(vidio_error::from_errno());
      return err;
    }
  }

  return nullptr;
}


const vidio_error* vidio_v4l_raw_device::open()
{
  if (m_fd == -1) {
    m_fd = ::open(m_device_file.c_str(), O_RDWR);
    if (m_fd == -1) {
      auto* err = new vidio_error(vidio_error_code_cannot_open_camera, "Cannot open V4L2 camera device '{0}'");
      err->set_arg(0, m_device_file);
      err->set_reason(vidio_error::from_errno());
      return err;
    }
  }

  return nullptr;
}


void vidio_v4l_raw_device::close()
{
  if (m_fd != -1) {
    ::close(m_fd);
    m_fd = -1;
  }
}


bool vidio_input_device_v4l::matches_v4l_raw_device(const vidio_v4l_raw_device* device) const
{
  assert(!m_v4l_capture_devices.empty());

  if (device->get_bus_info() != m_v4l_capture_devices[0]->get_bus_info()) {
    return false;
  }

  return true;
}


vidio_result<std::vector<vidio_input_device_v4l*>> v4l_list_input_devices(const struct vidio_input_device_filter* filter)
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

        auto result = device->query_device((std::string{"/dev/"} + dir->d_name).c_str());

        if (result.error) {
          return {result.error};
        }
        else if (result.value) {
          {
            if (device->has_video_capture_capability()) {
              rawdevices.push_back(device);
            }
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

  for (auto dev : rawdevices) {
    // If there is an existing vidio device for the same hardware device, add the v4l device to this,
    // otherwise create a new vidio device.

    vidio_input_device_v4l* captureDevice = nullptr;
    for (auto cd : devices) {
      if (cd->matches_v4l_raw_device(dev)) {
        captureDevice = cd;
        break;
      }
    }

    if (captureDevice) {
      captureDevice->add_v4l_raw_device(dev);
    }
    else {
      captureDevice = new vidio_input_device_v4l(dev);
      devices.emplace_back(captureDevice);
    }
  }

  return devices;
}


std::vector<vidio_video_format*> vidio_input_device_v4l::get_video_formats() const
{
  std::vector<vidio_video_format*> formats;

  for (auto dev : m_v4l_capture_devices) {
    auto f = dev->get_video_formats();
    formats.insert(formats.end(), f.begin(), f.end());
  }

  return formats;
}


const vidio_error* vidio_input_device_v4l::set_capture_format(const vidio_video_format* requested_format,
                                                              const vidio_video_format** out_actual_format)
{
  const auto* format_v4l = dynamic_cast<const vidio_video_format_v4l*>(requested_format);
  if (!format_v4l) {
    auto* err = new vidio_error(vidio_error_code_parameter_error, "Parameter error: format does not match V4L2 device");
    return err;
  }

  vidio_v4l_raw_device* capturedev = nullptr;
  __u32 pixelformat = format_v4l->get_v4l2_pixel_format();
  for (const auto& dev : m_v4l_capture_devices) {
    if (dev->supports_pixel_format(pixelformat)) {
      capturedev = dev;
      break;
    }
  }

  m_active_device = capturedev;

  if (!capturedev) {
    auto* err = new vidio_error(vidio_error_code_cannot_set_camera_format, "No device with matching pixel format found");
    return err;
  }

  const vidio_video_format_v4l* actual_format = nullptr;
  auto* err = capturedev->set_capture_format(format_v4l, &actual_format);
  if (err) {
    return err;
  }

  if (out_actual_format) {
    *out_actual_format = actual_format;
  }

  return nullptr;
}


const vidio_error* vidio_input_device_v4l::start_capturing()
{
  if (!m_active_device) {
    return new vidio_error(vidio_error_code_usage_error, "Usage error: cannot start capturing without setting capturing parameters.");
  }

  m_capturing_thread = std::thread(&vidio_v4l_raw_device::start_capturing_blocking, m_active_device, this);

  return nullptr;
}


const vidio_error* vidio_input_device_v4l::stop_capturing()
{
  auto* err = m_active_device->stop_capturing();
  if (err) {
    return err;
  }

  if (m_capturing_thread.joinable()) {
    m_capturing_thread.join();
    send_callback_message(vidio_input_message_end_of_stream);
  }

  return nullptr;
}


const vidio_frame* vidio_input_device_v4l::peek_next_frame() const
{
  std::lock_guard<std::mutex> lock(m_queue_mutex);

  if (m_frame_queue.empty()) {
    return nullptr;
  }
  else {
    return m_frame_queue.front();
  }
}

void vidio_input_device_v4l::pop_next_frame()
{
  std::lock_guard<std::mutex> lock(m_queue_mutex);
  delete m_frame_queue.front(); // TODO: we could reuse these frames
  m_frame_queue.pop_front();
}


void vidio_input_device_v4l::push_frame_into_queue(const vidio_frame* f)
{
  bool overflow = false;

  {
    std::lock_guard<std::mutex> lock(m_queue_mutex);

    if (m_frame_queue.size() < cMaxFrameQueueLength) {
      m_frame_queue.push_back(f);
    }
    else {
      overflow = true;
    }
  }

  if (overflow)
    send_callback_message(vidio_input_message_input_overflow);
  else
    send_callback_message(vidio_input_message_new_frame);
}


std::string vidio_input_device_v4l::serialize(vidio_serialization_format serialformat) const
{
#if WITH_JSON
  nlohmann::json json{
      {"class",       "v4l2"},
      {"bus_info",    m_v4l_capture_devices[0]->get_bus_info()},
      {"card",        (const char*) (m_v4l_capture_devices[0]->get_v4l_capabilities().card)},
      {"device_file", m_v4l_capture_devices[0]->get_device_file()}
  };

  return json.dump();
#endif
}


vidio_input_device_v4l* vidio_input_device_v4l::find_matching_device(const std::vector<vidio_input*>& inputs, const nlohmann::json& json)
{
  std::string bus_info = json["bus_info"];
  std::string card = json["card"];
  std::string device_file = json["device_file"];

  int maxScore = 0;
  vidio_input_device_v4l* bestDevice = nullptr;

  for (auto* input : inputs) {
    if (auto inputv4l = dynamic_cast<vidio_input_device_v4l*>(input)) {
      int score = inputv4l->spec_match_score(bus_info, card, device_file);
      if (score > maxScore) {
        maxScore = score;
        bestDevice = inputv4l;
      }
    }
  }

  return bestDevice;
}


int vidio_input_device_v4l::spec_match_score(const std::string& businfo, const std::string& card, const std::string& device_file) const
{
  // full match

  for (const auto& rawdevice : m_v4l_capture_devices) {
    if (rawdevice->get_bus_info() == businfo &&
        (const char*) (rawdevice->get_v4l_capabilities().card) == card &&
        rawdevice->get_device_file() == device_file) {
      return 10;
    }
  }

  // same bus-info and device name

  for (const auto& rawdevice : m_v4l_capture_devices) {
    if (rawdevice->get_bus_info() == businfo ||
        (const char*) (rawdevice->get_v4l_capabilities().card) == card) {
      return 5;
    }
  }

  // same device name

  for (const auto& rawdevice : m_v4l_capture_devices) {
    if ((const char*) (rawdevice->get_v4l_capabilities().card) == card) {
      return 1;
    }
  }

  return 0;
}
