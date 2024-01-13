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

#ifndef LIBVIDIO_VIDIO_FORMAT_CONVERTER_H
#define LIBVIDIO_VIDIO_FORMAT_CONVERTER_H

#include <libvidio/vidio.h>
#include <libvidio/vidio_frame.h>
#include <deque>
#include <mutex>


struct vidio_format_converter
{
public:
  virtual ~vidio_format_converter() {
    for (auto* frame : m_output_queue) {
      delete frame;
    }
  }

  virtual void push(const vidio_frame* in) = 0;

  vidio_frame* pull() {
    std::unique_lock<std::mutex> lock(m_mutex);

    if (m_output_queue.empty()) {
      return nullptr;
    }
    else {
      auto* frame = m_output_queue.front();
      m_output_queue.pop_front();
      return frame;
    }
  }

  static vidio_format_converter* create(vidio_pixel_format in, vidio_pixel_format out);

private:
  mutable std::mutex m_mutex;
  std::deque<vidio_frame*> m_output_queue;

protected:
  // derived classes should call this to deposit the decoded frames
  void push_decoded_frame(vidio_frame* f) {
    std::unique_lock<std::mutex> lock(m_mutex);
    m_output_queue.push_back(f);
  }

  vidio_format_converter() = default;
};


#endif //LIBVIDIO_VIDIO_FORMAT_CONVERTER_H
