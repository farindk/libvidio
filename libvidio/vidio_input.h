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

  virtual std::vector<vidio_video_format*> get_video_formats() const = 0;

  virtual vidio_error* set_capture_format(const vidio_video_format* requested_format,
                                          vidio_video_format** out_actual_format) = 0;

  virtual void set_message_callback(void (*callback)(enum vidio_input_message, void* userData), void* userData) {
    m_message_callback = callback;
    m_user_data = userData;
  }

  virtual vidio_error* start_capturing() = 0;

  virtual void stop_capturing() = 0;

  virtual const vidio_frame* peek_next_frame() const = 0;

  virtual void pop_next_frame() = 0;

private:
  void (*m_message_callback)(enum vidio_input_message, void* userData) = nullptr;
  void* m_user_data;

protected:
  void send_callback_message(enum vidio_input_message msg) const {
    if (m_message_callback) {
      m_message_callback(msg, m_user_data);
    }
  }
};


struct vidio_input_device : public vidio_input
{
public:
  static std::vector<vidio_input_device*> list_input_devices(const struct vidio_input_device_filter*);
};

#endif //LIBVIDIO_VIDIO_INPUT_H
