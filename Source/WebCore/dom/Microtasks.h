/*
 * Copyright (C) 2014 Yoav Weiss (yoav@yoav.ws)
 * Copyright (C) 2015 Akamai Technologies Inc. All rights reserved.
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
 *
 */

#pragma once

#include "EventLoop.h"
#include <JavaScriptCore/MicrotaskQueue.h>
#include <wtf/Forward.h>
#include <wtf/TZoneMalloc.h>
#include <wtf/Vector.h>
#include <wtf/WeakPtr.h>

namespace JSC {
class VM;
} // namespace JSC

namespace WebCore {

class WebCoreMicrotaskDispatcher : public JSC::MicrotaskDispatcher {
    WTF_MAKE_TZONE_ALLOCATED(WebCoreMicrotaskDispatcher);
public:
    enum class Type {
        JavaScript,
        UserGestureIndicator,
        Function,
    };

    WebCoreMicrotaskDispatcher(Type type, EventLoopTaskGroup& group)
        : m_type(type)
        , m_group(group)
    {
    }

    Type type() const { return m_type; }

    bool isRunnable() const final
    {
        return currentRunnability() == JSC::QueuedTask::Result::Executed;
    }

    JSC::QueuedTask::Result currentRunnability() const;

private:
    Type m_type { Type::JavaScript };
    WeakPtr<EventLoopTaskGroup> m_group;
};

class MicrotaskQueue final {
    WTF_MAKE_TZONE_ALLOCATED_EXPORT(MicrotaskQueue, WEBCORE_EXPORT);
public:
    WEBCORE_EXPORT MicrotaskQueue(JSC::VM&, EventLoop&);
    WEBCORE_EXPORT ~MicrotaskQueue();

    WEBCORE_EXPORT void append(JSC::QueuedTask&&);
    WEBCORE_EXPORT void performMicrotaskCheckpoint();

    WEBCORE_EXPORT void addCheckpointTask(std::unique_ptr<EventLoopTask>&&);

    bool isEmpty() const { return m_microtaskQueue.isEmpty(); }
    bool hasMicrotasksForFullyActiveDocument() const;

private:
    JSC::VM& vm() const { return m_vm.get(); }

    bool m_performingMicrotaskCheckpoint { false };
    // For the main thread the VM lives forever. For workers it's lifetime is tied to our owning WorkerGlobalScope. Regardless, we retain the VM here to be safe.
    Ref<JSC::VM> m_vm;
    WeakPtr<EventLoop> m_eventLoop;
    JSC::MicrotaskQueue m_microtaskQueue;

    EventLoop::TaskVector m_checkpointTasks;
};

} // namespace WebCore
