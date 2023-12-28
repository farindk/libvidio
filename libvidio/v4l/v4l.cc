//
// Created by farindk on 28.12.23.
//

#include "libvidio/v4l/v4l.h"
#include <dirent.h>
#include <cstdio>
#include <cstring>
#include <optional>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/ioctl.h>
#include <fcntl.h>
#include <unistd.h>


bool VidioInputDeviceInfo_V4L::query_device(const char* filename)
{
  int fd = open(filename, 0);
  if (fd == -1) {
    return false;
  }

  int ret;

  ret = ioctl(fd, VIDIOC_QUERYCAP, &m_caps);
  if (ret == -1) {
    return false;
  }

  m_device_file = filename;

  close(fd);

  return true;
}


std::vector<std::shared_ptr<VidioInputDeviceInfo_V4L>> v4l_list_input_devices(const struct vidio_input_device_filter* filter)
{
  std::vector<std::shared_ptr<VidioInputDeviceInfo_V4L>> devices;

  DIR *d;
  struct dirent *dir;
  d = opendir("/dev");
  if (d) {
    while ((dir = readdir(d)) != NULL) {
      if (strncmp(dir->d_name, "video", 5)==0) {
        auto device = std::make_shared<VidioInputDeviceInfo_V4L>();

        if (device->query_device((std::string{"/dev/"} + dir->d_name).c_str())) {
          devices.push_back(device);
        }
      }
    }
    closedir(d);
  }

  return devices;
}
