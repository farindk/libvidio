
# libvidio - Video Capturing Library

libvidio provides a simple to use C interface for video input.
Currently, it supports V4L2 Linux cameras, but the plan is to extend this to other systems and camera interfaces.

libvidio also handles decoding compressed input video (MJPEG, H264) and converting the numerous color spaces into
something that the application can work with.

It includes a simple test program `vidio-grab` to show connected cameras and the formats they provide.
It can also show live video and capture to image files.

# API

While libvidio itself is written in C++, it has a pure C interface, which is a very thin wrapper around the C++ classes.
This ensures ABI backwards compatibility and easy integration into other languages.

## Step by step tutorial

### Get connected cameras

First, we need to get a list of connected cameras:
````c++
  vidio_input_device* const* devices;
  size_t numDevices;
  const vidio_error* err = vidio_list_input_devices(nullptr, &devices, &numDevices);
  if (err) {
    const char* msg = vidio_error_get_message(err);
    ...
    vidio_string_free(msg);
    vidio_error_free(err);
  }
````

This allocates a list of pointers to the `vidio_input_device` objects. This list is NULL-terminated.
The `numDevices` argument can be omitted (set to NULL) if you don't need it.

Later, we will free the list of devices with
````c++
  vidio_input_devices_free_list(devices, true);
````

### Error handling concepts

Many functions return a `vidio_error` on error. On success, NULL is returned.
You **have to** free this error with `vidio_error_free()` even if you want to ignore it (which is not a good idea anyway).
It is legal to call `vidio_error_free()` with a NULL pointer. Thus, simply calling this every time is legal.

Errors have an error code, but also a text message. Since some errors provide additional information (like the camera device),
the text message can also be obtained as a template string with placeholders for the parameters ('{0}', '{1}', ...).
This string may be translated.

### Get list of video formats the camera supports

This is similar to getting the list of cameras:
````c++
  const struct vidio_video_format* const* formats;
  formats = vidio_input_get_video_formats((vidio_input*) selected_device, nullptr);
````
The list is also NULL-terminated. We omitted the list size in this example for a change.

### Set capturing format

From the list of supported formats, we can select one
````c++
vidio_input_device* selected_device = devices[...];
vidio_video_format* selected_format = formats[...];
err = vidio_input_configure_capture((vidio_input*) selected_device, selected_format, nullptr, nullptr);
````

Note that the `vidio_input_device` is casted to `vidio_input`.
This is because there is a *class* hierarchy of input devices. As `vidio_input_device` is derived from `vidio_input`,
it can be used at all places where a `vidio_input` argument is needed.

### Start capturing (C interface)

Before we start the camera, we define a callback function that will be notified when there are new frames,
some error occurs, or the end of stream is reached (unlikely for cameras, but possible with future file inputs). 

````c++
void on_vidio_message(vidio_input_message msg, void* userData)
{
  if (msg == vidio_input_message_new_frame) {
    ...
  }
}

vidio_input_set_message_callback(input, on_vidio_message, userPtr);
````

Now, we can finally start the camera:
````c++
err = vidio_input_start_capturing((vidio_input*)selected_device);
````

This function is non-blocking. It will start the capturing in a background process.

The work done in the callback should be as fast as possible as this runs in the main capturing loop.
That means that you should not process the captured images in this callback, but get and process the frames
in a separate thread.

When a `vidio_input_message_new_frame` message is received, you can get the frames that are currently queued up
in the `vidio_input` like this:
````c++
while (const vidio_frame* frame = vidio_input_peek_next_frame(input)) {
  // process image
  ...
  
  vidio_input_pop_next_frame(m_input);
}
````

### Start capturing (c++ interface)

As extracting the frames from the `vidio_input` should run in a separate thread and this is non-trivial boilerplate code,
there is a convenience class implemented in the header-only C++ class `vidio_capturing_loop`.
This class runs a separate thread that retrieves frames from the camera and calls a callback function for each frame.

````c++
vidio_capturing_loop capturingLoop;

capturingLoop.set_on_frame_received([](const vidio_frame* frame) {
  // do processing
});

err = capturingLoop.start_with_vidio_input(input, vidio_capturing_loop::run_mode::async);
````

With the `run_mode` argument, you can decide whether the capturing loop should be blocking (until the stream ends or
streaming is stopped explicitly) or run non-blocking in a separate thread.

converter = vidio_create_format_converter(vidio_video_format_get_pixel_format(selected_format),
vidio_pixel_format_RGB8);
}

### Format conversion

Cameras can output video in many different formats.
To simplify this for the application, libvidio can convert the camera format to a format the application can work with.

First, create a converter object to convert between two pixel formats:
````c++
vidio_format_converter* converter = vidio_create_format_converter(vidio_video_format_get_pixel_format(selected_format),
                                                                  vidio_pixel_format_RGB8);
````

Then, you can push input frames into the converter and retrieve converted frames:
````c++
    vidio_format_converter_push_compressed(converter, frame);

    while (vidio_frame* rgbFrame = vidio_format_converter_pull_decompressed(converter)) {
      // process rgbFrame
      ...
      
      vidio_frame_free(rgbFrame);
    }
````

Since the input can be compressed frames (H.264) there is no simple 1:1 relation between the input and output and
we need a loop to get all available decoded frames.

However, if we are sure that we are only converting simple pixel images (like YUV to RGB), we can also use
````c++
struct vidio_frame* vidio_format_converter_convert_direct(struct vidio_format_converter*,
                                                          const struct vidio_frame* input);
````

# Compiling

This library uses the CMake build system.
You will need the ffmpeg libraries as a dependency.
The `vidio-grab` example tool additionally uses SDL2 for display.

Build the library with these steps:
````shell
mkdir build
cd build
cmake .. -DCMAKE_INSTALL_PREFIX=<?>
make && make install
````

# Example program

The `vidio-grab` example is a simple tool to list the connected cameras, show the supported formats
and show the live video or save the images to disk.

# License

libvidio is distributed under the terms of the GNU General Public License (GPL).
It is also available with a commercial license for closed source applications. Contact me for details.

Copyright (c) 2023-2024 Dirk Farin
Contact: Dirk Farin dirk.farin@gmail.com
