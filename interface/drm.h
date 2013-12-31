/* wld: interface/drm.h
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

#ifndef DRM_DRIVER_NAME
#   error "You must define DRM_DRIVER_NAME before including interface/drm.h"
#endif

/* DRM implementation */
static bool drm_device_supported(uint32_t vendor_id, uint32_t device_id);
static void * drm_create_context(int drm_fd);
static void drm_destroy_context(void * context);
static struct wld_drawable * drm_create_drawable
    (void * context, uint32_t width, uint32_t height, uint32_t format);
static struct wld_drawable * drm_import(void * context,
                                        uint32_t width, uint32_t height,
                                        uint32_t format, int prime_fd,
                                        unsigned long pitch);
static struct wld_drawable * drm_import_gem(void * context,
                                            uint32_t width, uint32_t height,
                                            uint32_t format, uint32_t gem_name,
                                            unsigned long pitch);

#define EXPAND(f, x) f(x)
#define VAR(name) name ## _drm
const struct wld_drm_interface EXPAND(VAR, DRM_DRIVER_NAME) = {
    .device_supported = &drm_device_supported,
    .create_context = &drm_create_context,
    .destroy_context = &drm_destroy_context,
    .create_drawable = &drm_create_drawable,
    .import = &drm_import,
    .import_gem = &drm_import_gem,
};
#undef VAR
#undef EXPAND

