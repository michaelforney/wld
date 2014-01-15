/* wld: wayland-private.h
 *
 * Copyright (c) 2013, 2014 Michael Forney
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */

#ifndef WLD_WAYLAND_PRIVATE_H
#define WLD_WAYLAND_PRIVATE_H

#include "wld.h"

struct wl_display;
struct wl_event_queue;
struct wl_buffer;

struct wld_wayland_interface
{
    struct wld_context * (* create_context)(struct wl_display * display,
                                            struct wl_event_queue * queue);
};

struct wayland_context
{
    struct wld_context base;
    struct wl_display * display;
    struct wl_event_queue * queue;
};

#if WITH_WAYLAND_DRM
extern const struct wld_wayland_interface wayland_drm_interface;
#endif

#if WITH_WAYLAND_SHM
extern const struct wld_wayland_interface wayland_shm_interface;
#endif

/**
 * Like wl_display_roundtrip, but for a particular event queue.
 */
int wayland_roundtrip(struct wl_display * display,
                      struct wl_event_queue * queue);

struct wld_exporter * wayland_create_exporter(struct wl_buffer * buffer);

#endif

