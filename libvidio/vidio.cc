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

#include "libvidio/vidio.h"
#include "libvidio/vidio_input.h"
#include "libvidio/vidio_video_format.h"
#include <cassert>
#include <cstring>

static uint8_t vidio_version_major = VIDIO_VERSION_MAJOR;
static uint8_t vidio_version_minor = VIDIO_VERSION_MINOR;
static uint8_t vidio_version_patch = VIDIO_VERSION_PATCH;

#define xstr(s) str(s)
#define str(s) #s

static const char* vidio_version_string = xstr(VIDIO_VERSION_MAJOR) "." xstr(VIDIO_VERSION_MINOR) "." xstr(
        VIDIO_VERSION_PATCH);

const char* vidio_get_version(void)
{
  return vidio_version_string;
}

static constexpr int encode_bcd(int v)
{
  assert(v <= 99);
  assert(v >= 0);

  return (v / 10) * 16 + (v % 10);
}

uint32_t vidio_get_version_number(void)
{
  return ((encode_bcd(vidio_version_major) << 16) |
          (encode_bcd(vidio_version_minor) << 8) |
          (encode_bcd(vidio_version_patch)));
}

int vidio_get_version_number_major(void)
{
  return vidio_version_major;
}

int vidio_get_version_number_minor(void)
{
  return vidio_version_minor;
}

int vidio_get_version_number_patch(void)
{
  return vidio_version_patch;
}


void vidio_free_string(const char* s)
{
  delete[] s;
}


const struct vidio_input_device* const*
vidio_list_input_devices(const struct vidio_input_device_filter* filter, size_t* out_number)
{
  std::vector<vidio_input_device*> devices = vidio_input_device::list_input_devices(filter);

  auto devlist = new vidio_input_device* [devices.size() + 1];
  for (size_t i = 0; i < devices.size(); i++) {
    devlist[i] = devices[i];
  }

  devlist[devices.size()] = nullptr;

  if (out_number) {
    *out_number = devices.size();
  }

  return devlist;
}

void vidio_input_devices_free_list(const struct vidio_input_device* const* out_devices, int also_free_devices)
{
  if (also_free_devices) {
    for (auto p = out_devices; *p; p++) {
      delete *p;
    }
  }

  delete[] out_devices;
}


const char* make_vidio_string(const std::string& s)
{
  char* out = new char[s.length() + 1];
  strcpy(out, s.c_str());
  return out;
}

const char* vidio_input_get_display_name(const struct vidio_input* input)
{
  return make_vidio_string(input->get_display_name());
}

enum vidio_input_source vidio_input_get_source(const struct vidio_input* input)
{
  return input->get_source();
}


double vidio_fraction_to_double(const struct vidio_fraction* fraction)
{
  return fraction->numerator / (double) fraction->denominator;
}


const struct vidio_video_format* const*
vidio_input_get_video_formats(const struct vidio_input* input, size_t* out_number)
{
  auto formats = input->get_video_formats();

  auto outFormats = new vidio_video_format* [formats.size() + 1];
  for (size_t i = 0; i < formats.size(); i++) {
    outFormats[i] = formats[i];
  }
  outFormats[formats.size()] = nullptr;

  if (out_number) {
    *out_number = formats.size();
  }

  return outFormats;
}


const char* vidio_pixel_format_class_name(enum vidio_pixel_format_class format)
{
  switch (format) {
    case vidio_pixel_format_class_unknown:
      return "unknown";
    case vidio_pixel_format_class_RGB:
      return "RGB";
    case vidio_pixel_format_class_YUV:
      return "YUV";
    case vidio_pixel_format_class_MJPEG:
      return "MJPEG";
    case vidio_pixel_format_class_H264:
      return "H264";
    case vidio_pixel_format_class_H265:
      return "H265";
  }

  assert(false);
  return "invalid";
}


void vidio_video_formats_free_list(const struct vidio_video_format* formats)
{
  delete[] formats;
}


uint32_t vidio_video_format_get_width(const struct vidio_video_format* format)
{
  return format->get_width();
}

uint32_t vidio_video_format_get_height(const struct vidio_video_format* format)
{
  return format->get_height();
}

vidio_fraction vidio_video_format_get_framerate(const struct vidio_video_format* format)
{
  return format->get_framerate();
}

vidio_pixel_format_class vidio_video_format_get_pixel_format_class(const struct vidio_video_format* format)
{
  return format->get_pixel_format_class();
}

const char* vidio_video_format_get_user_description(const struct vidio_video_format* format)
{
  return make_vidio_string(format->get_user_description());
}

void vidio_video_formats_free_list(const struct vidio_video_format* const* list)
{
  for (auto* p = list; *p; p++) {
    delete *p;
  }

  delete[] list;
}


vidio_error* vidio_input_configure_capture(struct vidio_input* input,
                                           const vidio_video_format* requested_format,
                                           const vidio_output_format*,
                                           vidio_video_format** out_actual_format)
{
  auto err = input->set_capture_format(requested_format, out_actual_format);
  return err;
}


vidio_error* vidio_input_start_capture_blocking(struct vidio_input* input, void (*callback)(const vidio_frame*))
{
  return input->start_capturing_blocking(callback);
}
