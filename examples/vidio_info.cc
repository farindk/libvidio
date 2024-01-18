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
#include <cstdio>
#include <iostream>
#include <mutex>
#include <condition_variable>
#include <unistd.h>


vidio_format_converter* converter = nullptr;

static int cnt = 1;

bool save_frame(const vidio_frame* frame)
{
  std::cout << "format: ";
  switch (vidio_frame_get_pixel_format(frame)) {
    case vidio_pixel_format_YUV422_YUYV:
      std::cout << "YUYV ";
      break;
    case vidio_pixel_format_MJPEG:
      std::cout << "MJPEG ";
      break;
    case vidio_pixel_format_H264:
      std::cout << "H264 ";
      break;
    default:
      std::cout << "unknown ";
      break;
  }

  std::cout << vidio_frame_get_width(frame) << " x " << vidio_frame_get_height(frame) << "\n";

  vidio_format_converter_push_compressed(converter, frame);

  for (;;) {
    vidio_frame* rgbFrame = vidio_format_converter_pull_decompressed(converter);
    if (rgbFrame == nullptr)
      break;

#if 1
    char buf[100];
    sprintf(buf, "/home/farindk/out%04d.ppm", cnt);
    FILE* fh = fopen(buf, "wb");
    fprintf(fh, "P6\n%d %d\n255\n", vidio_frame_get_width(rgbFrame), vidio_frame_get_height(rgbFrame));
    const uint8_t* data;
    int stride;
    data = vidio_frame_get_color_plane_readonly(rgbFrame, vidio_color_channel_interleaved, &stride);
    for (int y = 0; y < vidio_frame_get_height(rgbFrame); y++) {
      fwrite(data + y * stride, 1, stride, fh);
    }
    fclose(fh);

#endif
    printf("save %d\n", cnt);
    cnt++;

    vidio_frame_free(rgbFrame);
  }

  //vidio_frame_release(frame);

  return cnt >= 150;
}


class StorageProcess
{
public:
  explicit StorageProcess(vidio_input* i) : m_input(i) {}

  void run()
  {
    while (!m_quit) {
      {
        std::unique_lock<std::mutex> lock(mutex);
        while (vidio_input_peek_next_frame(m_input) == nullptr && !m_quit) {
          cond.wait(lock);
        }
      }

      while (const vidio_frame* frame = vidio_input_peek_next_frame(m_input)) {
        bool end = save_frame(frame);
        vidio_input_pop_next_frame(m_input);

        if (end) {
          auto* err = vidio_input_stop_capturing(m_input);
          vidio_error_free(err);
        }
      }
    }
  }

  void frame_arrived()
  {
    std::unique_lock<std::mutex> lock(mutex);
    cond.notify_one();
  }

  void stop()
  {
    std::unique_lock<std::mutex> lock(mutex);
    m_quit = true;
    cond.notify_one();
  }

private:
  vidio_input* m_input;
  bool m_quit = false;

  std::mutex mutex;
  std::condition_variable cond;
};


void message_callback(vidio_input_message msg, void* userData)
{
  auto* storage = (StorageProcess*) userData;

  if (msg == vidio_input_message_new_frame) {
    storage->frame_arrived();
  }
  else if (msg == vidio_input_message_end_of_stream) {
    printf("================ STOP =================\n");
    storage->stop();
  }
  else if (msg == vidio_input_message_input_overflow) {
    printf("OVERFLOW\n");
  }
}


void show_err(const vidio_error* err, bool release = true)
{
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


int main(int argc, char** argv)
{
  printf("vidio_version: %s\n", vidio_get_version());

  const vidio_video_format* selected_format = nullptr;
  const vidio_input_device* selected_device = nullptr;
  const vidio_error* err = nullptr;

  vidio_input_device* const* devices;
  err = vidio_list_input_devices(nullptr, &devices, nullptr);
  if (err) {
    show_err(err);
    return 10;
  }

  for (size_t i = 0; devices[i]; i++) {
    auto name = vidio_input_get_display_name((vidio_input*) devices[i]);
    printf("> %s\n", name);
    vidio_string_free(name);

    size_t nFormats;
    const struct vidio_video_format* const* formats = vidio_input_get_video_formats((vidio_input*) devices[i],
                                                                                    &nFormats);
    for (size_t f = 0; f < nFormats; f++) {
      vidio_fraction framerate = vidio_video_format_get_framerate(formats[f]);
      const char* formatname = vidio_video_format_get_user_description(formats[f]);

      std::cout << vidio_pixel_format_class_name(vidio_video_format_get_pixel_format_class(formats[f])) << " "
                << formatname << " "
                << vidio_video_format_get_width(formats[f]) << "x"
                << vidio_video_format_get_height(formats[f])
                << " @ " << vidio_fraction_to_double(&framerate) << "\n";

      vidio_string_free(formatname);
    }

    size_t idx = 0;
    for (; idx < nFormats; idx++) {
      if (true /*vidio_video_format_get_pixel_format_class(formats[idx]) == vidio_pixel_format_class_YUV &&
          vidio_video_format_get_width(formats[idx]) == 640*/) {
        selected_format = formats[idx];
        selected_device = devices[i];

        const vidio_video_format* actual_format = nullptr; // vidio_video_format_clone(formats[0]);
        err = vidio_input_configure_capture((vidio_input*) selected_device, selected_format, nullptr,
                                            &actual_format);
        if (err) {
          show_err(err);
          return 10;
        }

        converter = vidio_create_converter(vidio_video_format_get_pixel_format(selected_format),
                                           vidio_pixel_format_RGB8);

        const char* devStr = vidio_input_serialize((vidio_input*) selected_device, vidio_serialization_format_json);
        std::cout << "DEVICE:\n" << devStr << "\n";
        vidio_string_free(devStr);

        const char* fmtStr = vidio_video_format_serialize(selected_format, vidio_serialization_format_json);
        std::cout << "FORMAT:\n" << fmtStr << "\n";
        vidio_string_free(fmtStr);

        vidio_video_formats_free_list(formats, true);
        break;
      }
    }

    if (selected_format != nullptr)
      break;
  }


  auto* selected_input = (vidio_input*) selected_device;
  StorageProcess storage(selected_input);
  vidio_input_set_message_callback(selected_input, message_callback, &storage);
  err = vidio_input_start_capturing(selected_input);
  vidio_error_free(err);

  storage.run();

  vidio_input_devices_free_list(devices, true);
  vidio_format_converter_free(converter);

  return 0;
}