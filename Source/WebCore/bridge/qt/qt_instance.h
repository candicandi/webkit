/*
 * Copyright (C) 2008 Nokia Corporation and/or its subsidiary(-ies)
 *
 *  This library is free software; you can redistribute it and/or
 *  modify it under the terms of the GNU Lesser General Public
 *  License as published by the Free Software Foundation; either
 *  version 2 of the License, or (at your option) any later version.
 *
 *  This library is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 *  Lesser General Public License for more details.
 *
 *  You should have received a copy of the GNU Lesser General Public
 *  License along with this library; if not, write to the Free Software
 *  Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 *
 */

#ifndef qt_instance_h
#define qt_instance_h

#include "BridgeJSC.h"
#include "runtime_root.h"
#include <JavaScriptCore/JSWeakObjectMapRefPrivate.h>
#include <JavaScriptCore/Weak.h>
#include <JavaScriptCore/WeakInlines.h>
#include <QPointer>
#include <qhash.h>
#include <qset.h>

namespace JSC {

namespace Bindings {

class QtClass;
class QtField;
class QtRuntimeMethod;

class WeakMapImpl : public RefCounted<WeakMapImpl> {
public:
    WeakMapImpl(JSContextGroupRef);
    ~WeakMapImpl();

    JSGlobalContextRef m_context;
    JSWeakObjectMapRef m_map;
};

class WeakMap {
public:
    ~WeakMap();

    void set(JSContextRef, void* key, JSObjectRef);
    JSObjectRef get(void* key);
    void remove(void* key);

private:
    RefPtr<WeakMapImpl> m_impl;
};

class QtInstance final : public Instance {
public:
    enum ValueOwnership {
        QtOwnership,
        ScriptOwnership,
        AutoOwnership
    };

    ~QtInstance();

    Class* getClass() const override;
    RuntimeObject* newRuntimeObject(JSGloablObject*) final;

    JSValue valueOf(JSGlobalObject*) const final;
    JSValue defaultValue(JSGlobalObject*, PreferredPrimitiveType) const final;

    JSValue getMethod(JSGlobalObject*, PropertyName) final;
    JSValue invokeMethod(JSGlobalObject*, RuntimeMethod*) final;

    void getPropertyNames(JSGlobalObject*, PropertyNameArray&) final;

    JSValue stringValue(JSGlobalObject*) const;
    JSValue numberValue(JSGlobalObject*) const;
    JSValue booleanValue() const;

    QObject* getObject() const { return m_object.data(); }
    QObject* hashKey() const { return m_hashkey; }

    static RefPtr<QtInstance> getQtInstance(QObject*, RootObject*, ValueOwnership);

    bool getOwnPropertySlot(JSObject*, JSGlobalObject*, PropertyName, PropertySlot&) final;
    bool put(JSObject*, JSGlobalObject*, PropertyName, JSValue, PutPropertySlot&) final;

    static QtInstance* getInstance(JSGlobalObject*, JSObject*);

private:
    static RefPtr<QtInstance> create(QObject *instance, RefPtr<RootObject>&& rootObject, ValueOwnership ownership)
    {
        return adoptRef(new QtInstance(instance, WTFMove(rootObject), ownership));
    }

    friend class QtClass;
    friend class QtField;
    friend class QtRuntimeMethod;
    QtInstance(QObject*, RefPtr<RootObject>&&, ValueOwnership); // Factory produced only..
    mutable QtClass* m_class;
    QPointer<QObject> m_object;
    QObject* m_hashkey;
    mutable QHash<QByteArray, QtRuntimeMethod*> m_methods;
    mutable QHash<QString, QtField*> m_fields;
    ValueOwnership m_ownership;
};

} // namespace Bindings

} // namespace JSC

#endif
