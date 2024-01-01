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

int main(int argc, char** argv)
{
  printf("vidio_version: %s\n", vidio_get_version());

  const vidio_input_device* const* devices = vidio_list_input_devices(nullptr, nullptr);
  for (size_t i = 0; devices[i]; i++) {
    auto name = vidio_input_get_display_name((vidio_input*) devices[i]);
    printf("> %s\n", name);
    vidio_free_string(name);

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

      vidio_free_string(formatname);
    }
    vidio_video_formats_free_list(formats);
  }

  vidio_input_devices_free_list(devices, true);

  return 0;
}