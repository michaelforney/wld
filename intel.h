/* wld: intel.h
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

#ifndef WLD_INTEL_H
#define WLD_INTEL_H 1

#include <stdbool.h>
#include <stdint.h>

struct wld_intel_context;
struct wld_drawable;
enum wld_format;

/**
 * Create a new drawing context which uses Intel Graphics hardware accelerated
 * operations.
 */
struct wld_intel_context * wld_intel_create_context(int drm_fd);

/**
 * Destroy an Intel context
 */
void wld_intel_destroy_context(struct wld_intel_context * context);

/**
 * Check if the device with the given vendor and device ID is supported by the
 * Intel drawing backend.
 */
bool wld_intel_device_supported(uint32_t vendor_id, uint32_t device_id);

/**
 * Create a new Intel drawable with the specified dimensions.
 */
struct wld_drawable * wld_intel_create_drawable
    (struct wld_intel_context * context, uint32_t width, uint32_t height,
     enum wld_format format);

/**
 * Get the name and pitch of the underlying BO for this Intel drawable.
 */
void wld_intel_get_drawable_info(struct wld_drawable * drawable,
                                 uint32_t * name, uint32_t * pitch);

#endif

