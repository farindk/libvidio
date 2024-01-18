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

#include "vidio_v4l_raw_device.h"
#include "vidio_video_format_v4l.h"
#include "vidio_input_device_v4l.h"
#include "libvidio/vidio_frame.h"

#include <sys/types.h>
#include <sys/stat.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <fcntl.h>
#include <unistd.h>
#include <cassert>


vidio_v4l_raw_device::~vidio_v4l_raw_device()
{
  close();
  delete m_capture_format;
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

  delete m_capture_format;
  m_capture_format = dynamic_cast<vidio_video_format_v4l*>(format_v4l->clone());
  assert(m_capture_format);

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
      case V4L2_PIX_FMT_SRGGB8:
        frame->set_format(vidio_pixel_format_RGGB8, m_capture_width, m_capture_height);
        frame->add_raw_plane(vidio_color_channel_interleaved, 8);
        frame->copy_raw_plane(vidio_color_channel_interleaved, (const uint8_t*) buffer.start, buf.bytesused);
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

  if (strncmp((const char*)(m_caps.card), "Creative WebCam Live! Motion", 32)==0) {
    // This camera needs to be closed after capturing. Otherwise it won't accept a different S_FMT.
    close();
  }

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
