//
// Created by farindk on 30.12.23.
//

#include <libvidio/VidioInputSource.h>
#include <vector>

#if WITH_VIDEO4LINUX2
#include "v4l/v4l.h"
#endif


std::vector<VidioInputDevice*> VidioInputDevice::list_input_devices(const struct vidio_input_device_filter* filter)
{
  std::vector<VidioInputDevice*> devices;

#if WITH_VIDEO4LINUX2
  std::vector<VidioInputDeviceV4L*> devs;
  devs = v4l_list_input_devices(filter);

  for (auto d : devs) {
    devices.emplace_back(d);
  }
#endif

  return devices;
}
