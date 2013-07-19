/* wld: intel.c
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

#include "intel.h"
#include "drm-private.h"
#include "wld-private.h"

#include <unistd.h>
#include <intelbatch/batch.h>
#include <intelbatch/blt.h>
#include <intelbatch/mi.h>
#include <intel_bufmgr.h>
#include <i915_drm.h>

struct wld_intel_context
{
    drm_intel_bufmgr * bufmgr;
    struct intel_batch batch;
};

struct intel_drawable
{
    struct drm_drawable drm;

    struct wld_intel_context * context;
    drm_intel_bo * bo;
};

_Static_assert(offsetof(struct intel_drawable, drm) == 0,
               "Non-zero offset of base field");

static void intel_fill_rectangle(struct wld_drawable * drawable, uint32_t color,
                                 pixman_rectangle16_t * rectangle);
static void intel_draw_text_utf8(struct wld_drawable * drawable,
                                 struct font * font, uint32_t color,
                                 int32_t x, int32_t y,
                                 const char * text, int32_t length);
static void intel_flush(struct wld_drawable * drawable);
static void intel_destroy(struct wld_drawable * drawable);

const static struct wld_draw_interface intel_draw = {
    .fill_rectangle = &intel_fill_rectangle,
    .fill_rectangles = &default_fill_rectangles,
    .draw_text_utf8 = &intel_draw_text_utf8,
    .flush = &intel_flush,
    .destroy = &intel_destroy
};

const struct wld_drm_interface intel_drm = {
    .device_supported = &wld_intel_device_supported,
    .create_context = (drm_create_context_func_t) &wld_intel_create_context,
    .destroy_context = (drm_destroy_context_func_t) &wld_intel_destroy_context,
    .create_drawable = (drm_create_drawable_func_t) &wld_intel_create_drawable,
};

bool wld_intel_device_supported(uint32_t vendor_id, uint32_t device_id)
{
    return vendor_id == 0x8086;
}

struct wld_intel_context * wld_intel_create_context(int drm_fd)
{
    struct wld_intel_context * context;

    context = malloc(sizeof *context);

    if (!context)
        return NULL;

    context->bufmgr = drm_intel_bufmgr_gem_init(drm_fd, INTEL_BATCH_SIZE);
    intel_batch_initialize(&context->batch, context->bufmgr);

    return context;
}

void wld_intel_destroy_context(struct wld_intel_context * context)
{
    drm_intel_bufmgr_destroy(context->bufmgr);
    free(context);
}

struct wld_drawable * wld_intel_create_drawable
    (struct wld_intel_context * context, uint32_t width, uint32_t height,
     uint32_t format)
{
    struct intel_drawable * intel;
    uint32_t tiling_mode = I915_TILING_X, pitch;

    intel = malloc(sizeof *intel);

    if (!intel)
        return NULL;

    intel->drm.base.interface = &intel_draw;
    intel->drm.base.width = width;
    intel->drm.base.height = height;

    intel->context = context;
    intel->bo = drm_intel_bo_alloc_tiled(context->bufmgr, "drawable",
                                         width, height, 4,
                                         &tiling_mode, &intel->drm.pitch, 0);
    drm_intel_bo_gem_export_to_prime(intel->bo, &intel->drm.fd);

    return &intel->drm.base;
}

void intel_fill_rectangle(struct wld_drawable * drawable, uint32_t color,
                          pixman_rectangle16_t * rectangle)
{
    struct intel_drawable * intel = (void *) drawable;

    xy_color_blt(&intel->context->batch, intel->bo, intel->drm.pitch,
                 rectangle->x, rectangle->y,
                 rectangle->x + rectangle->width,
                 rectangle->y + rectangle->height, color);
}

void intel_draw_text_utf8(struct wld_drawable * drawable,
                          struct font * font, uint32_t color,
                          int32_t x, int32_t y,
                          const char * text, int32_t length)
{
    struct intel_drawable * intel = (void *) drawable;
    int ret;
    struct glyph * glyph;
    uint32_t row;
    FT_UInt glyph_index;
    uint32_t c;
    uint8_t immediate[512];
    uint8_t * byte;
    uint8_t byte_index;

    xy_setup_blt(&intel->context->batch, true, INTEL_BLT_RASTER_OPERATION_SRC,
                 0, color, intel->bo, intel->drm.pitch);

    while ((ret = FcUtf8ToUcs4((FcChar8 *) text, &c, length)) > 0 && c != '\0')
    {
        text += ret;
        length -= ret;
        glyph_index = FT_Get_Char_Index(font->face, c);

        if (!font_ensure_glyph(font, glyph_index))
            continue;

        glyph = font->glyphs[glyph_index];

        if (glyph->bitmap.width == 0 || glyph->bitmap.rows == 0)
            goto advance;

        byte = immediate;

        /* XY_TEXT_IMMEDIATE requires a pitch with no extra bytes */
        for (row = 0; row < glyph->bitmap.rows; ++row)
        {
            memcpy(byte, glyph->bitmap.buffer + (row * glyph->bitmap.pitch),
                   (glyph->bitmap.width + 7) / 8);
            byte += (glyph->bitmap.width + 7) / 8;
        }

      retry:
        ret = xy_text_immediate_blt(&intel->context->batch, intel->bo,
                                    x + glyph->x, y + glyph->y,
                                    x + glyph->x + glyph->bitmap.width,
                                    y + glyph->y + glyph->bitmap.rows,
                                    (byte - immediate + 3) / 4,
                                    (uint32_t *) immediate);

        if (ret == INTEL_BATCH_NO_SPACE)
        {
            intel_batch_flush(&intel->context->batch);
            xy_setup_blt(&intel->context->batch, true,
                         INTEL_BLT_RASTER_OPERATION_SRC, 0, color,
                         intel->bo, intel->drm.pitch);
            goto retry;
        }

      advance:
        x += glyph->advance;
    }
}

void intel_flush(struct wld_drawable * drawable)
{
    struct intel_drawable * intel = (void *) drawable;

    intel_batch_flush(&intel->context->batch);
}

void intel_destroy(struct wld_drawable * drawable)
{
    struct intel_drawable * intel = (void *) drawable;
    drm_intel_bo_unreference(intel->bo);
    close(intel->drm.fd);
    free(intel);
}

