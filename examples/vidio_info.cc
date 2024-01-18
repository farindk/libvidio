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
#include "libvidio/vidio_capturing_loop.h"
#include <cstdio>
#include <iostream>
#include <iomanip>
#include <getopt.h>
#include <sstream>
#include <optional>

#if WITH_SDL
#include "sdl_window.h"
#endif

vidio_format_converter* converter = nullptr;
vidio_capturing_loop capturingLoop;

#if WITH_SDL
sdl_window sdlWindow;
#endif

static int cnt = 1;

int option_camera = -1;
int option_format = -1;
int option_show = 0;
int option_help = 0;
int option_num_frames = 100;
std::string option_output_directory;  // if empty, nothing is saved


static struct option long_options[] = {
    {"help", no_argument, &option_help, 'h'},
#if WITH_SDL
    {"show",       no_argument,       &option_show, 's'},
#endif
    {"output", required_argument, nullptr, 'o'},
    {"camera", required_argument, nullptr, 'c'},
    {"format", required_argument, nullptr, 'f'},
    {"num-frames", required_argument, nullptr, 'n'},
    {nullptr, 0, nullptr, 0}
};


void show_help(std::ostream& ostr)
{
  ostr << " vidio-info    v" << vidio_get_version() << "\n"
       << "------------------------\n"
       << "usage: vidio-info [options]\n"
       << "\n"
       << "options:\n"
       << "  -c, --camera #       number of camera\n"
       << "  -f, --format #       number of format\n"
       #if WITH_SDL
       << "  -s, --show           show live image in window\n"
       #endif
       << "  -o, --output DIR     save captured images to directory DIR\n"
       << "  -n, --num-frames #   number of frames to save (default: 100)\n"
       << "  -h, --help           usage help\n";
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

  if (!option_output_directory.empty() && cnt <= option_num_frames) {

    vidio_format_converter_push_compressed(converter, frame);

    int nDigits = num_digits(option_num_frames);

    while (vidio_frame* rgbFrame = vidio_format_converter_pull_decompressed(converter)) {

      std::stringstream outputFile;
      outputFile << option_output_directory << "/" << "image"
                 << std::setw(nDigits) << std::setfill('0') << cnt << ".ppm";

      // write PPM image

      FILE* fh = fopen(outputFile.str().c_str(), "wb");
      fprintf(fh, "P6\n%d %d\n255\n", vidio_frame_get_width(rgbFrame), vidio_frame_get_height(rgbFrame));
      const uint8_t* data;
      int stride;
      data = vidio_frame_get_color_plane_readonly(rgbFrame, vidio_color_channel_interleaved, &stride);
      for (int y = 0; y < vidio_frame_get_height(rgbFrame); y++) {
        fwrite(data + y * stride, 1, stride, fh);
      }
      fclose(fh);

      std::cout << "save frame " << cnt << "\r";
      std::cout.flush();

      vidio_frame_free(rgbFrame);

      cnt++;
      if (cnt > option_num_frames) {
        std::cout << "\n";

        if (!option_show) {
          capturingLoop.stop();
        }
      }
    }
  }
}


void message_callback(vidio_input_message msg)
{
  if (msg == vidio_input_message_input_overflow) {
    printf("WARNING: buffer overflow\n");
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
    std::cerr << "because: ";
    show_err(reason, false);
  }

  if (release) {
    vidio_error_free(err);
  }
}

std::string camera_name(const vidio_input_device* device)
{
  auto cameraName = vidio_input_get_display_name((vidio_input*) device);
  std::string name = cameraName;
  vidio_string_free(cameraName);

  return name;
}

std::string format_name(const vidio_video_format* format)
{
  std::stringstream sstr;

  const char* formatname = vidio_video_format_get_user_description(format);
  std::optional<vidio_fraction> framerate;
  if (vidio_video_format_has_fixed_framerate(format)) {
    framerate = vidio_video_format_get_framerate(format);
  }

  sstr << formatname << " "
       << vidio_video_format_get_width(format) << "x"
       << vidio_video_format_get_height(format);
  if (framerate) {
    sstr << " @ " << vidio_fraction_to_double(&*framerate);
  }

  vidio_string_free(formatname);

  return sstr.str();
}

int main(int argc, char** argv)
{
  while (true) {
    int option_index = 0;
    int c = getopt_long(argc, argv, "hn:o:f:c:"
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
      case 'n':
        option_num_frames = atoi(optarg);
        break;
      case 'o':
        option_output_directory = optarg;
        break;
      case 's':
        option_show = true;
        break;
      case 'f':
        option_format = atoi(optarg);
        break;
      case 'c':
        option_camera = atoi(optarg);
        break;
    }
  }

  const vidio_video_format* selected_format = nullptr;
  const vidio_input_device* selected_device = nullptr;
  const vidio_error* err = nullptr;

  vidio_input_device* const* devices;
  size_t numDevices;
  err = vidio_list_input_devices(nullptr, &devices, &numDevices);
  if (err) {
    show_err(err);
    return 10;
  }

  if (option_camera < 0 || option_camera >= (int) numDevices) {
    std::cout << "List of cameras:\n";
    for (size_t i = 0; devices[i]; i++) {
      std::cout << "  [" << i << "] - " << camera_name(devices[i]) << "\n";
    }

    std::cout << "\nSelect the camera on subsequent calls with '-c NUM'\n";

    vidio_input_devices_free_list(devices, true);
    return 0;
  }


  selected_device = devices[option_camera];


  size_t nFormats;
  const struct vidio_video_format* const* formats = vidio_input_get_video_formats((vidio_input*) selected_device,
                                                                                  &nFormats);

  if (option_format < 0 || option_format >= (int) nFormats) {

    std::cout << "List of formats for camera '" << camera_name(selected_device) << "':\n";

    for (size_t f = 0; f < nFormats; f++) {
      std::cout << "  [" << f << "] - " << format_name(formats[f]) << "\n";
    }

    std::cout << "\nSelect the capture format on subsequent calls with '-f NUM'\n";
    return 0;
  }

  selected_format = formats[option_format];


  if (!option_show && option_output_directory.empty()) {
#if WITH_SDL
    option_show = true; // fallback to show
#else
    option_output_directory = ".";
#endif
  }


  // --- show selected camera and format

  std::cout << "camera: " << camera_name(selected_device) << "\n";
  std::cout << "format: " << format_name(selected_format) << "\n";


  // prepare capturing format

  const vidio_video_format* actual_format = nullptr; // vidio_video_format_clone(formats[0]);
  err = vidio_input_configure_capture((vidio_input*) selected_device, selected_format, nullptr,
                                      &actual_format);
  if (err) {
    show_err(err);
    return 10;
  }

  // prepare converter (for PPM output)

  if (!option_output_directory.empty()) {
    converter = vidio_create_format_converter(vidio_video_format_get_pixel_format(selected_format),
                                              vidio_pixel_format_RGB8);
  }

  // do capturing

  vidio_video_formats_free_list(formats, true);

  capturingLoop.set_on_frame_received(output_frame);
  capturingLoop.set_on_stream_message(message_callback);

  err = capturingLoop.start_with_vidio_input((vidio_input*) selected_device, vidio_capturing_loop::run_mode::sync);
  show_err(err);

  // cleanup

  vidio_input_devices_free_list(devices, true);
  vidio_format_converter_free(converter);

#if WITH_SDL
  sdlWindow.close();
#endif

  return 0;
}