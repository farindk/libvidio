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
#include <libvidio/vidio_input.h>


struct vidio_input_device_v4l : public vidio_input_device {
public:
  bool query_device(const char* filename);

  std::string get_display_name() const override { return std::string((char*)&m_caps.card[0]); }

  vidio_input_source get_source() const override { return vidio_input_source_Video4Linux2; }

private:
  std::string m_device_file;

  struct v4l2_capability m_caps;
};


std::vector<vidio_input_device_v4l*> v4l_list_input_devices(const struct vidio_input_device_filter* filter);

#endif //LIBVIDIO_V4L_H
