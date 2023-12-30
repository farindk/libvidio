//
// Created by farindk on 29.12.23.
//

#ifndef LIBVIDIO_VIDIOINPUTSOURCE_H
#define LIBVIDIO_VIDIOINPUTSOURCE_H

#include <libvidio/vidio.h>
#include <string>
#include <vector>


class VidioInput
{
public:
  virtual ~VidioInput() = default;

  virtual vidio_input_source get_source() const = 0;

  virtual std::string get_display_name() const = 0;
};


class VidioInputDevice : public VidioInput {
public:
  static std::vector<VidioInputDevice*> list_input_devices(const struct vidio_input_device_filter*);
};

#endif //LIBVIDIO_VIDIOINPUTSOURCE_H
