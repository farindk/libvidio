/*
 * VidIO library
 * Copyright (c) 2026 Dirk Farin <dirk.farin@gmail.com>
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
#include "libvidio/vidio_capturing_loop.h"
#include <cstdio>
#include <iostream>
#include <iomanip>
#include <getopt.h>
#include <sstream>
#include <cstring>

#if WITH_SDL
#include "sdl_window.h"
#endif

vidio_format_converter* converter = nullptr;
vidio_capturing_loop capturingLoop;

#if WITH_SDL
sdl_window sdlWindow;
#endif

static int frame_count = 0;

std::string option_url;
std::string option_username;
std::string option_password;
std::string option_transport;
int option_timeout = 10;
int option_show = 0;
int option_help = 0;
int option_num_frames = 100;
std::string option_output_directory;


static struct option long_options[] = {
    {"help",      no_argument,       &option_help, 'h'},
#if WITH_SDL
    {"show",      no_argument,       &option_show, 's'},
#endif
    {"username",  required_argument, nullptr, 'u'},
    {"password",  required_argument, nullptr, 'p'},
    {"transport", required_argument, nullptr, 't'},
    {"timeout",   required_argument, nullptr, 'T'},
    {"output",    required_argument, nullptr, 'o'},
    {"num-frames", required_argument, nullptr, 'n'},
    {nullptr, 0, nullptr, 0}
};


void show_help(std::ostream& ostr)
{
  ostr << " vidio-rtsp-grab    v" << vidio_get_version() << "\n"
       << "---------------------------\n"
       << "usage: vidio-rtsp-grab [options] <rtsp-url>\n"
       << "\n"
       << "options:\n"
       << "  -u, --username USER  username for authentication\n"
       << "  -p, --password PASS  password for authentication\n"
       << "  -t, --transport MODE transport protocol: tcp, udp, or auto (default: auto)\n"
       << "  -T, --timeout SEC    connection timeout in seconds (default: 10)\n"
#if WITH_SDL
       << "  -s, --show           show live image in window (default if no -o)\n"
#endif
       << "  -o, --output DIR     save captured frames to directory DIR\n"
       << "  -n, --num-frames #   number of frames to save (default: 100)\n"
       << "  -h, --help           show this help\n"
       << "\n"
       << "examples:\n"
       << "  vidio-rtsp-grab rtsp://192.168.1.100:554/stream1\n"
       << "  vidio-rtsp-grab -u admin -p secret rtsp://camera.local/live\n"
       << "  vidio-rtsp-grab -t tcp --timeout 15 rtsp://camera/stream\n";
}


int num_digits(int maxValue)
{
  int n = 1;
  int maxi = 10;
  while (maxi - 1 < maxValue) {
    n++;
    maxi *= 10;
  }
  return n;
}


void output_frame(const vidio_frame* frame)
{
#if WITH_SDL
  if (option_show) {
    sdlWindow.show_image(frame);

    if (sdlWindow.check_close_button()) {
      capturingLoop.stop();
    }
  }
#endif

  if (!option_output_directory.empty() && frame_count < option_num_frames) {

    vidio_format_converter_push_compressed(converter, frame);

    int nDigits = num_digits(option_num_frames);

    while (vidio_frame* rgbFrame = vidio_format_converter_pull_decompressed(converter)) {

      std::stringstream outputFile;
      outputFile << option_output_directory << "/" << "frame"
                 << std::setw(nDigits) << std::setfill('0') << frame_count << ".ppm";

      FILE* fh = fopen(outputFile.str().c_str(), "wb");
      if (fh) {
        fprintf(fh, "P6\n%d %d\n255\n", vidio_frame_get_width(rgbFrame), vidio_frame_get_height(rgbFrame));
        const uint8_t* data;
        int stride;
        data = vidio_frame_get_color_plane_readonly(rgbFrame, vidio_color_channel_interleaved, &stride);
        for (int y = 0; y < vidio_frame_get_height(rgbFrame); y++) {
          fwrite(data + y * stride, 1, static_cast<size_t>(vidio_frame_get_width(rgbFrame) * 3), fh);
        }
        fclose(fh);

        std::cout << "saved frame " << frame_count << "\r";
        std::cout.flush();
      }

      vidio_frame_free(rgbFrame);

      frame_count++;
      if (frame_count >= option_num_frames) {
        std::cout << "\n";

        if (!option_show) {
          capturingLoop.stop();
        }
      }
    }
  }
  else {
    frame_count++;
  }
}


void message_callback(vidio_input_message msg)
{
  switch (msg) {
    case vidio_input_message_input_overflow:
      std::cerr << "WARNING: buffer overflow - frames being dropped\n";
      break;
    case vidio_input_message_end_of_stream:
      std::cout << "Stream ended\n";
      break;
    default:
      break;
  }
}


void show_err(const vidio_error* err, bool release = true)
{
  if (!err) {
    return;
  }

  const char* msg = vidio_error_get_message(err);
  std::cerr << "ERROR: " << msg << "\n";
  vidio_string_free(msg);

  const auto* reason = vidio_error_get_reason(err);
  if (reason) {
    std::cerr << "  caused by: ";
    show_err(reason, false);
  }

  if (release) {
    vidio_error_free(err);
  }
}


int main(int argc, char** argv)
{
  while (true) {
    int option_index = 0;
    int c = getopt_long(argc, argv, "hu:p:t:T:o:n:"
#if WITH_SDL
        "s"
#endif
        , long_options, &option_index);
    if (c == -1) {
      break;
    }

    switch (c) {
      case 'h':
        show_help(std::cout);
        return 0;
      case 'u':
        option_username = optarg;
        break;
      case 'p':
        option_password = optarg;
        break;
      case 't':
        option_transport = optarg;
        break;
      case 'T':
        option_timeout = atoi(optarg);
        break;
      case 'o':
        option_output_directory = optarg;
        break;
      case 'n':
        option_num_frames = atoi(optarg);
        break;
#if WITH_SDL
      case 's':
        option_show = true;
        break;
#endif
      case 0:
        // Handle flag options set via long_options
        if (option_help) {
          show_help(std::cout);
          return 0;
        }
        break;
      default:
        show_help(std::cerr);
        return 1;
    }
  }

  // Get URL from remaining arguments
  if (optind < argc) {
    option_url = argv[optind];
  }

  if (option_url.empty()) {
    std::cerr << "Error: RTSP URL is required\n\n";
    show_help(std::cerr);
    return 1;
  }

  // Default to showing if no output directory
  if (!option_show && option_output_directory.empty()) {
#if WITH_SDL
    option_show = true;
#else
    option_output_directory = ".";
#endif
  }

  // Create RTSP input
  vidio_input* rtsp_input;
  if (!option_username.empty()) {
    rtsp_input = vidio_create_rtsp_input_with_auth(option_url.c_str(),
                                                    option_username.c_str(),
                                                    option_password.c_str());
  }
  else {
    rtsp_input = vidio_create_rtsp_input(option_url.c_str());
  }

  if (!rtsp_input) {
    std::cerr << "Error: Failed to create RTSP input. RTSP support may not be compiled in.\n";
    return 1;
  }

  // Set transport
  if (!option_transport.empty()) {
    vidio_rtsp_transport transport = vidio_rtsp_transport_auto;
    if (option_transport == "tcp") {
      transport = vidio_rtsp_transport_tcp;
    }
    else if (option_transport == "udp") {
      transport = vidio_rtsp_transport_udp;
    }
    else if (option_transport != "auto") {
      std::cerr << "Warning: Unknown transport '" << option_transport << "', using auto\n";
    }
    vidio_rtsp_set_transport(rtsp_input, transport);
  }

  // Set timeout
  vidio_rtsp_set_timeout_seconds(rtsp_input, option_timeout);

  std::cout << "Connecting to: " << option_url << "\n";
  if (!option_username.empty()) {
    std::cout << "  Username: " << option_username << "\n";
  }
  std::cout << "  Transport: " << (option_transport.empty() ? "auto" : option_transport) << "\n";
  std::cout << "  Timeout: " << option_timeout << " s\n";

  // Configure capture (this will connect and get format info)
  const vidio_video_format* actual_format = nullptr;
  const vidio_error* err = vidio_input_configure_capture(rtsp_input, nullptr, nullptr, &actual_format);
  if (err) {
    show_err(err);
    vidio_input_release(rtsp_input);
    return 1;
  }

  if (actual_format) {
    // Output stream details
    std::cout << "\nStream information:\n";
    std::cout << "  Resolution: " << vidio_video_format_get_width(actual_format)
              << "x" << vidio_video_format_get_height(actual_format) << "\n";

    // Output codec/compression format
    const char* codec_name;
    switch (vidio_video_format_get_pixel_format_class(actual_format)) {
      case vidio_pixel_format_class_H264:
        codec_name = "H.264";
        break;
      case vidio_pixel_format_class_H265:
        codec_name = "H.265/HEVC";
        break;
      case vidio_pixel_format_class_MJPEG:
        codec_name = "MJPEG";
        break;
      case vidio_pixel_format_class_YUV:
        codec_name = "YUV (uncompressed)";
        break;
      case vidio_pixel_format_class_RGB:
        codec_name = "RGB (uncompressed)";
        break;
      default:
        codec_name = "Unknown";
        break;
    }
    std::cout << "  Codec: " << codec_name << "\n";

    // Output frame rate if available
    if (vidio_video_format_has_fixed_framerate(actual_format)) {
      vidio_fraction fr = vidio_video_format_get_framerate(actual_format);
      double fps = vidio_fraction_to_double(&fr);
      std::cout << "  Frame rate: " << fps << " fps";
      if (fr.denominator != 1) {
        std::cout << " (" << fr.numerator << "/" << fr.denominator << ")";
      }
      std::cout << "\n";
    }
    else {
      std::cout << "  Frame rate: variable\n";
    }

    std::cout << "\n";

    // Create converter for output
    if (!option_output_directory.empty()) {
      converter = vidio_create_format_converter(vidio_video_format_get_pixel_format(actual_format),
                                                vidio_pixel_format_RGB8);
    }

    vidio_video_format_free(actual_format);
  }

  std::cout << "Starting capture...\n";

  // Start capturing
  capturingLoop.set_on_frame_received(output_frame);
  capturingLoop.set_on_stream_message(message_callback);

  err = capturingLoop.start_with_vidio_input(rtsp_input, vidio_capturing_loop::run_mode::sync);
  if (err) {
    show_err(err);
  }

  std::cout << "Total frames received: " << frame_count << "\n";

  // Cleanup
  vidio_input_release(rtsp_input);
  vidio_format_converter_free(converter);

#if WITH_SDL
  sdlWindow.close();
#endif

  return 0;
}
