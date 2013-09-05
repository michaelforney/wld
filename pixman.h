/* wld: pixman.h
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

#ifndef WLD_PIXMAN_H
#define WLD_PIXMAN_H 1

#include <stdint.h>

struct wld_pixman_context;
struct wld_drawable;

/**
 * Create a new drawing context which uses Pixman to do software rendering on
 * buffers in memory.
 */
struct wld_pixman_context * wld_pixman_create_context();

/**
 * Destroy a Pixman context.
 */
void wld_pixman_destroy_context(struct wld_pixman_context * context);

/**
 * Create a new Pixman drawable with the specified dimensions.
 *
 * If data is NULL, new backing memory will be allocated.
 */
struct wld_drawable * wld_pixman_create_drawable
    (struct wld_pixman_context * context, uint32_t width, uint32_t height,
     void * data, uint32_t pitch, uint32_t format);

#endif

