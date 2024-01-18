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

#ifndef LIBVIDIO_VIDIO_CAPTURING_LOOP_H
#define LIBVIDIO_VIDIO_CAPTURING_LOOP_H

#include <libvidio/vidio.h>
#include <functional>
#include <mutex>
#include <condition_variable>
#include <thread>
#include <cassert>
#include <utility>


/** This is a C++ convenience class (header only) that runs a capturing thread in the background
 * and calls a callback whenever a new frame has been captured.
*/
class vidio_capturing_loop
{
public:
  void set_on_frame_received(std::function<void(const vidio_frame*)> f) { m_on_frame_received = std::move(f); }

  void set_on_stream_ended(std::function<void()> f) { m_on_stream_ended = std::move(f); }

  // Starts a new thread that runs the capturing loop in the background.
  const vidio_error* start_with_vidio_input(vidio_input* input)
  {
    assert(!m_active);
    m_input = input;
    m_active = true;

    m_thread = std::thread(&vidio_capturing_loop::loop, this);

    // start vidio input

    vidio_input_set_message_callback(m_input, on_vidio_message, this);

    return vidio_input_start_capturing(m_input);
  }

  // Stop the vidio_input. Will emit an 'on_stream_ended' callback.
  void stop()
  {
    m_active = false;
    m_cond.notify_one();
    m_thread.join();
  }

private:
  vidio_input* m_input;
  bool m_active = false;

  std::thread m_thread;
  std::mutex m_mutex;
  std::condition_variable m_cond;

  std::function<void(const vidio_frame*)> m_on_frame_received;
  std::function<void()> m_on_stream_ended;

  void loop()
  {
    for (;;) {
      // wait until more frames received or receiver stopped by user

      bool active;
      bool haveStopped = false;

      {
        std::unique_lock<std::mutex> lock(m_mutex);
        while (vidio_input_peek_next_frame(m_input) == nullptr && m_active) {
          m_cond.wait(lock);
        }

        active = m_active;
      }

      if (!active) {
        // stop the input capturing. We will still process the frames that remain in the input queue.
        vidio_input_stop_capturing(m_input);
      }

      while (const vidio_frame* frame = vidio_input_peek_next_frame(m_input)) {
        if (m_on_frame_received) {
          m_on_frame_received(frame);
        }

        if (!active && !haveStopped) {
          haveStopped = true;

          // stop the input capturing. We will still process the frames that remain in the input queue.
          vidio_input_stop_capturing(m_input);
        }

        vidio_input_pop_next_frame(m_input);
      }

      if (!active) {
        break;
      }
    }

    // send message that stream ended

    if (m_on_stream_ended) {
      m_on_stream_ended();
    }
  }

  static void on_vidio_message(vidio_input_message msg, void* userData)
  {
    auto* me = static_cast<vidio_capturing_loop*>(userData);

    if (msg == vidio_input_message_end_of_stream) {
      if (me->m_active) {
        me->m_active = false;
        me->m_cond.notify_one();
      }
    }
    else if (msg == vidio_input_message_new_frame) {
      me->m_cond.notify_one();
    }
  }
};

#endif //LIBVIDIO_VIDIO_CAPTURING_LOOP_H
