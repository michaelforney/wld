/* wld: drm.h
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

#ifndef WLD_DRM_H
#define WLD_DRM_H 1

#include <stdint.h>

struct wld_drm_context;
struct wld_drawable;

/**
 * Create a new DRM context from an opened DRM device file descriptor.
 */
struct wld_drm_context * wld_drm_create_context(int fd);

/**
 * Destroy a DRM context.
 */
void wld_drm_destroy_context(struct wld_drm_context * drm);

/**
 * Create a new DRM drawable.
 */
struct wld_drawable * wld_drm_create_drawable(struct wld_drm_context * drm,
                                              uint32_t width, uint32_t height,
                                              uint32_t format);

#endif

