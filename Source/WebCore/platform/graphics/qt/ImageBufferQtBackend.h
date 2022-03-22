/*
 * Copyright (C) 2008 Google Inc. All rights reserved.
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

#pragma once

#include "ImageBufferBackend.h"
#include <wtf/IsoMalloc.h>

namespace WebCore {

class ImageBufferQtBackend : public ImageBufferBackend {
    WTF_MAKE_ISO_ALLOCATED(ImageBufferQtBackend);
    WTF_MAKE_NONCOPYABLE(ImageBufferQtBackend);
public:
    static std::unique_ptr<ImageBufferQtBackend> create(const FloatSize &, float resolutionScale, ColorSpace, const HostWindow *);
    static std::unique_ptr<ImageBufferQtBackend> create(const FloatSize &, const GraphicsContext &);

    ImageBufferQtBackend(const FloatSize &logicalSize, const IntSize &backendSize, float resolutionScale, ColorSpace, std::unique_ptr<GraphicsContext>&& context, QImage nativeImage, Ref<Image> image);

    RefPtr<Image> copyImage(BackingStoreCopy = CopyBackingStore, PreserveResolution = PreserveResolution::No) const override;
    NativeImagePtr copyNativeImage(BackingStoreCopy = CopyBackingStore) const override;

    void draw(GraphicsContext &destContext, const FloatRect &destRect, const FloatRect &srcRect, const ImagePaintingOptions &options) override;
    void drawPattern(GraphicsContext &destContext, const FloatRect &destRect, const FloatRect &srcRect, const AffineTransform &patternTransform, const FloatPoint &phase, const FloatSize &spacing, const ImagePaintingOptions &) override;

    void transformColorSpace(ColorSpace srcColorSpace, ColorSpace destColorSpace);

    String toDataURL(const String &mimeType, Optional<double> quality, PreserveResolution) const override;
    Vector<uint8_t> toData(const String &mimeType, Optional<double> quality) const override;
    Vector<uint8_t> toBGRAData() const override;

    GraphicsContext& context() const override;

    RefPtr<ImageData> getImageData(AlphaPremultiplication outputFormat, const IntRect& srcRect) const override;
    void putImageData(AlphaPremultiplication inputFormat, const ImageData& imageData, const IntRect& srcRect, const IntPoint& destPoint) override;

protected:

    using ImageBufferBackend::ImageBufferBackend;

    ColorFormat backendColorFormat() const override { return ColorFormat::BGRA; }
    static void initPainter(QPainter *painter);
    void platformTransformColorSpace(const std::array<uint8_t, 256>& lookUpTable);

    QImage m_nativeImage;
    RefPtr<Image> m_image;
    std::unique_ptr<GraphicsContext> m_context;
};

} // namespace WebCore
