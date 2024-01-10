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


vidio_pixel_format vidio_video_format_v4l::get_pixel_format() const
{
  return v4l_pixelformat_to_pixel_format(m_format.pixelformat);;
}


bool vidio_v4l_raw_device::query_device(const char* filename)
{
  m_fd = ::open(filename, O_RDWR);
  if (m_fd == -1) {
    return false;
  }

  int ret;

  ret = ioctl(m_fd, VIDIOC_QUERYCAP, &m_caps);
  if (ret == -1) {
    ::close(m_fd);
    m_fd = -1;
    return false;
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
      return false;
    }
    m_supports_framerate = !!(streamparm.parm.capture.capability & V4L2_CAP_TIMEPERFRAME);

    // --- list supported formats

    auto formats = list_v4l_formats(V4L2_BUF_TYPE_VIDEO_CAPTURE);
    for (auto f : formats) {
      format_v4l format;
      format.m_fmtdesc = f;

      auto frmsizes = list_v4l_framesizes(f.pixelformat);
      for (auto s : frmsizes) {
        framesize_v4l fsize;
        fsize.m_framesize = s;

        if (m_supports_framerate) {
          std::vector<v4l2_frmivalenum> frmintervals;
          if (s.type == V4L2_FRMSIZE_TYPE_DISCRETE) {
            frmintervals = list_v4l_frameintervals(f.pixelformat, s.discrete.width, s.discrete.height);
          }
          else {
            frmintervals = list_v4l_frameintervals(f.pixelformat, s.stepwise.max_width, s.stepwise.max_height);
          }

          fsize.m_frameintervals = frmintervals;
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
      int e = errno;
      (void) e;
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

vidio_error* vidio_v4l_raw_device::set_capture_format(const vidio_video_format_v4l* format_v4l,
                                                      vidio_video_format_v4l** out_format)
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
    return 0; // TODO
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
      return 0; // TODO
    }

    printf("set framerate: %d/%d\n",
           param.parm.capture.timeperframe.denominator,
           param.parm.capture.timeperframe.numerator);
  }

  return nullptr;
}


vidio_error* vidio_v4l_raw_device::start_capturing_blocking(vidio_input_device_v4l* input_device)
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
    if (EINVAL == errno) {
      fprintf(stderr, "%s does not support "
                      "memory mappingn", "QWE");
      exit(EXIT_FAILURE);
    }
    else {
//      errno_exit("VIDIOC_REQBUFS");
    }
  }

  if (req.count < 2) {
    fprintf(stderr, "Insufficient buffer memory on %s\\n",
            "QWE"); //          dev_name);
    exit(EXIT_FAILURE);
  }

  m_buffers.resize(req.count);

  for (__u32 i = 0 /**/; i < req.count; i++) {
    v4l2_buffer buf{};

    buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    buf.memory = V4L2_MEMORY_MMAP;
    buf.index = i;

    if (-1 == ioctl(m_fd, VIDIOC_QUERYBUF, &buf)) {
      return 0; // TODO
      //  errno_exit("VIDIOC_QUERYBUF");
    }

    m_buffers[i].length = buf.length;
    m_buffers[i].start =
        mmap(nullptr /* start anywhere */,
             buf.length,
             PROT_READ | PROT_WRITE /* required */,
             MAP_SHARED /* recommended */,
             m_fd, buf.m.offset);

    if (MAP_FAILED == m_buffers[i].start) {
      int e = errno;
      (void) e;
      return 0; // TODO
      //errno_exit("mmap");
    }
  }

  // --- queue all buffers

  for (size_t i = 0; i < m_buffers.size(); i++) {
    v4l2_buffer buf{};

    buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    buf.memory = V4L2_MEMORY_MMAP;
    buf.index = (__u32) i;

    if (-1 == ioctl(m_fd, VIDIOC_QBUF, &buf)) {
      return 0; // TODO
    }
  }

  // --- switch on streaming

  v4l2_buf_type type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
  if (-1 == ioctl(m_fd, VIDIOC_STREAMON, &type)) {
    return 0; // TODO
  }

  //FILE* fh = fopen("/home/farindk/out.bin", "wb");

  m_capturing_active = true;

  int cnt = 0;
  while (m_capturing_active) {
    cnt++;

    fd_set fds;
    FD_ZERO(&fds);
    FD_SET(m_fd, &fds);

    /* Timeout. */
    struct timeval tv{};
    tv.tv_sec = 2;
    tv.tv_usec = 0;

    int r = select(m_fd + 1, &fds, nullptr, nullptr, &tv);
    if (r == -1) {
      if (errno == EINTR)
        continue;
      else {
        return 0; // TODO
      }
    }

    // get frame

    v4l2_buffer buf{};

    buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    buf.memory = V4L2_MEMORY_MMAP;

    if (-1 == ioctl(m_fd, VIDIOC_DQBUF, &buf)) {
      return 0; // TODO
    }

    printf("%d : got %p %d at %d.%d:%d %ld - #%d\n", cnt, m_buffers[buf.index].start, buf.bytesused,
           buf.timecode.minutes, buf.timecode.seconds, buf.timecode.frames,
           buf.timestamp.tv_usec, buf.sequence);
    //fwrite(m_buffers[buf.index].start, buf.bytesused, 1, fh);

    const buffer& buffer = m_buffers[buf.index];

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
    }

    input_device->push_frame_into_queue(frame);

    // --- re-queue buffer

    if (-1 == ioctl(m_fd, VIDIOC_QBUF, &buf)) {
      return 0; // TODO
    }
  }


  type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
  if (-1 == ioctl(m_fd, VIDIOC_STREAMOFF, &type)) {
    return 0; // TODO
  }

  //fclose(fh);

  return nullptr;
}


vidio_error* vidio_v4l_raw_device::open()
{
  if (m_fd == -1) {
    m_fd = ::open(m_device_file.c_str(), O_RDWR);
    if (m_fd == -1) {
      return 0; // TODO
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


vidio_error* vidio_input_device_v4l::set_capture_format(const vidio_video_format* requested_format,
                                                        vidio_video_format** out_actual_format)
{
  const auto* format_v4l = dynamic_cast<const vidio_video_format_v4l*>(requested_format);
  if (!format_v4l) {
    return 0; // TOOD
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
    return 0; // TODO
  }

  vidio_video_format_v4l* actual_format = nullptr;
  auto* err = capturedev->set_capture_format(format_v4l, &actual_format);
  if (err) {
    return err;
  }

  if (out_actual_format) {
    *out_actual_format = actual_format;
  }

  return nullptr;
}


vidio_error* vidio_input_device_v4l::start_capturing()
{
  if (!m_active_device) {
    return 0; // TODO
  }

  m_capturing_thread = std::thread(&vidio_v4l_raw_device::start_capturing_blocking, m_active_device, this);

  //m_active_device->start_capturing_blocking(callback);

  return nullptr;
}


void vidio_input_device_v4l::stop_capturing()
{
  m_active_device->stop_capturing();
  if (m_capturing_thread.joinable()) {
    m_capturing_thread.join();
  }
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
