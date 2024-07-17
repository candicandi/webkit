/*
 * Copyright (C) 2010 Igalia S.L
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public License
 * along with this library; see the file COPYING.LIB.  If not, write to
 * the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 */

#include "config.h"
#include "ImageGStreamer.h"

#if ENABLE(VIDEO) && USE(GSTREAMER)

#include "GStreamerCommon.h"
#include "BitmapImage.h"

#include <gst/gst.h>
#include <gst/video/gstvideometa.h>


namespace WebCore {

ImageGStreamer::ImageGStreamer(GRefPtr<GstSample>&& sample)
    : m_sample(WTFMove(sample))
{
    GstCaps* caps = gst_sample_get_caps(m_sample.get());
    GstVideoInfo videoInfo;
    gst_video_info_init(&videoInfo);
    if (!gst_video_info_from_caps(&videoInfo, caps))
        return;
    // QTFIXME: Check for https://bugreports.qt.io/browse/QTBUG-44245

    // Right now the TextureMapper only supports chromas with one plane
    ASSERT(GST_VIDEO_INFO_N_PLANES(&videoInfo) == 1);

    GstBuffer* buffer = gst_sample_get_buffer(m_sample.get());
    if (UNLIKELY(!GST_IS_BUFFER(buffer)))
        return;

    m_frameMapped = gst_video_frame_map(&m_videoFrame, &videoInfo, buffer, GST_MAP_READ);
    if (!m_frameMapped)
        return;

    unsigned char* bufferData = reinterpret_cast<unsigned char*>(GST_VIDEO_FRAME_PLANE_DATA(&m_videoFrame, 0));
    int stride = GST_VIDEO_FRAME_PLANE_STRIDE(&m_videoFrame, 0);
    int width = GST_VIDEO_FRAME_WIDTH(&m_videoFrame);
    int height = GST_VIDEO_FRAME_HEIGHT(&m_videoFrame);

    QImage surface;
    QImage::Format imageFormat;
    imageFormat = (GST_VIDEO_FRAME_FORMAT(&m_videoFrame) == GST_VIDEO_FORMAT_RGBA) ? QImage::Format_ARGB32_Premultiplied : QImage::Format_RGB32;

    // GStreamer doesn't use premultiplied alpha, but Qt does. So if the video format has an alpha component
    // we need to premultiply it before passing the data to Qt. This needs to be both using gstreamer-gl and not
    // using it.
    //
    // This method could be called several times for the same buffer, for example if we are rendering the video frames
    // in several non accelerated canvases. Due to this, we cannot modify the buffer, so we need to create a copy.
    if (imageFormat == QImage::Format_ARGB32_Premultiplied) {
        unsigned char* surfaceData = static_cast<unsigned char*>(fastMalloc(height * stride));
        unsigned char* surfacePixel = surfaceData;

        for (int x = 0; x < width; x++) {
            for (int y = 0; y < height; y++) {
                unsigned short alpha = bufferData[3];
#if G_BYTE_ORDER == G_LITTLE_ENDIAN
                // Video frames use RGBA in little endian.
                surfacePixel[0] = (bufferData[2] * alpha + 128) / 255;
                surfacePixel[1] = (bufferData[1] * alpha + 128) / 255;
                surfacePixel[2] = (bufferData[0] * alpha + 128) / 255;
                surfacePixel[3] = alpha;
#else
                // Video frames use RGBA in big endian.
                surfacePixel[0] = alpha;
                surfacePixel[1] = (bufferData[0] * alpha + 128) / 255;
                surfacePixel[2] = (bufferData[1] * alpha + 128) / 255;
                surfacePixel[3] = (bufferData[2] * alpha + 128) / 255;
#endif
                bufferData += 4;
                surfacePixel += 4;
            }
        }
        surface = QImage(surfaceData, width, height, stride, imageFormat, [](void* data) {
            fastFree(data);
        });
    } else
        surface = QImage(bufferData, width, height, stride, imageFormat);

#if PLATFORM(QT)
    m_image = QImage(surface);
#else
    m_image = BitmapImage::create(WTFMove(surface));
#endif

    if (GstVideoCropMeta* cropMeta = gst_buffer_get_video_crop_meta(buffer))
        setCropRect(FloatRect(cropMeta->x, cropMeta->y, cropMeta->width, cropMeta->height));
}

ImageGStreamer::~ImageGStreamer()
{
    // We keep the buffer memory mapped until the image is destroyed because the internal
    // QImage was created using the buffer data directly.
    if (m_frameMapped)
        gst_video_frame_unmap(&m_videoFrame);
}

} // namespace WebCore

#endif // USE(GSTREAMER)
