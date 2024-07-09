/*
 * Copyright (C) 2019 Konstantin Tokarev <annulen@yandex.ru>
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
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS''
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "PublicSuffixStore.h"

#include <QVector>
#include <private/qtldurl_p.h>
#include <private/qurl_p.h>
#include <wtf/URL.h>

namespace WebCore {

static inline QByteArray asciiStringToByteArrayNoCopy(const String& string)
{
    ASSERT(string.is8Bit());
    return QByteArray::fromRawData(reinterpret_cast<const char*>(string.span8().data()), string.span8().size());
}

static QString fromAce(StringView domain)
{
    if (domain.is8Bit())
        return QUrl::fromAce(asciiStringToByteArrayNoCopy(domain.convertToASCIILowercase()));
    return QString(domain.toString()).toLower();
}

bool PublicSuffixStore::platformIsPublicSuffix(StringView domain) const
{
    if (domain.isEmpty())
        return false;

    return qIsEffectiveTLD(fromAce(domain));
}

static QString qTopLevelDomain(QStringView domain)
{
    QVector<QStringView> sections = domain.split(QLatin1Char('.'), Qt::SkipEmptyParts);
    if (sections.isEmpty())
        return QString();

    QString level, tld;
    for (int j = sections.count() - 1; j >= 0; --j) {
        level.prepend(QLatin1Char('.') + sections.at(j));
        if (qIsEffectiveTLD(level.right(level.size() - 1)))
            tld = level;
    }
    return tld;
}

String PublicSuffixStore::platformTopPrivatelyControlledDomain(StringView domain) const
{
    QString qDomain;
    if (domain.is8Bit())
        qDomain = QString::fromUtf8(reinterpret_cast<const char*>(domain.span8().data()), domain.span8().size());
    else
        qDomain = QString(reinterpret_cast<const QChar*>(domain.span16().data()), static_cast<qsizetype>(domain.span16().size()));

    if (qIsEffectiveTLD(qDomain))
        return String();

    QString tld = qTopLevelDomain(qDomain);
    auto privateLabels = qDomain.left(qDomain.length() - tld.length());
    auto topPrivateLabel = privateLabels.split(QLatin1Char('.'), Qt::SkipEmptyParts).last();
    return QString(topPrivateLabel + tld);
}

} // namespace WebCore
