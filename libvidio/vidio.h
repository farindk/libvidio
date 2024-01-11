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

#ifndef LIBVIDIO_VIDIO_H
#define LIBVIDIO_VIDIO_H

#if defined(_MSC_VER) && !defined(LIBVIDIO_STATIC_BUILD)
#ifdef LIBVIDIO_EXPORTS
#define LIBVIDIO_API __declspec(dllexport)
#else
#define LIBVIDIO_API __declspec(dllimport)
#endif
#elif defined(HAVE_VISIBILITY) && HAVE_VISIBILITY
#ifdef LIBVIDIO_EXPORTS
#define LIBVIDIO_API __attribute__((__visibility__("default")))
#else
#define LIBVIDIO_API
#endif
#else
#define LIBVIDIO_API
#endif

#include <stdint.h>
#include <stddef.h>


extern "C" {

/* === Version Numbers === */

// Version string of linked libheif library.
LIBVIDIO_API const char* vidio_get_version(void);

// BCD-coded numeric version of linked libheif library, encoded as 0xHHMMLL00.
LIBVIDIO_API uint32_t vidio_get_version_number(void);

// Numeric part "HH" from above. Returned as a decimal number.
LIBVIDIO_API int vidio_get_version_number_major(void);
// Numeric part "MM" from above. Returned as a decimal number.
LIBVIDIO_API int vidio_get_version_number_minor(void);
// Numeric part "LL" from above. Returned as a decimal number.
LIBVIDIO_API int vidio_get_version_number_patch(void);


// === Generic Types ===

LIBVIDIO_API void vidio_free_string(const char*);


struct vidio_fraction
{
  int numerator;
  int denominator;
};

LIBVIDIO_API double vidio_fraction_to_double(const struct vidio_fraction*);


struct vidio_error;

enum vidio_error_code
{
  vidio_error_code_success = 0,
  vidio_error_code_other = 1 // TODO
};

LIBVIDIO_API void vidio_error_release(const vidio_error*);

LIBVIDIO_API vidio_error_code vidio_error_get_code(const vidio_error*);

// free with vidio_free_string()
LIBVIDIO_API const char* vidio_error_get_message(const vidio_error*);

// free with vidio_free_string()
LIBVIDIO_API const char* vidio_error_get_message_template(const vidio_error*);

// free with vidio_free_string()
LIBVIDIO_API const char* vidio_error_get_argument(const vidio_error*, int n);

LIBVIDIO_API int vidio_error_get_number_of_arguments(const vidio_error*);


// === Video Frame ===

enum vidio_pixel_format
{
  vidio_pixel_format_undefined = 0,
  vidio_pixel_format_RGB8 = 1,
  vidio_pixel_format_RGB8_planar = 2,
  vidio_pixel_format_YUV420_planar = 100,
  vidio_pixel_format_YUV422_YUYV = 101,
  vidio_pixel_format_MJPEG = 500,
  vidio_pixel_format_H264 = 501,
  vidio_pixel_format_H265 = 502
};

enum vidio_color_channel
{
  vidio_color_channel_undefined = 0,
  vidio_color_channel_R = 1,
  vidio_color_channel_G = 2,
  vidio_color_channel_B = 3,
  vidio_color_channel_Y = 4,
  vidio_color_channel_U = 5,
  vidio_color_channel_V = 6,
  vidio_color_channel_alpha = 7,
  vidio_color_channel_depth = 8,
  vidio_color_channel_interleaved = 100,
  vidio_color_channel_compressed = 101
};

enum vidio_channel_format
{
  vidio_channel_format_undefined = 0,
  vidio_channel_format_pixels = 1,
  vidio_channel_format_compressed_MJPEG = 500,
  vidio_channel_format_compressed_H264 = 501,
  vidio_channel_format_compressed_H265 = 502
};

struct vidio_frame;

LIBVIDIO_API void vidio_frame_release(const vidio_frame*);

LIBVIDIO_API int vidio_frame_get_width(const vidio_frame*);

LIBVIDIO_API int vidio_frame_get_height(const vidio_frame*);

LIBVIDIO_API enum vidio_pixel_format vidio_frame_get_pixel_format(const vidio_frame*);

LIBVIDIO_API int vidio_frame_has_color_plane(const vidio_frame*, vidio_color_channel);

LIBVIDIO_API uint8_t* vidio_frame_get_color_plane(vidio_frame*, vidio_color_channel, int* stride);

LIBVIDIO_API const uint8_t* vidio_frame_get_color_plane_readonly(const vidio_frame*, vidio_color_channel, int* stride);


// === Format Conversion ===

LIBVIDIO_API vidio_frame* vidio_frame_convert(const vidio_frame*, vidio_pixel_format);


struct vidio_format_converter;

LIBVIDIO_API vidio_format_converter* vidio_create_converter(vidio_pixel_format from, vidio_pixel_format to);

LIBVIDIO_API void vidio_format_converter_release(vidio_format_converter*);

LIBVIDIO_API vidio_frame* vidio_format_converter_convert_uncompressed(vidio_format_converter*, const vidio_frame*);

LIBVIDIO_API void vidio_format_converter_push_compressed(vidio_format_converter*, const vidio_frame*);

LIBVIDIO_API vidio_frame* vidio_format_converter_pull_decompressed(vidio_format_converter*);


// === Video Format ===

enum vidio_pixel_format_class
{
  vidio_pixel_format_class_unknown = 0,
  vidio_pixel_format_class_RGB = 1,
  vidio_pixel_format_class_YUV = 2,
  vidio_pixel_format_class_MJPEG = 3,
  vidio_pixel_format_class_H264 = 4,
  vidio_pixel_format_class_H265 = 5
};

// Do not release the returned string.
LIBVIDIO_API const char* vidio_pixel_format_class_name(enum vidio_pixel_format_class format);

LIBVIDIO_API vidio_pixel_format vidio_video_format_get_pixel_format(const struct vidio_video_format* format);

struct vidio_video_format;

LIBVIDIO_API uint32_t vidio_video_format_get_width(const struct vidio_video_format* format);

LIBVIDIO_API uint32_t vidio_video_format_get_height(const struct vidio_video_format* format);

LIBVIDIO_API vidio_fraction vidio_video_format_get_framerate(const struct vidio_video_format* format);

LIBVIDIO_API vidio_pixel_format_class
vidio_video_format_get_pixel_format_class(const struct vidio_video_format* format);

LIBVIDIO_API const char* vidio_video_format_get_user_description(const struct vidio_video_format* format);


LIBVIDIO_API const struct vidio_video_format* const*
vidio_input_get_video_formats(const struct vidio_input* input, size_t* out_number);

LIBVIDIO_API void vidio_video_formats_free_list(const struct vidio_video_format* const*, int also_free_formats);


// === Video Inputs ===

/*
 * Hierarchy:
 * vidio_input
 * +- vidio_input_device
 * |  +- vidio_input_device_v4l
 * +- vidio_input_file
 */

struct vidio_input;
struct vidio_input_file;
struct vidio_input_device;
struct vidio_input_device_v4l;
struct vidio_input_device_filter;

enum vidio_input_source
{
  vidio_input_source_Video4Linux2 = 1
};

enum vidio_input_message
{
  vidio_input_message_new_frame,
  vidio_input_message_end_of_stream,
  vidio_input_message_input_overflow
};

LIBVIDIO_API const struct vidio_input_device* const*
vidio_list_input_devices(const struct vidio_input_device_filter*, size_t* out_number);

LIBVIDIO_API void vidio_input_devices_free_list(const struct vidio_input_device* const* devices, int also_free_devices);

LIBVIDIO_API const char* vidio_input_get_display_name(const struct vidio_input* input);

LIBVIDIO_API enum vidio_input_source vidio_input_get_source(const struct vidio_input* input);

struct vidio_output_format;

LIBVIDIO_API vidio_error* vidio_input_configure_capture(struct vidio_input* input,
                                                        const vidio_video_format* requested_format,
                                                        const vidio_output_format*,
                                                        vidio_video_format** out_actual_format);

struct vidio_frame;

// Note: do no heavy processing in the callback function as this runs in the main capturing loop.
LIBVIDIO_API void vidio_input_set_message_callback(struct vidio_input* input,
                                                   void (*)(enum vidio_input_message, void* userData), void* userData);

LIBVIDIO_API vidio_error* vidio_input_start_capturing(struct vidio_input* input);

LIBVIDIO_API void vidio_input_stop_capturing(struct vidio_input* input);

LIBVIDIO_API const vidio_frame* vidio_input_peek_next_frame(struct vidio_input* input);

LIBVIDIO_API void vidio_input_pop_next_frame(struct vidio_input* input);
}

#endif //LIBVIDIO_VIDIO_H
