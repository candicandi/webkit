/*
    Copyright (C) 2008 Nokia Corporation and/or its subsidiary(-ies)
    Copyright (C) 2008 Holger Hans Peter Freyther

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

    This class provides all functionality needed for loading images, style sheets and html
    pages from the web. It has a memory cache for these objects.
*/
#include "config.h"
#include "GlyphPage.h"

#include "Font.h"
#include <QFontMetricsF>
#include <QTextLayout>

namespace WebCore {

bool GlyphPage::fill(std::span<const UChar> buffer)
{
    QRawFont rawFont = font().platformData().rawFont();
    QString qstring = QString::fromRawData(reinterpret_cast<const QChar*>(buffer.data()), static_cast<int>(buffer.size()));
    QVector<quint32> indexes = rawFont.glyphIndexesForString(qstring);
    unsigned indexSize = (unsigned)indexes.size();

    bool haveGlyphs = false;

    for (unsigned i = 0; i < GlyphPage::size; ++i) {
        Glyph glyph = (i < indexSize) ? indexes.at(i) : 0;
        // FIXME: https://bugs.webkit.org/show_bug.cgi?id=259205 Determine if the glyph is a color glyph or not.
        if (!glyph)
            setGlyphForIndex(i, 0, ColorGlyphType::Outline);
        else {
            haveGlyphs = true;
            setGlyphForIndex(i, glyph, font().colorGlyphType(glyph));
        }
    }
    return haveGlyphs;
}

}
