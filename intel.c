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

#include "drm-private.h"
#include "drm.h"
#include "wld-private.h"

#include <unistd.h>
#include <intelbatch.h>
#include <intel_bufmgr.h>
#include <i915_drm.h>

struct intel_context
{
    struct wld_context base;
    drm_intel_bufmgr * bufmgr;
    struct intel_batch * batch;
};

struct intel_drawable
{
    struct wld_drawable base;

    struct intel_context * context;
    drm_intel_bo * bo;
    pixman_image_t * virtual;
};

#include "interface/context.h"
#define DRM_DRIVER_NAME intel
#include "interface/drm.h"
#include "interface/drm_drawable.h"
IMPL(intel, context)

bool drm_device_supported(uint32_t vendor_id, uint32_t device_id)
{
    return vendor_id == 0x8086;
}

struct wld_context * drm_create_context(int drm_fd)
{
    struct intel_context * context;

    context = malloc(sizeof *context);

    if (!context)
        goto error0;

    context_initialize(&context->base, &context_impl);
    context->bufmgr = drm_intel_bufmgr_gem_init(drm_fd, INTEL_BATCH_SIZE);

    if (!context->bufmgr)
        goto error1;

    context->batch = intel_batch_new(context->bufmgr);

    if (!context->batch)
        goto error2;

    return &context->base;

  error2:
    drm_intel_bufmgr_destroy(context->bufmgr);
  error1:
    free(context);
  error0:
    return NULL;
}

static struct intel_drawable * new_drawable(struct intel_context * context,
                                            uint32_t width, uint32_t height,
                                            uint32_t format)
{
    struct intel_drawable * intel;

    if (!(intel = malloc(sizeof *intel)))
        return NULL;

    intel->base.interface = &draw_interface.base;
    intel->base.width = width;
    intel->base.height = height;
    intel->base.format = format;
    intel->context = context;
    intel->virtual = NULL;

    return intel;
}

struct wld_drawable * context_create_drawable(struct wld_context * base,
                                              uint32_t width, uint32_t height,
                                              uint32_t format)
{
    struct intel_context * context = intel_context(base);
    struct intel_drawable * intel;
    uint32_t tiling_mode = width >= 128 ? I915_TILING_X : I915_TILING_NONE;

    if (!(intel = new_drawable(context, width, height, format)))
        return NULL;

    intel->bo = drm_intel_bo_alloc_tiled(context->bufmgr, "drawable",
                                         width, height, 4, &tiling_mode,
                                         &intel->base.pitch, 0);

    return &intel->base;
}

struct wld_drawable * context_import(struct wld_context * base,
                                     uint32_t type, union wld_object object,
                                     uint32_t width, uint32_t height,
                                     uint32_t format, uint32_t pitch)
{
    struct intel_context * context = intel_context(base);
    struct intel_drawable * intel;
    drm_intel_bo * bo;

    switch (type)
    {
        case WLD_DRM_OBJECT_PRIME_FD:
        {
            uint32_t size = width * height * format_bytes_per_pixel(format);
            bo = drm_intel_bo_gem_create_from_prime(context->bufmgr,
                                                    object.i, size);
            break;
        }
        case WLD_DRM_OBJECT_GEM_NAME:
            bo = drm_intel_bo_gem_create_from_name(context->bufmgr, "buffer",
                                                   object.u32);
            break;
        default: bo = NULL;
    };

    if (!bo)
        goto error0;

    if (!(intel = new_drawable(context, width, height, format)))
        goto error1;

    intel->bo = bo;
    intel->base.pitch = pitch;

    return &intel->base;

  error1:
    drm_intel_bo_unreference(bo);
  error0:
    return NULL;
}

void context_destroy(struct wld_context * base)
{
    struct intel_context * context = intel_context(base);

    intel_batch_destroy(context->batch);
    drm_intel_bufmgr_destroy(context->bufmgr);
    free(context);
}

void drawable_fill_rectangle(struct wld_drawable * drawable, uint32_t color,
                             int32_t x, int32_t y,
                             uint32_t width, uint32_t height)
{
    struct intel_drawable * intel = (void *) drawable;

    xy_color_blt(intel->context->batch, intel->bo, intel->base.pitch,
                 x, y, x + width, y + height, color);
}

void drawable_copy_rectangle(struct wld_drawable * src_drawable,
                             struct wld_drawable * dst_drawable,
                             int32_t src_x, int32_t src_y,
                             int32_t dst_x, int32_t dst_y,
                             uint32_t width, uint32_t height)
{
    struct intel_drawable * src = (void *) src_drawable;
    struct intel_drawable * dst = (void *) dst_drawable;

    xy_src_copy_blt(dst->context->batch,
                    src->bo, src->base.pitch, src_x, src_y,
                    dst->bo, dst->base.pitch, dst_x, dst_y, width, height);
}

void drawable_draw_text_utf8(struct wld_drawable * drawable,
                             struct font * font, uint32_t color,
                             int32_t x, int32_t y,
                             const char * text, int32_t length,
                             struct wld_extents * extents)
{
    struct intel_drawable * intel = (void *) drawable;
    int ret;
    struct glyph * glyph;
    uint32_t row;
    FT_UInt glyph_index;
    uint32_t c;
    uint8_t immediate[512];
    uint8_t * byte;
    int32_t origin_x = x;

    xy_setup_blt(intel->context->batch, true, INTEL_BLT_RASTER_OPERATION_SRC,
                 0, color, intel->bo, intel->base.pitch);

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
        ret = xy_text_immediate_blt(intel->context->batch, intel->bo,
                                    origin_x + glyph->x, y + glyph->y,
                                    origin_x + glyph->x + glyph->bitmap.width,
                                    y + glyph->y + glyph->bitmap.rows,
                                    (byte - immediate + 3) / 4,
                                    (uint32_t *) immediate);

        if (ret == INTEL_BATCH_NO_SPACE)
        {
            intel_batch_flush(intel->context->batch);
            xy_setup_blt(intel->context->batch, true,
                         INTEL_BLT_RASTER_OPERATION_SRC, 0, color,
                         intel->bo, intel->base.pitch);
            goto retry;
        }

      advance:
        origin_x += glyph->advance;
    }

    if (extents)
        extents->advance = origin_x - x;
}

void drawable_write(struct wld_drawable * drawable, const void * data, size_t size)
{
    struct intel_drawable * intel = (void *) drawable;

    drm_intel_bo_subdata(intel->bo, 0, size, data);
}

static void destroy_virtual(pixman_image_t * image, void * data)
{
    struct intel_drawable * intel = data;

    drm_intel_gem_bo_unmap_gtt(intel->bo);
    intel->virtual = NULL;
}

pixman_image_t * drawable_map(struct wld_drawable * drawable)
{
    struct intel_drawable * intel = (void *) drawable;

    if (!intel->virtual)
    {
        drm_intel_gem_bo_map_gtt(intel->bo);
        intel->virtual = pixman_image_create_bits_no_clear
            (format_wld_to_pixman(drawable->format),
             drawable->width, drawable->height,
             intel->bo->virtual, drawable->pitch);
        pixman_image_set_destroy_function(intel->virtual, &destroy_virtual,
                                          intel);
    }
    else
        pixman_image_ref(intel->virtual);

    return intel->virtual;
}

void drawable_flush(struct wld_drawable * drawable)
{
    struct intel_drawable * intel = (void *) drawable;

    intel_batch_flush(intel->context->batch);
}

void drawable_destroy(struct wld_drawable * drawable)
{
    struct intel_drawable * intel = (void *) drawable;
    drm_intel_bo_unreference(intel->bo);
    free(intel);
}

int drawable_export(struct wld_drawable * drawable)
{
    struct intel_drawable * intel = (void *) drawable;
    int prime_fd;

    drm_intel_bo_gem_export_to_prime(intel->bo, &prime_fd);

    return prime_fd;
}

uint32_t drawable_get_handle(struct wld_drawable * drawable)
{
    struct intel_drawable * intel = (void *) drawable;

    return intel->bo->handle;
}

