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

#ifndef WKNativeEvent_h
#define WKNativeEvent_h

#ifdef __APPLE__
#include <TargetConditionals.h>
#endif

#ifdef __cplusplus
extern "C" {
#endif

#if defined(__APPLE__) && !TARGET_OS_IPHONE && !defined(BUILDING_GTK__) && !defined(BUILDING_QT__)
#ifdef __OBJC__
@class NSEvent;
#elif __cplusplus
class NSEvent;
#else
struct NSEvent;
#endif
typedef NSEvent *WKNativeEventPtr;
#elif defined(BUILDING_GTK__)
typedef const GdkEvent* WKNativeEventPtr;
#elif defined(WIN32)
typedef const struct tagMSG* WKNativeEventPtr;
#elif !defined(BUILDING_QT__)
typedef const void* WKNativeEventPtr;
#endif

#ifdef __cplusplus
}
#endif

#if defined(BUILDING_QT__)
#include <qglobal.h>
QT_BEGIN_NAMESPACE
class QEvent;
QT_END_NAMESPACE
typedef const QEvent* WKNativeEventPtr;
#endif

#endif /* WKNativeEvent_h */
