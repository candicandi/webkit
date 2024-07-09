/*
 * Copyright (C) 2006 Dirk Mueller <mueller@kde.org>
 * Copyright (C) 2006 Zack Rusin <zack@kde.org>
 * Copyright (C) 2006 Simon Hausmann <hausmann@kde.org>
 * Copyright (C) 2009 Torch Mobile Inc. http://www.torchmobile.com/
 * Copyright (C) 2010 Sencha, Inc.
 *
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE COMPUTER, INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE COMPUTER, INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "NativeImage.h"

#include "AffineTransform.h"
#include "BitmapImage.h"
#include "FloatRect.h"
#include "GraphicsContext.h"
#include "ImageObserver.h"
#include "ShadowBlur.h"
#include "StillImageQt.h"
#include "NativeImage.h"
#include "Timer.h"
#include "GraphicsContextQt.h"
#include "NotImplemented.h"

#include <QColorSpace>
#include <QPainter>
#include <QPaintEngine>
#include <QPixmap>
#include <QPixmapCache>

#include <private/qhexstring_p.h>

namespace WebCore {

IntSize PlatformImageNativeImageBackend::size() const
{
    return IntSize(m_platformImage.size());
}

bool PlatformImageNativeImageBackend::hasAlpha() const
{
    return m_platformImage.hasAlphaChannel();
}

DestinationColorSpace PlatformImageNativeImageBackend::colorSpace() const
{
    QColorSpace colorSpace = m_platformImage.colorSpace();

    if (colorSpace == QColorSpace::SRgb)
        return DestinationColorSpace::SRGB();
#if ENABLE(PREDEFINED_COLOR_SPACE_DISPLAY_P3)
    if (colorSpace == QColorSpace::DisplayP3)
        return DestinationColorSpace::DisplayP3();
#endif
    return DestinationColorSpace::SRGB();
}

std::optional<Color> NativeImage::singlePixelSolidColor() const
{
    if (size() != IntSize(1, 1))
        return std::nullopt;

    return QColor::fromRgba(platformImage().pixel(0, 0));
}

void NativeImage::clearSubimages()
{
}

void NativeImage::draw(GraphicsContext& context, const FloatRect& destinationRect, const FloatRect& sourceRect, ImagePaintingOptions options)
{
    context.drawNativeImageInternal(*this, destinationRect, sourceRect, options);
}

} // namespace WebCore
