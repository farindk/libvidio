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

#include "libvidio/vidio_frame.h"
#include "libvidio/v4l/vidio_v4l_raw_device.h"
#include "libvidio/v4l/vidio_input_device_v4l.h"
#include <dirent.h>
#include <cstring>
#include <algorithm>
#include "libvidio/vidio_error.h"


std::string vidio_input_device_v4l::get_display_name() const
{
  return m_v4l_capture_devices[0]->get_display_name();
}


bool vidio_input_device_v4l::matches_v4l_raw_device(const vidio_v4l_raw_device* device) const
{
  assert(!m_v4l_capture_devices.empty());

  if (device->get_bus_info() != m_v4l_capture_devices[0]->get_bus_info()) {
    return false;
  }

  return true;
}


vidio_result<std::vector<vidio_input_device_v4l*>> v4l_list_input_devices(const struct vidio_input_device_filter* filter)
{
  std::vector<vidio_v4l_raw_device*> rawdevices;
  std::vector<vidio_input_device_v4l*> devices;

  DIR* d;
  struct dirent* dir;
  d = opendir("/dev");
  if (d) {
    while ((dir = readdir(d)) != nullptr) {
      if (strncmp(dir->d_name, "video", 5) == 0) {
        auto device = new vidio_v4l_raw_device();

        auto result = device->query_device((std::string{"/dev/"} + dir->d_name).c_str());

        if (result.error) {
          return {result.error};
        }
        else if (result.value) {
          {
            if (device->has_video_capture_capability()) {
              rawdevices.push_back(device);
            }
          }
        }

        // if we didn't add it to the list, delete it again
        if (rawdevices.empty() || rawdevices.back() != device) {
          delete device;
        }
      }
    }
    closedir(d);
  }


  // --- group v4l devices that operate on the same hardware

  for (auto dev : rawdevices) {
    // If there is an existing vidio device for the same hardware device, add the v4l device to this,
    // otherwise create a new vidio device.

    vidio_input_device_v4l* captureDevice = nullptr;
    for (auto cd : devices) {
      if (cd->matches_v4l_raw_device(dev)) {
        captureDevice = cd;
        break;
      }
    }

    if (captureDevice) {
      captureDevice->add_v4l_raw_device(dev);
    }
    else {
      captureDevice = new vidio_input_device_v4l(dev);
      devices.emplace_back(captureDevice);
    }
  }

  return devices;
}


std::vector<vidio_video_format*> vidio_input_device_v4l::get_video_formats() const
{
  std::vector<vidio_video_format*> formats;

  for (auto dev : m_v4l_capture_devices) {
    auto f = dev->get_video_formats();
    formats.insert(formats.end(), f.begin(), f.end());
  }

  return formats;
}


const vidio_error* vidio_input_device_v4l::set_capture_format(const vidio_video_format* requested_format,
                                                              const vidio_video_format** out_actual_format)
{
  const auto* format_v4l = dynamic_cast<const vidio_video_format_v4l*>(requested_format);
  if (!format_v4l) {
    auto* err = new vidio_error(vidio_error_code_parameter_error, "Parameter error: format does not match V4L2 device");
    return err;
  }

  vidio_v4l_raw_device* capturedev = nullptr;
  __u32 pixelformat = format_v4l->get_v4l2_pixel_format();
  for (const auto& dev : m_v4l_capture_devices) {
    if (dev->supports_pixel_format(pixelformat)) {
      capturedev = dev;
      break;
    }
  }

  m_active_device = capturedev;

  if (!capturedev) {
    auto* err = new vidio_error(vidio_error_code_cannot_set_camera_format, "No device with matching pixel format found");
    return err;
  }

  const vidio_video_format_v4l* actual_format = nullptr;
  auto* err = capturedev->set_capture_format(format_v4l, &actual_format);
  if (err) {
    return err;
  }

  if (out_actual_format) {
    *out_actual_format = actual_format;
  }

  return nullptr;
}


const vidio_error* vidio_input_device_v4l::start_capturing()
{
  if (!m_active_device) {
    return new vidio_error(vidio_error_code_usage_error, "Usage error: cannot start capturing without setting capturing parameters.");
  }

  m_capturing_thread = std::thread(&vidio_v4l_raw_device::start_capturing_blocking, m_active_device, this);

  return nullptr;
}


const vidio_error* vidio_input_device_v4l::stop_capturing()
{
  auto* err = m_active_device->stop_capturing();
  if (err) {
    return err;
  }

  if (m_capturing_thread.joinable()) {
    m_capturing_thread.join();
    send_callback_message(vidio_input_message_end_of_stream);
  }

  return nullptr;
}


const vidio_frame* vidio_input_device_v4l::peek_next_frame() const
{
  std::lock_guard<std::mutex> lock(m_queue_mutex);

  if (m_frame_queue.empty()) {
    return nullptr;
  }
  else {
    return m_frame_queue.front();
  }
}

void vidio_input_device_v4l::pop_next_frame()
{
  std::lock_guard<std::mutex> lock(m_queue_mutex);
  delete m_frame_queue.front(); // TODO: we could reuse these frames
  m_frame_queue.pop_front();
}


void vidio_input_device_v4l::push_frame_into_queue(const vidio_frame* f)
{
  bool overflow = false;

  {
    std::lock_guard<std::mutex> lock(m_queue_mutex);

    if (m_frame_queue.size() < cMaxFrameQueueLength) {
      m_frame_queue.push_back(f);
    }
    else {
      overflow = true;
    }
  }

  if (overflow)
    send_callback_message(vidio_input_message_input_overflow);
  else
    send_callback_message(vidio_input_message_new_frame);
}


std::string vidio_input_device_v4l::serialize(vidio_serialization_format serialformat) const
{
#if WITH_JSON
  nlohmann::json json{
      {"class",       "v4l2"},
      {"bus_info",    m_v4l_capture_devices[0]->get_bus_info()},
      {"card",        (const char*) (m_v4l_capture_devices[0]->get_v4l_capabilities().card)},
      {"device_file", m_v4l_capture_devices[0]->get_device_file()}
  };

  return json.dump();
#endif
}


vidio_input_device_v4l* vidio_input_device_v4l::find_matching_device(const std::vector<vidio_input*>& inputs, const nlohmann::json& json)
{
  std::string bus_info = json["bus_info"];
  std::string card = json["card"];
  std::string device_file = json["device_file"];

  int maxScore = 0;
  vidio_input_device_v4l* bestDevice = nullptr;

  for (auto* input : inputs) {
    if (auto inputv4l = dynamic_cast<vidio_input_device_v4l*>(input)) {
      int score = inputv4l->spec_match_score(bus_info, card, device_file);
      if (score > maxScore) {
        maxScore = score;
        bestDevice = inputv4l;
      }
    }
  }

  return bestDevice;
}


int vidio_input_device_v4l::spec_match_score(const std::string& businfo, const std::string& card, const std::string& device_file) const
{
  // full match

  for (const auto& rawdevice : m_v4l_capture_devices) {
    if (rawdevice->get_bus_info() == businfo &&
        (const char*) (rawdevice->get_v4l_capabilities().card) == card &&
        rawdevice->get_device_file() == device_file) {
      return 10;
    }
  }

  // same bus-info and device name

  for (const auto& rawdevice : m_v4l_capture_devices) {
    if (rawdevice->get_bus_info() == businfo ||
        (const char*) (rawdevice->get_v4l_capabilities().card) == card) {
      return 5;
    }
  }

  // same device name

  for (const auto& rawdevice : m_v4l_capture_devices) {
    if ((const char*) (rawdevice->get_v4l_capabilities().card) == card) {
      return 1;
    }
  }

  return 0;
}
