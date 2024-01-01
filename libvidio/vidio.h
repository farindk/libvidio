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


struct vidio_video_format;

LIBVIDIO_API uint32_t vidio_video_format_get_width(const struct vidio_video_format* format);

LIBVIDIO_API uint32_t vidio_video_format_get_height(const struct vidio_video_format* format);

LIBVIDIO_API vidio_fraction vidio_video_format_get_framerate(const struct vidio_video_format* format);

LIBVIDIO_API vidio_pixel_format_class
vidio_video_format_get_pixel_format_class(const struct vidio_video_format* format);

LIBVIDIO_API const char* vidio_video_format_get_user_description(const struct vidio_video_format* format);


LIBVIDIO_API const struct vidio_video_format* const*
vidio_input_get_video_formats(const struct vidio_input* input, size_t* out_number);

LIBVIDIO_API void vidio_video_formats_free_list(const struct vidio_video_format* const*);


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

LIBVIDIO_API const struct vidio_input_device* const*
vidio_list_input_devices(const struct vidio_input_device_filter*, size_t* out_number);

LIBVIDIO_API void vidio_input_devices_free_list(const struct vidio_input_device* const* devices, int also_free_devices);

LIBVIDIO_API const char* vidio_input_get_display_name(const struct vidio_input* input);

LIBVIDIO_API enum vidio_input_source vidio_input_get_source(const struct vidio_input* input);

}

#endif //LIBVIDIO_VIDIO_H
