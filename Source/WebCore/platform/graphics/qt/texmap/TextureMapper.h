/*
 Copyright (C) 2010 Nokia Corporation and/or its subsidiary(-ies)

 This library is free software; you can redistribute it and/or
 modify it under the terms of the GNU Library General Public
 License as published by the Free Software Foundation; either
 version 2 of the License, or (at your option) any later version.

 This library is distributed in the hope that it will be useful,
 but WITHOUT ANY WARRANTY; without even the implied warranty of
 MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 Library General Public License for more details.

 You should have received a copy of the GNU Library General Public License
 along with this library; see the file COPYING.LIB.  If not, write to
 the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 Boston, MA 02110-1301, USA.
 */

#pragma once

#include "BitmapTexture.h"
#include "Color.h"
#include "IntRect.h"
#include "IntSize.h"
#include "TransformationMatrix.h"
#include <wtf/OptionSet.h>

/*
    TextureMapper is a mechanism that enables hardware acceleration of CSS animations (accelerated compositing) without
    a need for a platform specific scene-graph library like CoreAnimations or QGraphicsView.
*/

namespace WebCore {

class BitmapTexturePool;
class GraphicsLayer;
class TextureMapper;
class FilterOperations;
class FloatRoundedRect;

class TextureMapper {
    WTF_MAKE_FAST_ALLOCATED;
public:
    enum PaintFlag {
        PaintingMirrored = 1 << 0,
    };

    enum class WrapMode : uint8_t {
        Stretch,
        Repeat
    };

    typedef unsigned PaintFlags;

    WEBCORE_EXPORT static std::unique_ptr<TextureMapper> create();

    explicit TextureMapper();
    virtual ~TextureMapper();

    enum class AllEdgesExposed : bool { No, Yes };

    virtual void drawBorder(const Color&, float borderWidth, const FloatRect&, const TransformationMatrix&) = 0;
    virtual void drawNumber(int number, const Color&, const FloatPoint&, const TransformationMatrix&) = 0;

    virtual void drawTexture(const BitmapTexture&, const FloatRect& target, const TransformationMatrix& modelViewMatrix = TransformationMatrix(), float opacity = 1.0f, AllEdgesExposed = AllEdgesExposed::Yes) = 0;
    virtual void drawSolidColor(const FloatRect&, const TransformationMatrix&, const Color&, bool) = 0;
    virtual void clearColor(const Color&) = 0;

    // makes a surface the target for the following drawTexture calls.
    virtual void bindSurface(BitmapTexture* surface) = 0;
    virtual BitmapTexture* currentSurface() = 0;
    virtual void beginClip(const TransformationMatrix&, const FloatRoundedRect&) = 0;
    virtual void endClip() = 0;
    virtual IntRect clipBounds() = 0;
    virtual Ref<BitmapTexture> createTexture() = 0;
    virtual void setDepthRange(double zNear, double zFar) = 0;

    virtual void beginPainting(PaintFlags = 0, BitmapTexture* = nullptr) { }
    virtual void endPainting() { }

    void setMaskMode(bool m) { m_isMaskMode = m; }

    virtual IntSize maxTextureSize() const = 0;

    virtual RefPtr<BitmapTexture> acquireTextureFromPool(const IntSize&, OptionSet<BitmapTexture::Flags> = { BitmapTexture::Flags::SupportsAlpha });
    virtual RefPtr<BitmapTexture> applyFilters(RefPtr<BitmapTexture>&, const FilterOperations&, bool) { return currentSurface(); }

    void setPatternTransform(const TransformationMatrix& p) { m_patternTransform = p; }
    void setWrapMode(WrapMode m) { m_wrapMode = m; }

protected:
    std::unique_ptr<BitmapTexturePool> m_texturePool;

    bool isInMaskMode() const { return m_isMaskMode; }
    WrapMode wrapMode() const { return m_wrapMode; }
    const TransformationMatrix& patternTransform() const { return m_patternTransform; }

private:
    static std::unique_ptr<TextureMapper> platformCreateAccelerated()
    {
        return nullptr;
    }

    bool m_isMaskMode { false };
    TransformationMatrix m_patternTransform;
    WrapMode m_wrapMode { WrapMode::Stretch };
};

}
