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
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#pragma once

#include <WebCore/DataListSuggestionPicker.h>

#if ENABLE(DATALIST_ELEMENT)

namespace WebCore {
class DataListSuggestionsClient;
}

class QWebPageAdapter;

namespace WebKit {

class DataListSuggestionPickerQt final : public WebCore::DataListSuggestionPicker, public RefCounted<DataListSuggestionPickerQt> {
    WTF_MAKE_TZONE_ALLOCATED_INLINE(DataListSuggestionPicker);
public:
    DataListSuggestionPickerQt(QWebPageAdapter&, WebCore::DataListSuggestionsClient&);
    ~DataListSuggestionPickerQt();

    void ref() const final { RefCounted::ref(); }
    void deref() const final { RefCounted::deref(); }

    void handleKeydownWithIdentifier(const String&) final;
    void close() final;
    void displayWithActivationType(WebCore::DataListSuggestionActivationType) final;
private:
    QWebPageAdapter& m_page;
    WebCore::DataListSuggestionsClient& m_dataListSuggestionsClient;

};

}

#endif
