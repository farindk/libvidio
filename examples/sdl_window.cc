//
// Created by farindk on 18.01.24.
//

#include "sdl_window.h"
#include <cstring>
#include <cassert>


void sdl_window::show_image(const vidio_frame* frame)
{
  if (!mWindow) {
    open(frame);
  }

  vidio_format_converter_push_compressed(m_converter, frame);

  while (const auto* rgbFrame = vidio_format_converter_pull_decompressed(m_converter)) {
    uint8_t* pixels = nullptr;
    int stride = 0;

    if (SDL_LockTexture(mTexture, nullptr,
                        reinterpret_cast<void**>(&pixels), &stride) < 0)
      return;

    int width = vidio_frame_get_width(frame);
    int height = vidio_frame_get_height(frame);

    int inStride;
    const uint8_t* inPixels = vidio_frame_get_color_plane_readonly(rgbFrame, vidio_color_channel_interleaved, &inStride);

    for (int y = 0; y < height; y++) {
      memcpy(pixels + y * stride, inPixels + inStride * y, width * 3);
    }

    SDL_UnlockTexture(mTexture);

    SDL_RenderCopy(mRenderer, mTexture, nullptr, nullptr);
    SDL_RenderPresent(mRenderer);

    vidio_frame_free(rgbFrame);
  }
}

void sdl_window::open(const vidio_frame* frame)
{
  assert(mWindow == nullptr);

  if (SDL_Init(SDL_INIT_VIDEO) < 0) {
    printf("SDL_Init() failed: %s\n", SDL_GetError());
    SDL_Quit();
    return;
  }

  int width, height;
  width = vidio_frame_get_width(frame);
  height = vidio_frame_get_width(frame);

  const char* window_title = "Live camera view";
  mWindow = SDL_CreateWindow(window_title,
                             SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED,
                             width, height, 0);
  if (!mWindow) {
    printf("SDL: Couldn't set video mode to %dx%d: %s\n",
           width, height, SDL_GetError());
    SDL_Quit();
    return;
  }

  Uint32 flags = 0;  // Empty flags prioritize SDL_RENDERER_ACCELERATED.
  mRenderer = SDL_CreateRenderer(mWindow, -1, flags);
  if (!mRenderer) {
    printf("SDL: Couldn't create renderer: %s\n", SDL_GetError());
    SDL_Quit();
    return;
  }

  Uint32 pixelFormat = SDL_PIXELFORMAT_RGB24;
  mTexture = SDL_CreateTexture(mRenderer, pixelFormat,
                               SDL_TEXTUREACCESS_STREAMING, width, height);
  if (!mTexture) {
    printf("SDL: Couldn't create SDL texture: %s\n", SDL_GetError());
    SDL_Quit();
    return;
  }

  mRect.x = 0;
  mRect.y = 0;
  mRect.w = width;
  mRect.h = height;

  m_converter = vidio_create_format_converter(vidio_frame_get_pixel_format(frame),
                                              vidio_pixel_format_RGB8);
}

void sdl_window::close()
{
  if (!mWindow)
    return;

  if (mTexture) {
    SDL_DestroyTexture(mTexture);
    mTexture = nullptr;
  }
  if (mRenderer) {
    SDL_DestroyRenderer(mRenderer);
    mRenderer = nullptr;
  }
  if (mWindow) {
    SDL_DestroyWindow(mWindow);
    mWindow = nullptr;
  }

  SDL_Quit();
}


bool sdl_window::check_close_button()
{
  SDL_Event event;

  while (SDL_PollEvent(&event)) {
    switch (event.type) {

      case SDL_KEYDOWN:
        switch (event.key.keysym.sym) {
          case SDLK_ESCAPE:
            return true;
            break;
        }
        break;

      case SDL_WINDOWEVENT:
        switch (event.window.event) {
          case SDL_WINDOWEVENT_CLOSE:
            return true;
            break;

          default:
            break;
        }
        break;
    }
  }

  return false;
}
