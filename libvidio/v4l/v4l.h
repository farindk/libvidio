//
// Created by farindk on 28.12.23.
//

#ifndef LIBVIDIO_V4L_H
#define LIBVIDIO_V4L_H

#include <linux/videodev2.h>
#include <string>
#include <vector>
#include <memory>
#include <libvidio/vidio.h>


class VidioInputDeviceInfo {
public:
  virtual ~VidioInputDeviceInfo() = default;

  virtual std::string get_device_name() const = 0;

  virtual vidio_input_device_backend get_backend() const = 0;
};


class VidioInputDeviceInfo_V4L : public VidioInputDeviceInfo {
public:
  bool query_device(const char* filename);

  std::string get_device_name() const override { return std::string((char*)&m_caps.card[0]); }

  vidio_input_device_backend get_backend() const override { return vidio_input_device_backend_Video4Linux2; }

private:
  std::string m_device_file;

  struct v4l2_capability m_caps;
};


std::vector<std::shared_ptr<VidioInputDeviceInfo_V4L>> v4l_list_input_devices(const struct vidio_input_device_filter* filter);

#endif //LIBVIDIO_V4L_H
