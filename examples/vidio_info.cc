/*
 * VidIO library
 * Copyright (c) 2023 Dirk Farin <dirk.farin@gmail.com>
 *
 * This file is part of libvidio.
 *
 * libvidio is free software: you can redistribute it and/or modify
 * it under the terms of the GNU Lesser General Public License as
 * published by the Free Software Foundation, either version 3 of
 * the License, or (at your option) any later version.
 *
 * libvidio is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with libvidio.  If not, see <http://www.gnu.org/licenses/>.
 */

#include "libvidio/vidio.h"
#include <stdio.h>

int main(int argc, char** argv)
{
  printf("vidio_version: %s\n", vidio_get_version());

  const vidio_input_device** devices;
  auto n = vidio_list_input_devices(&devices, nullptr);
  for (size_t i=0;i<n;i++) {
    auto name = vidio_input_get_display_name((vidio_input*)devices[i]);
    printf("> %s\n", name);
    vidio_free_string(name);
  }

  return 0;
}