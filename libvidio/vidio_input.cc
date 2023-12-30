//
// Created by farindk on 30.12.23.
//

#include <libvidio/vidio_input.h>
#include <vector>

#if WITH_VIDEO4LINUX2
#include "v4l/v4l.h"
#endif


std::vector<vidio_input_device*> vidio_input_device::list_input_devices(const struct vidio_input_device_filter* filter)
{
  std::vector<vidio_input_device*> devices;

#if WITH_VIDEO4LINUX2
  std::vector<vidio_input_device_v4l*> devs;
  devs = v4l_list_input_devices(filter);

  for (auto d : devs) {
    devices.emplace_back(d);
  }
#endif

  return devices;
}
