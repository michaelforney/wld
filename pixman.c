/* wld: pixman.c
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

#include "pixman.h"
#include "wld-private.h"

#define PIXMAN_COLOR(c) {                   \
    .alpha  = ((c >> 24) & 0xff) * 0x101,   \
    .red    = ((c >> 16) & 0xff) * 0x101,   \
    .green  = ((c >>  8) & 0xff) * 0x101,   \
    .blue   = ((c >>  0) & 0xff) * 0x101,   \
}

struct wld_pixman_context
{
    pixman_glyph_cache_t * glyph_cache;
};

struct pixman_drawable
{
    struct wld_drawable base;
    pixman_image_t * image;
    struct wld_pixman_context * context;
};

_Static_assert(offsetof(struct pixman_drawable, base) == 0,
               "Non-zero offset of base field");

static void pixman_fill_rectangle(struct wld_drawable * drawable,
                                  uint32_t color, int32_t x, int32_t y,
                                  uint32_t width, uint32_t height);
static void pixman_fill_region(struct wld_drawable * drawable, uint32_t color,
                               pixman_region32_t * region);
static void pixman_copy_rectangle(struct wld_drawable * src,
                                  struct wld_drawable * dst,
                                  int32_t src_x, int32_t src_y,
                                  int32_t dst_x, int32_t dst_y,
                                  uint32_t width, uint32_t height);
static void pixman_copy_region(struct wld_drawable * src,
                               struct wld_drawable * dst,
                               pixman_region32_t * region,
                               int32_t dst_x, int32_t dst_y);
static void pixman_draw_text_utf8(struct wld_drawable * drawable,
                                  struct font * font, uint32_t color,
                                  int32_t x, int32_t y,
                                  const char * text, int32_t length,
                                  struct wld_extents * extents);
static void pixman_write(struct wld_drawable * drawable,
                         const void * data, size_t size);
static pixman_image_t * pixman_map(struct wld_drawable * drawable);
static void pixman_flush(struct wld_drawable * drawable);
static void pixman_destroy(struct wld_drawable * drawable);

const static struct wld_draw_interface pixman_draw = {
    .fill_rectangle = &pixman_fill_rectangle,
    .fill_region = &pixman_fill_region,
    .copy_rectangle = &pixman_copy_rectangle,
    .copy_region = &pixman_copy_region,
    .draw_text_utf8 = &pixman_draw_text_utf8,
    .write = &pixman_write,
    .map = &pixman_map,
    .flush = &pixman_flush,
    .destroy = &pixman_destroy
};

struct wld_pixman_context * wld_pixman_create_context()
{
    struct wld_pixman_context * context;

    context = malloc(sizeof *context);

    if (!context)
        return NULL;

    context->glyph_cache = pixman_glyph_cache_create();

    return context;
}

void wld_pixman_destroy_context(struct wld_pixman_context * context)
{
    pixman_glyph_cache_destroy(context->glyph_cache);
    free(context);
}

struct wld_drawable * wld_pixman_create_drawable
    (struct wld_pixman_context * context, uint32_t width, uint32_t height,
     void * data, uint32_t pitch, uint32_t format)
{
    struct pixman_drawable * pixman;

    pixman = malloc(sizeof *pixman);

    if (!pixman)
        goto error0;

    pixman->base.interface = &pixman_draw;
    pixman->base.width = width;
    pixman->base.height = height;
    pixman->base.format = format;
    pixman->base.pitch = pitch;

    pixman->context = context;
    pixman->image = pixman_image_create_bits(pixman_format(format),
                                             width, height,
                                             (uint32_t *) data, pitch);

    if (!pixman->image)
        goto error1;

    return &pixman->base;

  error1:
    free(pixman);
  error0:
    return NULL;
}

static void pixman_fill_rectangle(struct wld_drawable * drawable,
                                  uint32_t color, int32_t x, int32_t y,
                                  uint32_t width, uint32_t height)
{
    struct pixman_drawable * pixman = (void *) drawable;
    pixman_color_t pixman_color = PIXMAN_COLOR(color);
    pixman_box32_t box = { x, y, x + width, y + height };

    pixman_image_fill_boxes(PIXMAN_OP_SRC, pixman->image, &pixman_color,
                            1, &box);
}

static void pixman_fill_region(struct wld_drawable * drawable, uint32_t color,
                               pixman_region32_t * region)
{
    struct pixman_drawable * pixman = (void *) drawable;
    pixman_color_t pixman_color = PIXMAN_COLOR(color);
    pixman_box32_t * boxes;
    int num_boxes;

    boxes = pixman_region32_rectangles(region, &num_boxes);
    pixman_image_fill_boxes(PIXMAN_OP_SRC, pixman->image, &pixman_color,
                            num_boxes, boxes);
}

static void pixman_copy_rectangle(struct wld_drawable * src_drawable,
                                  struct wld_drawable * dst_drawable,
                                  int32_t src_x, int32_t src_y,
                                  int32_t dst_x, int32_t dst_y,
                                  uint32_t width, uint32_t height)
{
    struct pixman_drawable * src = (void *) src_drawable;
    struct pixman_drawable * dst = (void *) dst_drawable;

    pixman_image_composite32(PIXMAN_OP_SRC, src->image, NULL, dst->image,
                             src_x, src_y, 0, 0, dst_x, dst_y, width, height);
}

static void pixman_copy_region(struct wld_drawable * src_drawable,
                               struct wld_drawable * dst_drawable,
                               pixman_region32_t * region,
                               int32_t dst_x, int32_t dst_y)
{
    struct pixman_drawable * src = (void *) src_drawable;
    struct pixman_drawable * dst = (void *) dst_drawable;

    pixman_image_set_clip_region32(src->image, region);
    pixman_image_composite32(PIXMAN_OP_SRC, src->image, NULL, dst->image,
                             region->extents.x1, region->extents.y1, 0, 0,
                             region->extents.x1 + dst_x,
                             region->extents.y1 + dst_y,
                             region->extents.x2 - region->extents.x1,
                             region->extents.y2 - region->extents.y1);
    pixman_image_set_clip_region32(src->image, NULL);
}

static inline uint8_t reverse(uint8_t byte)
{
    byte = ((byte << 1) & 0xaa) | ((byte >> 1) & 0x55);
    byte = ((byte << 2) & 0xcc) | ((byte >> 2) & 0x33);
    byte = ((byte << 4) & 0xf0) | ((byte >> 4) & 0x0f);

    return byte;
}

static void pixman_draw_text_utf8(struct wld_drawable * drawable,
                                  struct font * font, uint32_t color,
                                  int32_t x, int32_t y,
                                  const char * text, int32_t length,
                                  struct wld_extents * extents)
{
    struct pixman_drawable * pixman = (void *) drawable;
    int ret;
    uint32_t c;
    struct glyph * glyph;
    FT_UInt glyph_index;
    pixman_glyph_t glyphs[strlen(text)];
    uint32_t index = 0, origin_x = 0;
    pixman_color_t pixman_color = PIXMAN_COLOR(color);
    pixman_image_t * solid;

    solid = pixman_image_create_solid_fill(&pixman_color);

    while ((ret = FcUtf8ToUcs4((FcChar8 *) text, &c, length)) > 0 && c != '\0')
    {
        text += ret;
        length -= ret;
        glyph_index = FT_Get_Char_Index(font->face, c);

        if (!font_ensure_glyph(font, glyph_index))
            continue;

        glyph = font->glyphs[glyph_index];

        glyphs[index].x = origin_x;
        glyphs[index].y = 0;
        glyphs[index].glyph = pixman_glyph_cache_lookup
            (pixman->context->glyph_cache, font, glyph);

        /* If we don't have the glyph in our cache, do some conversions to make
         * pixman happy, and then insert it. */
        if (!glyphs[index].glyph)
        {
            uint8_t * src, * dst;
            uint32_t row, byte_index, bytes_per_row, pitch;
            pixman_image_t * image;
            FT_Bitmap * bitmap;

            bitmap = &glyph->bitmap;
            image = pixman_image_create_bits
                (PIXMAN_a1, bitmap->width, bitmap->rows, NULL, bitmap->pitch);

            if (!image)
                goto advance;

            pitch = pixman_image_get_stride(image);
            bytes_per_row = (bitmap->width + 7) / 8;
            src = bitmap->buffer;
            dst = (uint8_t *) pixman_image_get_data(image);

            for (row = 0; row < bitmap->rows; ++row)
            {
                /* Pixman's A1 format expects the bits in the opposite order
                 * that Freetype gives us. Sigh... */
                for (byte_index = 0; byte_index < bytes_per_row; ++byte_index)
                    dst[byte_index] = reverse(src[byte_index]);

                dst += pitch;
                src += bitmap->pitch;
            }

            /* Insert the glyph into the cache. */
            pixman_glyph_cache_freeze(pixman->context->glyph_cache);
            glyphs[index].glyph = pixman_glyph_cache_insert
                (pixman->context->glyph_cache, font, glyph,
                 -glyph->x, -glyph->y, image);
            pixman_glyph_cache_thaw(pixman->context->glyph_cache);

            /* The glyph cache copies the contents of the glyph bitmap. */
            pixman_image_unref(image);
        }

        ++index;

      advance:
        origin_x += glyph->advance;
    }

    pixman_composite_glyphs_no_mask(PIXMAN_OP_OVER, solid, pixman->image, 0, 0,
                                    x, y, pixman->context->glyph_cache,
                                    index, glyphs);

    pixman_image_unref(solid);

    if (extents)
        extents->advance = origin_x;
}

static void pixman_write(struct wld_drawable * drawable,
                         const void * data, size_t size)
{
    struct pixman_drawable * pixman = (void *) drawable;

    memcpy(pixman_image_get_data(pixman->image), data, size);
}

pixman_image_t * pixman_map(struct wld_drawable * drawable)
{
    struct pixman_drawable * pixman = (void *) drawable;

    return pixman_image_ref(pixman->image);
}

static void pixman_flush(struct wld_drawable * drawable)
{
}

static void pixman_destroy(struct wld_drawable * drawable)
{
    struct pixman_drawable * pixman = (void *) drawable;

    pixman_image_unref(pixman->image);
    free(pixman);
}

