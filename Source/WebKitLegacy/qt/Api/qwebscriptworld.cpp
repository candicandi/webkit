/*
    Copyright (C) 2010 Robert Hogan <robert@roberthogan.net>

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

#include "qwebscriptworld.h"
#include "qwebscriptworld_p.h"

#include <QStringList>
#include <WebCore/ScriptController.h>
#include <wtf/URL.h>

using namespace WebCore;

/*!
    Constructs a security origin from \a other.
*/
QWebScriptWorld::QWebScriptWorld()
{
    d = new QWebScriptWorldPrivate(ScriptController::createWorld("WebScriptWorld"_s));
}

QWebScriptWorld::QWebScriptWorld(const QWebScriptWorld& other)
    : d(other.d)
{
}

QWebScriptWorld &QWebScriptWorld::operator=(const QWebScriptWorld& other)
{
    d = other.d;
    return *this;
}

DOMWrapperWorld* QWebScriptWorld::world() const
{
    return d ? d->world.ptr() : nullptr;
}

/*!
    Destroys the security origin.
*/
QWebScriptWorld::~QWebScriptWorld()
{
}

