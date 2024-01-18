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

#ifndef LIBVIDIO_SDL_WINDOW_H
#define LIBVIDIO_SDL_WINDOW_H

#include <libvidio/vidio.h>
#include <SDL.h>

class sdl_window
{
public:
  void show_image(const vidio_frame* frame);

  void close();

  bool check_close_button();

private:
  SDL_Window *mWindow = nullptr;
  SDL_Renderer *mRenderer = nullptr;
  SDL_Texture *mTexture = nullptr;
  SDL_Rect     mRect;

  vidio_format_converter* m_converter;

  void open(const vidio_frame* frame);
};


#endif //LIBVIDIO_SDL_WINDOW_H
