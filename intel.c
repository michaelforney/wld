/* wld: intel.c
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
};

struct intel_renderer
{
    struct wld_renderer base;
    struct intel_batch * batch;
    struct intel_buffer * target;
};

struct intel_buffer
{
    struct wld_buffer base;
    struct wld_exporter exporter;
    drm_intel_bo * bo;
};

#include "interface/context.h"
#include "interface/renderer.h"
#include "interface/buffer.h"
#include "interface/exporter.h"
#define DRM_DRIVER_NAME intel
#include "interface/drm.h"
IMPL(intel, context)
IMPL(intel, renderer)
IMPL(intel, buffer)

/**** DRM driver ****/
bool driver_device_supported(uint32_t vendor_id, uint32_t device_id)
{
    return vendor_id == 0x8086;
}

struct wld_context * driver_create_context(int drm_fd)
{
    struct intel_context * context;

    context = malloc(sizeof *context);

    if (!context)
        goto error0;

    context_initialize(&context->base, &context_impl);
    context->bufmgr = drm_intel_bufmgr_gem_init(drm_fd, INTEL_BATCH_SIZE);

    if (!context->bufmgr)
        goto error1;

    return &context->base;

  error1:
    free(context);
  error0:
    return NULL;
}

/**** Context ****/
struct wld_renderer * context_create_renderer(struct wld_context * base)
{
    struct intel_context * context = intel_context(base);
    struct intel_renderer * renderer;

    if (!(renderer = malloc(sizeof *renderer)))
        goto error0;

    if (!(renderer->batch = intel_batch_new(context->bufmgr)))
        goto error1;

    renderer_initialize(&renderer->base, &renderer_impl);

    return &renderer->base;

  error1:
    free(renderer);
  error0:
    return NULL;
}

static struct wld_buffer * new_buffer(uint32_t width, uint32_t height,
                                      uint32_t format, uint32_t pitch,
                                      drm_intel_bo * bo)
{
    struct intel_buffer * buffer;

    if (!(buffer = malloc(sizeof *buffer)))
        return NULL;

    buffer_initialize(&buffer->base, &buffer_impl,
                      width, height, format, pitch);
    buffer->bo = bo;
    exporter_initialize(&buffer->exporter, &exporter_impl);
    buffer_add_exporter(&buffer->base, &buffer->exporter);

    return &buffer->base;
}

struct wld_buffer * context_create_buffer(struct wld_context * base,
                                          uint32_t width, uint32_t height,
                                          uint32_t format)
{
    struct intel_context * context = intel_context(base);
    struct wld_buffer * buffer;
    drm_intel_bo * bo;
    uint32_t tiling_mode = width >= 128 ? I915_TILING_X : I915_TILING_NONE;
    unsigned long pitch;

    bo = drm_intel_bo_alloc_tiled(context->bufmgr, "buffer", width, height, 4,
                                  &tiling_mode, &pitch, 0);

    if (!bo)
        goto error0;

    if (!(buffer = new_buffer(width, height, format, pitch, bo)))
        goto error1;

    return buffer;

  error1:
    drm_intel_bo_unreference(bo);
  error0:
    return NULL;
}

struct wld_buffer * context_import_buffer(struct wld_context * base,
                                          uint32_t type,
                                          union wld_object object,
                                          uint32_t width, uint32_t height,
                                          uint32_t format, uint32_t pitch)
{
    struct intel_context * context = intel_context(base);
    struct wld_buffer * buffer;
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

    if (!(buffer = new_buffer(width, height, format, pitch, bo)))
        goto error1;

    return buffer;

  error1:
    drm_intel_bo_unreference(bo);
  error0:
    return NULL;
}

void context_destroy(struct wld_context * base)
{
    struct intel_context * context = intel_context(base);

    drm_intel_bufmgr_destroy(context->bufmgr);
    free(context);
}

/**** Renderer ****/
uint32_t renderer_capabilities(struct wld_renderer * renderer,
                               struct wld_buffer * buffer)
{
    if (buffer->impl == &buffer_impl)
        return WLD_CAPABILITY_READ | WLD_CAPABILITY_WRITE;

    return 0;
}

bool renderer_set_target(struct wld_renderer * base,
                         struct wld_buffer * buffer)
{
    struct intel_renderer * renderer = intel_renderer(base);

    if (buffer && buffer->impl != &buffer_impl)
        return false;

    renderer->target = buffer ? intel_buffer(buffer) : NULL;

    return true;
}

void renderer_fill_rectangle(struct wld_renderer * base, uint32_t color,
                             int32_t x, int32_t y,
                             uint32_t width, uint32_t height)
{
    struct intel_renderer * renderer = intel_renderer(base);

    xy_color_blt(renderer->batch, renderer->target->bo,
                 renderer->target->base.pitch,
                 x, y, x + width, y + height, color);
}

void renderer_copy_rectangle(struct wld_renderer * base,
                             struct wld_buffer * buffer_base,
                             int32_t dst_x, int32_t dst_y,
                             int32_t src_x, int32_t src_y,
                             uint32_t width, uint32_t height)
{
    struct intel_renderer * renderer = intel_renderer(base);

    if (buffer_base->impl != &buffer_impl)
        return;

    struct intel_buffer * buffer = intel_buffer(buffer_base);

    xy_src_copy_blt(renderer->batch, buffer->bo, buffer->base.pitch,
                    src_x, src_y,
                    renderer->target->bo, renderer->target->base.pitch,
                    dst_x, dst_y, width, height);
}

void renderer_draw_text(struct wld_renderer * base,
                        struct font * font, uint32_t color,
                        int32_t x, int32_t y, const char * text, int32_t length,
                        struct wld_extents * extents)
{
    struct intel_renderer * renderer = intel_renderer(base);
    int ret;
    struct glyph * glyph;
    uint32_t row;
    FT_UInt glyph_index;
    uint32_t c;
    uint8_t immediate[512];
    uint8_t * byte;
    int32_t origin_x = x;

    xy_setup_blt(renderer->batch, true, INTEL_BLT_RASTER_OPERATION_SRC,
                 0, color, renderer->target->bo, renderer->target->base.pitch);

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
        ret = xy_text_immediate_blt(renderer->batch, renderer->target->bo,
                                    origin_x + glyph->x, y + glyph->y,
                                    origin_x + glyph->x + glyph->bitmap.width,
                                    y + glyph->y + glyph->bitmap.rows,
                                    (byte - immediate + 3) / 4,
                                    (uint32_t *) immediate);

        if (ret == INTEL_BATCH_NO_SPACE)
        {
            intel_batch_flush(renderer->batch);
            xy_setup_blt(renderer->batch, true,
                         INTEL_BLT_RASTER_OPERATION_SRC, 0, color,
                         renderer->target->bo, renderer->target->base.pitch);
            goto retry;
        }

      advance:
        origin_x += glyph->advance;
    }

    if (extents)
        extents->advance = origin_x - x;
}

void renderer_flush(struct wld_renderer * base)
{
    struct intel_renderer * renderer = intel_renderer(base);

    intel_batch_flush(renderer->batch);
}

void renderer_destroy(struct wld_renderer * base)
{
    struct intel_renderer * renderer = intel_renderer(base);

    intel_batch_destroy(renderer->batch);
    free(renderer);
}

/**** Buffer ****/
bool buffer_map(struct wld_buffer * base)
{
    struct intel_buffer * buffer = intel_buffer(base);

    if (drm_intel_gem_bo_map_gtt(buffer->bo) != 0)
        return false;

    buffer->base.map.data = buffer->bo->virtual;

    return true;
}

bool buffer_unmap(struct wld_buffer * base)
{
    struct intel_buffer * buffer = intel_buffer(base);

    if (drm_intel_gem_bo_unmap_gtt(buffer->bo) != 0)
        return false;

    buffer->base.map.data = NULL;

    return true;
}

void buffer_destroy(struct wld_buffer * base)
{
    struct intel_buffer * buffer = intel_buffer(base);

    drm_intel_bo_unreference(buffer->bo);
    free(buffer);
}

/**** Exporter ****/
bool exporter_export(struct wld_exporter * exporter, struct wld_buffer * base,
                     uint32_t type, union wld_object * object)
{
    struct intel_buffer * buffer = intel_buffer(base);

    switch (type)
    {
        case WLD_DRM_OBJECT_HANDLE:
            object->u32 = buffer->bo->handle;
            return true;
        case WLD_DRM_OBJECT_PRIME_FD:
            if (drm_intel_bo_gem_export_to_prime(buffer->bo, &object->i) != 0)
                return false;
            return true;
        case WLD_DRM_OBJECT_GEM_NAME:
            if (drm_intel_bo_flink(buffer->bo, &object->u32) != 0)
                return false;
            return true;
        default:
            return false;
    }
}

void exporter_destroy(struct wld_exporter * exporter)
{
}

