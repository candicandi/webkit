/*
 * Copyright (C) 2010, 2011, 2012 Igalia S.L
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

#pragma once

#if ENABLE(VIDEO) && USE(GSTREAMER)

#include "FloatRect.h"
#include "GStreamerCommon.h"
#include "PlatformImage.h"

#include <gst/video/video-frame.h>

#include <wtf/Forward.h>

namespace WebCore {
class IntSize;

class ImageGStreamer : public RefCounted<ImageGStreamer> {
public:
    static Ref<ImageGStreamer> create(GRefPtr<GstSample>&& sample)
    {
        return adoptRef(*new ImageGStreamer(WTFMove(sample)));
    }
    ~ImageGStreamer();

#if PLATFORM(QT)
    operator bool() const { return !m_image.isNull(); }
#else
    operator bool() const { return !!m_image; }
#endif

    PlatformImagePtr image() const { return m_image; }

    void setCropRect(FloatRect rect) { m_cropRect = rect; }
    FloatRect rect()
    {
#if PLATFORM(QT)
        ASSERT(!m_image.isNull());
#else
        ASSERT(m_image);
#endif
        if (!m_cropRect.isEmpty())
            return FloatRect(m_cropRect);
        return FloatRect(0, 0, m_size.width(), m_size.height());
    }

    bool hasAlpha() const { return m_hasAlpha; }

private:
    ImageGStreamer(GRefPtr<GstSample>&&);
    GRefPtr<GstSample> m_sample;
    PlatformImagePtr m_image;
    FloatRect m_cropRect;
#if USE(CAIRO) || PLATFORM(QT)
    GstVideoFrame m_videoFrame;
    bool m_frameMapped { false };
#endif
    FloatSize m_size;
    bool m_hasAlpha { false };
};

}
#endif // ENABLE(VIDEO) && USE(GSTREAMER)
