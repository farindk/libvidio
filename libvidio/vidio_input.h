//
// Created by farindk on 29.12.23.
//

#ifndef LIBVIDIO_VIDIO_INPUT_H
#define LIBVIDIO_VIDIO_INPUT_H

#include <libvidio/vidio.h>
#include <string>
#include <vector>


struct vidio_input
{
public:
  virtual ~vidio_input() = default;

  virtual vidio_input_source get_source() const = 0;

  virtual std::string get_display_name() const = 0;
};


struct vidio_input_device : public vidio_input {
public:
  static std::vector<vidio_input_device*> list_input_devices(const struct vidio_input_device_filter*);
};

#endif //LIBVIDIO_VIDIO_INPUT_H
