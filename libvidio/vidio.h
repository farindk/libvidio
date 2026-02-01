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

// Version string of linked libheif library. You do not have to free the string.
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

typedef int vidio_bool;

// Many libvidio functions return strings. When not otherwise noted, they have to be release with this function.
LIBVIDIO_API void vidio_string_free(const char*);


struct vidio_fraction
{
  int numerator;
  int denominator;
};

LIBVIDIO_API double vidio_fraction_to_double(const struct vidio_fraction*);



// === Error Handling ===

// Many libvidio functions return a vidio_error pointer.
// On success, this pointer is NULL. On failure, the error object has to be released with vidio_error_free().
// NOTE: this means that you should never ignore this error because it will lead to a memory leak in case of error.

struct vidio_error;

enum vidio_error_code
{
  vidio_error_code_success = 0,
  vidio_error_code_other = 1,
  vidio_error_code_parameter_error = 2,
  vidio_error_code_usage_error = 3,
  vidio_error_code_internal_error = 4,
  vidio_error_code_errno = 5,
  vidio_error_code_cannot_open_camera = 6,
  vidio_error_code_cannot_query_device_capabilities = 7,
  vidio_error_code_cannot_set_camera_format = 8,
  vidio_error_code_cannot_alloc_capturing_buffers = 9,
  vidio_error_code_cannot_start_capturing = 10,
  vidio_error_code_error_while_capturing = 11,
  vidio_error_code_cannot_stop_capturing = 12,
  vidio_error_code_cannot_free_capturing_buffers = 13,

  // RTSP error codes (20-29)
  vidio_error_code_rtsp_connection_failed = 20,
  vidio_error_code_rtsp_authentication_failed = 21,
  vidio_error_code_rtsp_stream_not_found = 22,
  vidio_error_code_rtsp_connection_lost = 23,
  vidio_error_code_rtsp_timeout = 24,
  vidio_error_code_rtsp_unsupported_codec = 25
};

LIBVIDIO_API void vidio_error_free(const struct vidio_error*);

LIBVIDIO_API vidio_error_code vidio_error_get_code(const struct vidio_error*);

// free with vidio_string_free()
LIBVIDIO_API const char* vidio_error_get_message(const struct vidio_error*);

// free with vidio_string_free()
LIBVIDIO_API const char* vidio_error_get_message_template(const struct vidio_error*);

// free with vidio_string_free()
LIBVIDIO_API const char* vidio_error_get_argument(const struct vidio_error*, int n);

LIBVIDIO_API int vidio_error_get_number_of_arguments(const struct vidio_error*);

// Provide the deeper reason for this error. This may be a longer chain of errors.
// For example: "Cannot start capturing" because "Cannot open device" because "No access permission".
// If there is no deeper reason, NULL is returned.
LIBVIDIO_API const struct vidio_error* vidio_error_get_reason(const struct vidio_error*);



// === Video Frame ===

enum vidio_pixel_format
{
  vidio_pixel_format_undefined = 0,

  // RGB
  vidio_pixel_format_RGB8 = 1,
  vidio_pixel_format_RGB8_planar = 2,

  // YUV
  vidio_pixel_format_YUV420_planar = 100,
  vidio_pixel_format_YUV422_YUYV = 101,

  // Bayer
  vidio_pixel_format_RGGB8 = 200,

  // compressed
  vidio_pixel_format_MJPEG = 500,
  vidio_pixel_format_H264 = 501,
  vidio_pixel_format_H265 = 502
};

// A coarser classification of the pixel format. Used for informational purposes only.
enum vidio_pixel_format_class
{
  vidio_pixel_format_class_unknown = 0,
  vidio_pixel_format_class_RGB = 1,
  vidio_pixel_format_class_YUV = 2,
  vidio_pixel_format_class_MJPEG = 3,
  vidio_pixel_format_class_H264 = 4,
  vidio_pixel_format_class_H265 = 5
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


enum vidio_device_match
{
  vidio_device_match_none = 0,
  vidio_device_match_approx = 50,
  vidio_device_match_exact = 100
};


struct vidio_frame;

LIBVIDIO_API void vidio_frame_free(const struct vidio_frame*);

LIBVIDIO_API int vidio_frame_get_width(const struct vidio_frame*);

LIBVIDIO_API int vidio_frame_get_height(const struct vidio_frame*);

LIBVIDIO_API enum vidio_pixel_format vidio_frame_get_pixel_format(const struct vidio_frame*);

LIBVIDIO_API vidio_bool vidio_frame_has_color_plane(const struct vidio_frame*, enum vidio_color_channel);

LIBVIDIO_API uint8_t* vidio_frame_get_color_plane(vidio_frame*, enum vidio_color_channel, int* stride);

LIBVIDIO_API const uint8_t* vidio_frame_get_color_plane_readonly(const struct vidio_frame*, enum vidio_color_channel, int* stride);

LIBVIDIO_API uint64_t vidio_frame_get_timestamp_us(const struct vidio_frame*);


// === Format Conversion ===

// TODO: should return vidio_error. Should we remove this completely in favor of vidio_format_converter, or keep it as
//       a convenience wrapper around video_format_converter.
LIBVIDIO_API struct vidio_frame* vidio_frame_convert(const struct vidio_frame*, enum vidio_pixel_format);


struct vidio_format_converter;

// TODO: should return vidio_error if format conversion is not supported. Add conversion options?
LIBVIDIO_API struct vidio_format_converter* vidio_create_format_converter(enum vidio_pixel_format from, enum vidio_pixel_format to);

LIBVIDIO_API void vidio_format_converter_free(struct vidio_format_converter*);

// TODO: should return vidio_error
LIBVIDIO_API void vidio_format_converter_push_compressed(struct vidio_format_converter*, const struct vidio_frame*);

// Returns NULL when no more frames are available.
LIBVIDIO_API struct vidio_frame* vidio_format_converter_pull_decompressed(struct vidio_format_converter*);

// Convenience function to avoid the push/pull functions above. Note that this will not work when the input stream
// is compressed with frame reordering that can code a group of frames into one vidio_frame, or delay frames because of
// the reordering.
// Use this for simple RGB<->YUV conversions.
// TODO: should return vidio_error
LIBVIDIO_API struct vidio_frame* vidio_format_converter_convert_direct(struct vidio_format_converter*, const struct vidio_frame*);


// === Video Format ===

LIBVIDIO_API void vidio_video_format_free(const struct vidio_video_format*);

// Do not release the returned string.
LIBVIDIO_API const char* vidio_pixel_format_class_name(enum vidio_pixel_format_class format);

LIBVIDIO_API enum vidio_pixel_format vidio_video_format_get_pixel_format(const struct vidio_video_format* format);

struct vidio_video_format;

LIBVIDIO_API uint32_t vidio_video_format_get_width(const struct vidio_video_format* format);

LIBVIDIO_API uint32_t vidio_video_format_get_height(const struct vidio_video_format* format);

LIBVIDIO_API vidio_bool vidio_video_format_has_fixed_framerate(const struct vidio_video_format* format);

LIBVIDIO_API struct vidio_fraction vidio_video_format_get_framerate(const struct vidio_video_format* format);

LIBVIDIO_API enum vidio_pixel_format_class
vidio_video_format_get_pixel_format_class(const struct vidio_video_format* format);

LIBVIDIO_API const char* vidio_video_format_get_user_description(const struct vidio_video_format* format);

enum vidio_serialization_format
{
  vidio_serialization_format_unknown = 0,
  vidio_serialization_format_json = 1,
  vidio_serialization_format_keyvalue = 2
};

// TODO: would it make sense to add a vidio_input* to the serialize functions? Can the format always be independent from the device/input?
// -> We need the vidio_input parameter so that

// If JSON has not been compiled in, NULL is returned.
LIBVIDIO_API const char* vidio_video_format_serialize(const struct vidio_video_format* format, enum vidio_serialization_format);

LIBVIDIO_API const struct vidio_video_format* vidio_video_format_deserialize(const char* serializedString, enum vidio_serialization_format);

LIBVIDIO_API const struct vidio_video_format* vidio_video_format_find_best_match(const struct vidio_video_format* const* format_list, int nFormats,
                                                                                 const struct vidio_video_format* requested_format,
                                                                                 enum vidio_device_match* out_score);


/**
 * Get a list of video formats supported by this input.
 * The returned list and its entries must be released with `vidio_video_formats_free_list`.
 * The returned number of entries does not include the NULL termination.
 *
 * @param input The video input.
 * @param out_number The number of video formats returned. Optional, may be NULL.
 * @return Array of video formats, terminated by NULL entry.
 */
LIBVIDIO_API const struct vidio_video_format* const*
vidio_input_get_video_formats(const struct vidio_input* input, size_t* out_number);

/**
 * Free a list of video formats.
 * This function can either free the list together with all the formats contained in it
 * or just the list.
 *
 * @param format_list A list of video formats.
 * @param also_free_formats If set to 'false', only the list is released, but not the format objects listed.
 */
LIBVIDIO_API void vidio_video_formats_free_list(const struct vidio_video_format* const* format_list, int also_free_formats);


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
  vidio_input_source_Video4Linux2 = 1,
  vidio_input_source_RTSP = 2
};

enum vidio_rtsp_transport
{
  vidio_rtsp_transport_auto = 0,
  vidio_rtsp_transport_tcp = 1,
  vidio_rtsp_transport_udp = 2
};

enum vidio_input_message
{
  vidio_input_message_new_frame,
  vidio_input_message_end_of_stream,
  vidio_input_message_input_overflow
};

LIBVIDIO_API const vidio_error* vidio_list_input_devices(const struct vidio_input_device_filter*,
                                                         struct vidio_input_device* const** out_device_list, size_t* out_number);

LIBVIDIO_API void vidio_input_devices_free_list(const struct vidio_input_device* const* devices, int also_free_devices);

LIBVIDIO_API void vidio_input_device_free(const struct vidio_input_device* device);

// If JSON has not been compiled in, NULL is returned.
LIBVIDIO_API const char* vidio_input_serialize(const struct vidio_input* input, enum vidio_serialization_format);

/**
 * Find the input device from the list of devices that matches the serialized spec.
 * If the spec is a file input or generated input, the vidio_input is created from scratch (does not have to be in the input list).
 *
 * @param nDevices number of devices in 'devices'. If the input list is NULL-terminated, this can be set to '-1'.
 * @param serializedString
 * @return
 */
LIBVIDIO_API struct vidio_input* vidio_input_find_matching_device(struct vidio_input_device* const* devices, int nDevices,
                                                                  const char* serializedString,
                                                                  enum vidio_serialization_format,
                                                                  enum vidio_device_match* out_opt_matchscore);


LIBVIDIO_API const char* vidio_input_get_display_name(const struct vidio_input* input);

LIBVIDIO_API enum vidio_input_source vidio_input_get_source(const struct vidio_input* input);

struct vidio_output_format;

LIBVIDIO_API const vidio_error* vidio_input_configure_capture(struct vidio_input* input,
                                                              const struct vidio_video_format* requested_format,
                                                              const struct vidio_output_format*,
                                                              const struct vidio_video_format** out_actual_format);

struct vidio_frame;

// Note: do no heavy processing in the callback function as this runs in the main capturing loop.
LIBVIDIO_API void vidio_input_set_message_callback(struct vidio_input* input,
                                                   void (*)(enum vidio_input_message, void* userData), void* userData);

LIBVIDIO_API const struct vidio_error* vidio_input_start_capturing(struct vidio_input* input);

LIBVIDIO_API const struct vidio_error* vidio_input_stop_capturing(struct vidio_input* input);

// Do not free the returned frame. It will be released or reused in vidio_input_pop_next_frame().
LIBVIDIO_API const struct vidio_frame* vidio_input_peek_next_frame(struct vidio_input* input);

LIBVIDIO_API void vidio_input_pop_next_frame(struct vidio_input* input);

LIBVIDIO_API void vidio_input_release(struct vidio_input* input);


// === RTSP Input ===

/**
 * Create an RTSP input from a URL.
 * The URL format is: rtsp://[user:pass@]host[:port]/path
 *
 * @param url The RTSP URL to connect to.
 * @return A new vidio_input for RTSP streaming, or NULL if RTSP support is not compiled in.
 */
LIBVIDIO_API struct vidio_input* vidio_create_rtsp_input(const char* url);

/**
 * Create an RTSP input with separate credentials.
 * This keeps credentials out of the URL, avoiding accidental exposure
 * in logs or when the URL is displayed.
 *
 * @param url The RTSP URL to connect to (without credentials).
 * @param username The username for authentication (may be NULL).
 * @param password The password for authentication (may be NULL).
 * @return A new vidio_input for RTSP streaming, or NULL if RTSP support is not compiled in.
 */
LIBVIDIO_API struct vidio_input* vidio_create_rtsp_input_with_auth(const char* url,
                                                                    const char* username,
                                                                    const char* password);

/**
 * Set the RTSP transport protocol.
 * Must be called before starting capture.
 *
 * @param input The RTSP input.
 * @param transport The transport protocol to use (auto, tcp, or udp).
 */
LIBVIDIO_API void vidio_rtsp_set_transport(struct vidio_input* input, enum vidio_rtsp_transport transport);

/**
 * Set the connection timeout for RTSP operations.
 * Must be called before starting capture.
 *
 * @param input The RTSP input.
 * @param timeout_seconds Timeout in seconds (default is 10).
 */
LIBVIDIO_API void vidio_rtsp_set_timeout_seconds(struct vidio_input* input, int timeout_seconds);

}

#endif //LIBVIDIO_VIDIO_H
