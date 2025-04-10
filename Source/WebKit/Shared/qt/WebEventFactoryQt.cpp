/*
 * Copyright (C) 2010 Apple Inc. All rights reserved.
 * Copyright (C) 2010 Nokia Corporation and/or its subsidiary(-ies)
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
#include "WebEventFactory.h"

#include <QKeyEvent>
#include <QLineF>
#include <QTransform>
#include <WebCore/FloatPoint.h>
#include <WebCore/FloatSize.h>
#include <WebCore/IntPoint.h>
#include <WebCore/PlatformKeyboardEvent.h>
#include <wtf/WallTime.h>

using namespace WebCore;

namespace WebKit {

static inline WallTime currentTimeForEvent(const QInputEvent* event)
{
    ASSERT(event);

    // Use the input event timestamps if they are available.
    // These timestamps are in milliseconds, thus convert them to seconds.
    if (event->timestamp())
        return WallTime::fromRawSeconds(event->timestamp()/1000);

    return WTF::WallTime::now();
}

static inline unsigned short buttonsForEvent(const QMouseEvent* event)
{
    unsigned short buttons = 0;

    if(event->buttons() & Qt::LeftButton)
        buttons |= 1;
    
    if(event->buttons() & Qt::RightButton)
        buttons |= 4;

    if(event->buttons() & Qt::MiddleButton)
        buttons |= 2;

    return buttons;
}

static WebMouseEvent::Button mouseButtonForEvent(const QMouseEvent *event)
{
    if (event->button() == Qt::LeftButton || (event->buttons() & Qt::LeftButton))
        return WebMouseEvent::LeftButton;
    if (event->button() == Qt::RightButton || (event->buttons() & Qt::RightButton))
        return WebMouseEvent::RightButton;
    if (event->button() == Qt::MiddleButton || (event->buttons() & Qt::MiddleButton))
        return WebMouseEvent::MiddleButton;
    return WebMouseEvent::NoButton;
}

static WebEvent::Type webEventTypeForEvent(const QEvent* event)
{
    switch (event->type()) {
    case QEvent::MouseButtonPress:
        return WebEvent::MouseDown;
    case QEvent::MouseButtonRelease:
        return WebEvent::MouseUp;
    case QEvent::MouseMove:
        return WebEvent::MouseMove;
    case QEvent::Wheel:
        return WebEvent::Wheel;
    case QEvent::KeyPress:
        return WebEvent::KeyDown;
    case QEvent::KeyRelease:
        return WebEvent::KeyUp;
#if ENABLE(TOUCH_EVENTS)
    case QEvent::TouchBegin:
        return WebEvent::TouchStart;
    case QEvent::TouchUpdate:
        return WebEvent::TouchMove;
    case QEvent::TouchEnd:
        return WebEvent::TouchEnd;
    case QEvent::TouchCancel:
        return WebEvent::TouchCancel;
#endif
    case QEvent::MouseButtonDblClick:
        ASSERT_NOT_REACHED();
        return WebEvent::NoType;
    default:
        // assert
        return WebEvent::MouseMove;
    }
}

static inline OptionSet<WebEvent::Modifier> modifiersForEvent(Qt::KeyboardModifiers modifiers)
{
    OptionSet<WebEvent::Modifier> result;
    if (modifiers & Qt::ShiftModifier)
        result.add(WebEvent::Modifier::ShiftKey);
    if (modifiers & Qt::ControlModifier)
        result.add(WebEvent::Modifier::ControlKey);
    if (modifiers & Qt::AltModifier)
        result.add(WebEvent::Modifier::AltKey);
    if (modifiers & Qt::MetaModifier)
        result.add(WebEvent::Modifier::MetaKey);
    return result;
}

WebMouseEvent WebEventFactory::createWebMouseEvent(const QMouseEvent* event, const QTransform& fromItemTransform, int eventClickCount)
{
    static FloatPoint lastPos = FloatPoint(0, 0);

    WebEvent::Type type             = webEventTypeForEvent(event);
    WebMouseEvent::Button button    = mouseButtonForEvent(event);
    auto buttons                    = buttonsForEvent(event);
    float deltaX                    = event->pos().x() - lastPos.x();
    float deltaY                    = event->pos().y() - lastPos.y();
    int clickCount                  = eventClickCount;
    OptionSet<WebEvent::Modifier> modifiers = modifiersForEvent(event->modifiers());
    WallTime timestamp                = currentTimeForEvent(event);
    lastPos.set(event->localPos().x(), event->localPos().y());

    return WebMouseEvent(type, button, buttons, fromItemTransform.map(event->localPos()).toPoint(), event->screenPos().toPoint(), deltaX, deltaY, 0.0f, clickCount, modifiers, timestamp);
}

    WebWheelEvent WebEventFactory::createWebWheelEvent(const QWheelEvent* e, const QTransform& fromItemTransform)
{
    float deltaX                            = 0;
    float deltaY                            = 0;
    float wheelTicksX                       = 0;
    float wheelTicksY                       = 0;
    WebWheelEvent::Granularity granularity  = WebWheelEvent::ScrollByPixelWheelEvent;
    OptionSet<WebEvent::Modifier> modifiers           = modifiersForEvent(e->modifiers());
    WallTime timestamp                        = currentTimeForEvent(e);

    deltaX = e->angleDelta().x();
    wheelTicksX = deltaX / 120.0f;
    deltaY = e->angleDelta().y();
    wheelTicksY = deltaY / 120.0f;

    // Since we report the scroll by the pixel, convert the delta to pixel distance using standard scroll step.
    // Use the same single scroll step as QTextEdit (in QTextEditPrivate::init [h,v]bar->setSingleStep)
    static const float cDefaultQtScrollStep = 20.f;
    // ### FIXME: Default from QtGui. Should use Qt platform theme API once configurable.
    const int wheelScrollLines = 3;
    deltaX = wheelTicksX * wheelScrollLines * cDefaultQtScrollStep;
    deltaY = wheelTicksY * wheelScrollLines * cDefaultQtScrollStep;

    // Transform the position and the pixel scrolling distance.
    QLineF transformedScroll = fromItemTransform.map(QLineF(e->position(), e->position() + QPointF(deltaX, deltaY)));
    IntPoint transformedPoint = transformedScroll.p1().toPoint();
    IntPoint globalPoint = e->globalPosition().toPoint();
    FloatSize transformedDelta(transformedScroll.dx(), transformedScroll.dy());
    FloatSize wheelTicks(wheelTicksX, wheelTicksY);
    return WebWheelEvent(WebEvent::Wheel, transformedPoint, globalPoint, transformedDelta, wheelTicks, granularity, modifiers, timestamp);
}

WebKeyboardEvent WebEventFactory::createWebKeyboardEvent(const QKeyEvent* event)
{
    const int state                 = event->modifiers();
    WebEvent::Type type             = webEventTypeForEvent(event);
    const String text               = event->text();
    const String unmodifiedText     = event->text();
    bool isAutoRepeat               = event->isAutoRepeat();
    bool isSystemKey                = false; // FIXME: No idea what that is.
    bool isKeypad                   = (state & Qt::KeypadModifier);
    const String keyIdentifier      = keyIdentifierForQtKeyCode(event->key());
    const String key                = text.isEmpty() ? keyIdentifier : text;
    const String code               = keyCodeForQtKeyEvent(event);
    int windowsVirtualKeyCode       = windowsKeyCodeForKeyEvent(event->key(), isKeypad);
    int nativeVirtualKeyCode        = event->nativeVirtualKey();
    int macCharCode                 = 0;
    OptionSet<WebEvent::Modifier> modifiers           = modifiersForEvent(event->modifiers());
    WallTime timestamp                = currentTimeForEvent(event);

    return WebKeyboardEvent(type, text, unmodifiedText, key, code, keyIdentifier, windowsVirtualKeyCode, nativeVirtualKeyCode, macCharCode, isAutoRepeat, isKeypad, isSystemKey, modifiers, timestamp);
}

#if ENABLE(TOUCH_EVENTS)
WebTouchEvent WebEventFactory::createWebTouchEvent(const QTouchEvent* event, const QTransform& fromItemTransform)
{
    WebEvent::Type type  = webEventTypeForEvent(event);
    WebPlatformTouchPoint::TouchPointState state = static_cast<WebPlatformTouchPoint::TouchPointState>(0);
    unsigned id;
    OptionSet<WebEvent::Modifier> modifiers           = modifiersForEvent(event->modifiers());
    double timestamp                = currentTimeForEvent(event);

    const QList<QTouchEvent::TouchPoint>& points = event->touchPoints();
    
    Vector<WebPlatformTouchPoint> touchPoints;
    for (int i = 0; i < points.count(); ++i) {
        const QTouchEvent::TouchPoint& touchPoint = points.at(i);
        id = static_cast<unsigned>(touchPoint.id());
        switch (touchPoint.state()) {
        case Qt::TouchPointReleased: 
            state = WebPlatformTouchPoint::TouchReleased; 
            break;
        case Qt::TouchPointMoved: 
            state = WebPlatformTouchPoint::TouchMoved; 
            break;
        case Qt::TouchPointPressed: 
            state = WebPlatformTouchPoint::TouchPressed; 
            break;
        case Qt::TouchPointStationary: 
            state = WebPlatformTouchPoint::TouchStationary; 
            break;
        default:
            ASSERT_NOT_REACHED();
            break;
        }

        // Qt does not have a Qt::TouchPointCancelled point state, so if we receive a touch cancel event,
        // simply cancel all touch points here.
        if (type == WebEvent::TouchCancel)
            state = WebPlatformTouchPoint::TouchCancelled;

        IntSize radius(touchPoint.rect().width()/ 2, touchPoint.rect().height() / 2);
        touchPoints.append(WebPlatformTouchPoint(id, state, touchPoint.screenPos().toPoint(), fromItemTransform.map(touchPoint.pos()).toPoint(), radius, 0.0, touchPoint.pressure()));
    }

    return WebTouchEvent(type, WTFMove(touchPoints), modifiers, timestamp);
}
#endif

} // namespace WebKit
