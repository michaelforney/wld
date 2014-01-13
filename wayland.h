/* wld: wayland.h
 *
 * Copyright (c) 2013 Michael Forney
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

#ifndef WLD_WAYLAND_H
#define WLD_WAYLAND_H

#include <stdint.h>

struct wl_display;
struct wl_surface;
struct wl_buffer;

struct wld_wayland_context;

enum wld_wayland_interface_id
{
    /**
     * Give up on trying any new interfaces. This can be considered as a
     * sentinel for wld_wayland_create_context.
     */
    WLD_NONE = -2,

    /**
     * Try any available interface.
     */
    WLD_ANY = -1,
    WLD_DRM,
    WLD_SHM
};

enum wld_wayland_object_type
{
    WLD_WAYLAND_OBJECT_BUFFER   = 0x00020000
};

enum wld_wayland_damage_flags
{
    /**
     * Copy the damaged region from the front buffer to the back buffer after
     * swapping buffers.
     *
     * This allows you to think of the drawable as a single image.
     */
    WLD_WAYLAND_DAMAGE_COPY     = 1 << 0,

    /**
     * Submit the damaged region to the compositor before swapping buffers.
     *
     * If this is not selected, you must manually damage the surface before
     * calling wld_flush.
     */
    WLD_WAYLAND_DAMAGE_SUBMIT   = 1 << 1
};

/**
 * Create a new drawing context which uses various available Wayland interfaces
 * (such as wl_shm and wl_drm) to create buffers backed by drawables specific
 * to the interface.
 *
 * You can specify the particular interface you want to use by specifying them
 * as arguments. Interfaces will be tried in the order they are given.
 *
 * The last argument must be either WLD_NONE or WLD_ANY.
 *
 * @see enum wld_wayland_interface_id
 */
struct wld_wayland_context * wld_wayland_create_context
    (struct wl_display * display, enum wld_wayland_interface_id id, ...);

/**
 * Destroy a Wayland context.
 */
void wld_wayland_destroy_context(struct wld_wayland_context * context);

/**
 * Create a new Wayland drawable for the given surface with a particular pixel
 * format.
 *
 * @param damage_flags Initial damage tracking mode.
 *                     @see enum wld_wayland_damage_tracking_flags
 */
struct wld_drawable * wld_wayland_create_drawable
    (struct wld_wayland_context * context, struct wl_surface * surface,
     uint32_t width, uint32_t height, uint32_t format, uint32_t damage_flags);

/**
 * Enable or disable damage tracking on the specified Wayland drawable.
 *
 * @see enum wld_wayland_damage_tracking_flags
 */
void wld_wayland_drawable_set_damage_tracking(struct wld_drawable * drawable,
                                              uint32_t flags);

#endif

