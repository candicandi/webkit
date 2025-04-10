/*
 * Copyright (C) 2010 Apple Inc. All rights reserved.
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
#include "WebInspectorProxy.h"

#include <WebCore/NotImplemented.h>
#include <wtf/text/WTFString.h>

namespace WebKit {

void WebKit::WebInspectorProxy::platformInvalidate()
{
    notImplemented();
}

void WebInspectorProxy::platformHide()
{
    notImplemented();
}

void WebInspectorProxy::platformBringToFront()
{
    notImplemented();
}

bool WebInspectorProxy::platformIsFront()
{
    notImplemented();
    return false;
}

void WebInspectorProxy::platformDidCloseForCrash()
{
}

void WebInspectorProxy::platformInspectedURLChanged(const String&)
{
    notImplemented();
}

unsigned WebInspectorProxy::platformInspectedWindowHeight()
{
    notImplemented();
    return 0;
}

unsigned WebInspectorProxy::platformInspectedWindowWidth()
{
    notImplemented();
    return 0;
}

void WebInspectorProxy::platformAttach()
{
    notImplemented();
}

void WebInspectorProxy::platformDetach()
{
    notImplemented();
}

void WebInspectorProxy::platformAttachAvailabilityChanged(bool)
{
    notImplemented();
}

void WebInspectorProxy::platformSetAttachedWindowHeight(unsigned)
{
    notImplemented();
}

void WebInspectorProxy::platformSetAttachedWindowWidth(unsigned)
{
    notImplemented();
}

void WebInspectorProxy::platformSetForcedAppearance(WebCore::InspectorFrontendClient::Appearance)
{
    notImplemented();
}

void WebKit::WebInspectorProxy::platformStartWindowDrag()
{
    notImplemented();
}

void WebInspectorProxy::platformSave(const String&, const String&, bool, bool)
{
    notImplemented();
}

void WebInspectorProxy::platformAppend(const String&, const String&)
{
    notImplemented();
}

String WebInspectorProxy::inspectorPageURL()
{
    notImplemented();
    return String();
}

String WebInspectorProxy::inspectorTestPageURL()
{
    notImplemented();
    return String();
}

String WebInspectorProxy::inspectorBaseURL()
{
    notImplemented();
    return String();
}

DebuggableInfoData WebInspectorProxy::infoForLocalDebuggable()
{
    notImplemented();
    return DebuggableInfoData::empty();
}

void WebInspectorProxy::platformShowCertificate(const WebCore::CertificateInfo&)
{
    notImplemented();
}

void WebInspectorProxy::platformResetState()
{
    notImplemented();
}

void WebInspectorProxy::platformBringInspectedPageToFront()
{
    notImplemented();
}

void WebInspectorProxy::platformSetSheetRect(const WebCore::FloatRect&)
{
    notImplemented();
}

WebPageProxy* WebInspectorProxy::platformCreateFrontendPage()
{
}

void WebInspectorProxy::platformCreateFrontendWindow()
{
}

void WebInspectorProxy::platformCloseFrontendPageAndWindow()
{
}

} // namespace WebKit
