/* wld: interface/drawable.h
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

static void drawable_fill_rectangle(struct wld_drawable * drawable,
                                    uint32_t color, int32_t x, int32_t y,
                                    uint32_t width, uint32_t height);
static void drawable_copy_rectangle(struct wld_drawable * src,
                                    struct wld_drawable * dst,
                                    int32_t src_x, int32_t src_y,
                                    int32_t dst_x, int32_t dst_y,
                                    uint32_t width, uint32_t height);
#ifdef DRAWABLE_IMPLEMENTS_REGION
static void drawable_fill_region(struct wld_drawable * drawable, uint32_t color,
                                 pixman_region32_t * region);
static void drawable_copy_region(struct wld_drawable * src,
                                 struct wld_drawable * drawable,
                                 pixman_region32_t * region,
                                 int32_t dst_x, int32_t dst_y);
#endif
static void drawable_draw_text(struct wld_drawable * drawable,
                               struct font * font, uint32_t color,
                               int32_t x, int32_t y,
                               const char * text, int32_t length,
                               struct wld_extents * extents);
static void drawable_write(struct wld_drawable * drawable,
                           const void * data, size_t size);
static pixman_image_t * drawable_map(struct wld_drawable * drawable);
static void drawable_flush(struct wld_drawable * drawable);
static void drawable_destroy(struct wld_drawable * drawable);

static const struct wld_drawable_impl drawable_impl = {
    .fill_rectangle = &drawable_fill_rectangle,
    .copy_rectangle = &drawable_copy_rectangle,
#ifdef DRAWABLE_IMPLEMENTS_REGION
    .fill_region = &drawable_fill_region,
    .copy_region = &drawable_copy_region,
#else
    .fill_region = &default_fill_region,
    .copy_region = &default_copy_region,
#endif
    .draw_text = &drawable_draw_text,
    .write = &drawable_write,
    .map = &drawable_map,
    .flush = &drawable_flush,
    .destroy = &drawable_destroy
};

