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
#include "libvidio/vidio_error.h"
#include "libvidio/vidio_frame.h"
#include "libvidio/vidio_input.h"
#include "libvidio/vidio_video_format.h"
#include "libvidio/vidio_format_converter.h"
#include "libvidio/colorconversion/converter.h"
#if WITH_VIDEO4LINUX2
#include "libvidio/v4l/vidio_input_device_v4l.h"
#endif
#if WITH_RTSP
#include "libvidio/rtsp/vidio_input_device_rtsp.h"
#endif
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


void vidio_string_free(const char* s)
{
  delete[] s;
}

static const char* make_vidio_string(const std::string& s)
{
  char* out = new char[s.length() + 1];
  strcpy(out, s.c_str());
  return out;
}


void vidio_error_free(const vidio_error* err)
{
  delete err;
}

vidio_error_code vidio_error_get_code(const vidio_error* err)
{
  return err->get_code();
}

const char* vidio_error_get_message(const vidio_error* err)
{
  return make_vidio_string(err->get_formatted_message());
}

const char* vidio_error_get_message_template(const vidio_error* err)
{
  return make_vidio_string(err->get_message_template());
}

const char* vidio_error_get_argument(const vidio_error* err, int n)
{
  return make_vidio_string(err->get_arg(n));
}

int vidio_error_get_number_of_arguments(const vidio_error* err)
{
  return err->get_number_of_args();
}

const vidio_error* vidio_error_get_reason(const vidio_error* err)
{
  return err->get_reason();
}


const vidio_error* vidio_list_input_devices(const struct vidio_input_device_filter* filter,
                                            struct vidio_input_device* const** out_deviceList,
                                            size_t* out_number)
{
  if (out_deviceList == nullptr && out_number == nullptr) {
    return nullptr;
  }

  vidio_result<std::vector<vidio_input_device*>> devicesResult = vidio_input_device::list_input_devices(filter);
  if (devicesResult.error) {
    if (out_deviceList) {
      *out_deviceList = nullptr;
    }

    if (out_number) {
      *out_number = 0;
    }

    return devicesResult.error;
  }

  std::vector<vidio_input_device*>& devices = devicesResult.value;

  auto devlist = new vidio_input_device* [devices.size() + 1];
  for (size_t i = 0; i < devices.size(); i++) {
    devlist[i] = devices[i];
  }

  devlist[devices.size()] = nullptr;

  if (out_deviceList) {
    *out_deviceList = devlist;
  }

  if (out_number) {
    *out_number = devices.size();
  }

  return nullptr;
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


void vidio_input_device_free(const struct vidio_input_device* device)
{
  delete device;
}


const char* vidio_input_serialize(const struct vidio_input* input, vidio_serialization_format serialformat)
{
  return make_vidio_string(input->serialize(serialformat));
}

vidio_input* vidio_input_find_matching_device(struct vidio_input_device* const* in_devices, int nDevices,
                                              const char* serializedString,
                                              vidio_serialization_format serialformat,
                                              enum vidio_device_match* out_opt_matchscore)
{
  std::vector<vidio_input*> devices;
  if (nDevices < 0) {
    for (int i = 0; in_devices[i]; i++) {
      devices.push_back(in_devices[i]);
    }
  }
  else {
    for (int i = 0; i < nDevices; i++) {
      devices.push_back(in_devices[i]);
    }
  }

  return vidio_input::find_matching_device(devices, serializedString, serialformat);
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


void vidio_frame_free(const vidio_frame* f)
{
  delete f;
}

int vidio_frame_get_width(const vidio_frame* f)
{
  return f->get_width();
}

int vidio_frame_get_height(const vidio_frame* f)
{
  return f->get_height();
}

enum vidio_pixel_format vidio_frame_get_pixel_format(const vidio_frame* f)
{
  return f->get_pixel_format();
}

vidio_bool vidio_frame_has_color_plane(const vidio_frame* f, vidio_color_channel c)
{
  return f->has_plane(c);
}

uint8_t* vidio_frame_get_color_plane(vidio_frame* f, vidio_color_channel c, int* stride)
{
  return f->get_plane(c, stride);
}

const uint8_t* vidio_frame_get_color_plane_readonly(const vidio_frame* f, vidio_color_channel c, int* stride)
{
  return f->get_plane(c, stride);
}

uint64_t vidio_frame_get_timestamp_us(const vidio_frame* f)
{
  return f->get_timestamp_us();
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


vidio_pixel_format vidio_video_format_get_pixel_format(const struct vidio_video_format* format)
{
  return format->get_pixel_format();
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

vidio_bool vidio_video_format_has_fixed_framerate(const struct vidio_video_format* format)
{
  return format->has_fixed_framerate();
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

#if WITH_JSON

const char* vidio_video_format_serialize(const struct vidio_video_format* format, vidio_serialization_format serialformat)
{
  return make_vidio_string(format->serialize(serialformat));
}

const vidio_video_format* vidio_video_format_deserialize(const char* serializedString, vidio_serialization_format serialformat)
{
  return vidio_video_format::deserialize(serializedString, serialformat);
}

#endif

const vidio_video_format* vidio_video_format_find_best_match(const vidio_video_format* const* in_formats, int nFormats,
                                                             const vidio_video_format* requested_format,
                                                             enum vidio_device_match* out_score)
{
  std::vector<const vidio_video_format*> formatList;
  if (nFormats < 0) {
    for (int i = 0; in_formats[i]; i++) {
      formatList.push_back(in_formats[i]);
    }
  }
  else {
    for (int i = 0; i < nFormats; i++) {
      formatList.push_back(in_formats[i]);
    }
  }

  return vidio_video_format::find_best_match(formatList, requested_format, out_score);
}


void vidio_video_formats_free_list(const struct vidio_video_format* const* list, int also_free_formats)
{
  if (also_free_formats) {
    for (auto* p = list; *p; p++) {
      delete *p;
    }
  }

  delete[] list;
}

void vidio_video_format_free(const struct vidio_video_format* format)
{
  delete format;
}


const vidio_error* vidio_input_configure_capture(struct vidio_input* input,
                                                 const vidio_video_format* requested_format,
                                                 const vidio_output_format*,
                                                 const vidio_video_format** out_actual_format)
{
  auto err = input->set_capture_format(requested_format, out_actual_format);
  return err;
}


void vidio_input_set_message_callback(struct vidio_input* input,
                                      void (* callback)(enum vidio_input_message, void* userData), void* userData)
{
  input->set_message_callback(callback, userData);
}


const vidio_error* vidio_input_start_capturing(struct vidio_input* input)
{
  return input->start_capturing();
}


const vidio_error* vidio_input_stop_capturing(struct vidio_input* input)
{
  return input->stop_capturing();
}


vidio_frame* vidio_frame_convert(const vidio_frame* f, vidio_pixel_format format)
{
  return convert_frame(f, format);
}


vidio_format_converter* vidio_create_format_converter(vidio_pixel_format from, vidio_pixel_format to)
{
  return vidio_format_converter::create(from, to);
}

void vidio_format_converter_free(vidio_format_converter* converter)
{
  delete converter;
}

vidio_frame* vidio_format_converter_convert_uncompressed(vidio_format_converter* converter, const vidio_frame* f)
{
  converter->push(f);
  return converter->pull();
}

void vidio_format_converter_push_compressed(vidio_format_converter* converter, const vidio_frame* f)
{
  converter->push(f);
}

vidio_frame* vidio_format_converter_pull_decompressed(vidio_format_converter* converter)
{
  return converter->pull();
}

const vidio_frame* vidio_input_peek_next_frame(struct vidio_input* input)
{
  return input->peek_next_frame();
}

void vidio_input_pop_next_frame(struct vidio_input* input)
{
  input->pop_next_frame();
}


void vidio_input_release(struct vidio_input* input)
{
  delete input;
}


// === RTSP Input ===

vidio_input* vidio_create_rtsp_input(const char* url)
{
#if WITH_RTSP
  if (!url) {
    return nullptr;
  }
  return vidio_input_device_rtsp::create(url);
#else
  (void)url;
  return nullptr;
#endif
}


vidio_input* vidio_create_rtsp_input_with_auth(const char* url,
                                               const char* username,
                                               const char* password)
{
#if WITH_RTSP
  if (!url) {
    return nullptr;
  }
  return vidio_input_device_rtsp::create(url,
                                         username ? username : "",
                                         password ? password : "");
#else
  (void)url;
  (void)username;
  (void)password;
  return nullptr;
#endif
}


void vidio_rtsp_set_transport(vidio_input* input, vidio_rtsp_transport transport)
{
#if WITH_RTSP
  if (!input) {
    return;
  }
  auto* rtsp_input = dynamic_cast<vidio_input_device_rtsp*>(input);
  if (rtsp_input) {
    rtsp_input->set_transport(transport);
  }
#else
  (void)input;
  (void)transport;
#endif
}


void vidio_rtsp_set_timeout_seconds(vidio_input* input, int timeout_seconds)
{
#if WITH_RTSP
  if (!input) {
    return;
  }
  auto* rtsp_input = dynamic_cast<vidio_input_device_rtsp*>(input);
  if (rtsp_input) {
    rtsp_input->set_timeout_seconds(timeout_seconds);
  }
#else
  (void)input;
  (void)timeout_seconds;
#endif
}
