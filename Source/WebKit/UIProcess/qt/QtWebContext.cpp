/*
 * Copyright (C) 2011 Nokia Corporation and/or its subsidiary(-ies)
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public License
 * along with this program; see the file COPYING.LIB.  If not, write to
 * the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 *
 */

#include "config.h"
#include "QtWebContext.h"

#include "QtDownloadManager.h"
#include "QtWebIconDatabaseClient.h"
#include "qquickwebview_p_p.h"
#include <QDir>
#include <QStandardPaths>
#include <QStringBuilder>
#include <WKArray.h>
#include <WKContextPrivate.h>
#include <WKData.h>
#include <WKPage.h>
#include <WKString.h>
#include <WKStringQt.h>
#include <WKType.h>

namespace WebKit {

// Prevent the destruction of the WKContext for two reasons:
// - An internal reference is kept to the WebContext waiting until the web process shut down, which
// does so 60 seconds after the last page closed. We want to reuse that web process if possible and
// avoid creating a second parallel WKContext + web process.
// - The IconDatabase wasn't designed to have more than one instance, or to be quickly opened/closed.
static QtWebContext* s_defaultQtWebContext = 0;

static void initInspectorServer()
{
#if ENABLE(INSPECTOR_SERVER)
    QString inspectorEnv = QString::fromUtf8(qgetenv("QTWEBKIT_INSPECTOR_SERVER"));
    if (!inspectorEnv.isEmpty()) {
        QString bindAddress = QStringLiteral("127.0.0.1");
        QString portStr = inspectorEnv;
        int port = 0;

        int portColonPos = inspectorEnv.lastIndexOf(':');
        if (portColonPos != -1) {
            portStr = inspectorEnv.mid(portColonPos + 1);
            bindAddress = inspectorEnv.mid(0, portColonPos);
        }

        bool ok = false;
        port = portStr.toInt(&ok);
        if (!ok) {
            qWarning("Non numeric port for the inspector server \"%s\". Examples of valid input: \"12345\" or \"192.168.2.14:12345\" (with the address of one of this host's interface).", qPrintable(portStr));
            return;
        }

        bool success = WebInspectorServer::singleton().listen(bindAddress, port);
        if (success) {
            QString inspectorServerUrl = QStringLiteral("http://%1:%2").arg(bindAddress).arg(port);
            qWarning("Inspector server started successfully. Try pointing a WebKit browser to %s", qPrintable(inspectorServerUrl));
        } else
            qWarning("Couldn't start the inspector server on bind address \"%s\" and port \"%d\". In case of invalid input, try something like: \"12345\" or \"192.168.2.14:12345\" (with the address of one of this host's interface).", qPrintable(bindAddress), port);
    }
#endif
}

static void globalInitialization()
{
    static bool initialized = false;
    if (initialized)
        return;

    initInspectorServer();
    initialized = true;
}

static void didReceiveMessageFromInjectedBundle(WKContextRef, WKStringRef messageName, WKTypeRef messageBody, const void*)
{
    if (!WKStringIsEqualToUTF8CString(messageName, "MessageFromNavigatorQtObject")
#if ENABLE(QT_WEBCHANNEL)
        && !WKStringIsEqualToUTF8CString(messageName, "MessageFromNavigatorQtWebChannelTransportObject")
#endif
        )
    {
        return;
    }

    ASSERT(messageBody);
    ASSERT(WKGetTypeID(messageBody) == WKArrayGetTypeID());

    WKArrayRef body = static_cast<WKArrayRef>(messageBody);
    ASSERT(WKArrayGetSize(body) == 2);
    ASSERT(WKGetTypeID(WKArrayGetItemAtIndex(body, 0)) == WKPageGetTypeID());

    WKPageRef page = static_cast<WKPageRef>(WKArrayGetItemAtIndex(body, 0));

    if (WKStringIsEqualToUTF8CString(messageName, "MessageFromNavigatorQtObject")) {
        ASSERT(WKGetTypeID(WKArrayGetItemAtIndex(body, 1)) == WKStringGetTypeID());
        WKStringRef data = static_cast<WKStringRef>(WKArrayGetItemAtIndex(body, 1));
        QQuickWebViewPrivate::get(page)->didReceiveMessageFromNavigatorQtObject(data);
    }
#if ENABLE(QT_WEBCHANNEL)
    else if (WKStringIsEqualToUTF8CString(messageName, "MessageFromNavigatorQtWebChannelTransportObject")) {
        ASSERT(WKGetTypeID(WKArrayGetItemAtIndex(body, 1)) == WKDataGetTypeID());
        WKDataRef data = static_cast<WKDataRef>(WKArrayGetItemAtIndex(body, 1));
        QQuickWebViewPrivate::get(page)->didReceiveMessageFromNavigatorQtWebChannelTransportObject(data);
    }
#endif
}

static void initializeContextInjectedBundleClient(WKContextRef context)
{
    WKContextInjectedBundleClientV0 injectedBundleClient;
    memset(&injectedBundleClient, 0, sizeof(WKContextInjectedBundleClientV0));
    injectedBundleClient.base.version = 0;
    injectedBundleClient.didReceiveMessageFromInjectedBundle = didReceiveMessageFromInjectedBundle;
    WKContextSetInjectedBundleClient(context, &injectedBundleClient.base);
}

QtWebContext::QtWebContext(WebProcessPool* context)
    : m_context(adoptRef(context))
    , m_downloadManager(new QtDownloadManager(toAPI(context)))
    , m_iconDatabase(new QtWebIconDatabaseClient(toAPI(context)))
{//Fix QtDownloadManager and QtWebIconDB to stop using WK* stuff
}

QtWebContext::~QtWebContext()
{
}

// Used directly only by WebKitTestRunner.
QtWebContext* QtWebContext::create(WebProcessPool* context)
{
    globalInitialization();
    return new QtWebContext(context);
}

QtWebContext* QtWebContext::defaultContext()
{
    if (!s_defaultQtWebContext) {
        auto configuration = API::ProcessPoolConfiguration::create();
        WebProcessPool* context = &WebProcessPool::create(configuration).leakRef();
        // Make sure for WebKitTestRunner that the injected bundle client isn't initialized
        // and that the page cache isn't enabled (defaultContext() isn't used there).
        initializeContextInjectedBundleClient(toAPI(context));
        // A good all-around default.
        WKContextSetCacheModel(toAPI(context), kWKCacheModelDocumentBrowser);

        // Those paths have to be set before the first web process is spawned.
// QTFIXME
//        WKContextSetDatabaseDirectory(wkContext.get(), adoptWK(WKStringCreateWithQString(preparedStoragePath(DatabaseStorage))).get());
//        WKContextSetLocalStorageDirectory(wkContext.get(), adoptWK(WKStringCreateWithQString(preparedStoragePath(LocalStorage))).get());
//        WKContextSetCookieStorageDirectory(toAPI(context), adoptWK(WKStringCreateWithQString(preparedStoragePath(CookieStorage))).get());
//        WKContextSetDiskCacheDirectory(wkContext.get(), adoptWK(WKStringCreateWithQString(preparedStoragePath(DiskCacheStorage))).get());

        s_defaultQtWebContext = QtWebContext::create(context);
    }

    return s_defaultQtWebContext;
}

static QString defaultLocation(QStandardPaths::StandardLocation type)
{
    QString path = QStandardPaths::writableLocation(type);
    Q_ASSERT(!path.isEmpty());
    return path % QDir::separator() % QStringLiteral(".QtWebKit") % QDir::separator();
}

QString QtWebContext::preparedStoragePath(StorageType type)
{
    QString path;
    switch (type) {
    case ApplicationCacheStorage:
        path = defaultLocation(QStandardPaths::AppLocalDataLocation) % QStringLiteral("Applications");
        QDir::root().mkpath(path);
        break;
    case DatabaseStorage:
        path = defaultLocation(QStandardPaths::AppLocalDataLocation) % QStringLiteral("Databases");
        QDir::root().mkpath(path);
        break;
    case LocalStorage:
        path = defaultLocation(QStandardPaths::AppLocalDataLocation) % QStringLiteral("LocalStorage");
        QDir::root().mkpath(path);
        break;
    case CookieStorage:
        path = defaultLocation(QStandardPaths::AppLocalDataLocation);
        QDir::root().mkpath(path);
        break;
    case DiskCacheStorage:
#if ENABLE(NETWORK_CACHE)
        path = defaultLocation(QStandardPaths::CacheLocation) % QStringLiteral("WebKitCache");
#else
        path = defaultLocation(QStandardPaths::CacheLocation) % QStringLiteral("DiskCache");
#endif
        QDir::root().mkpath(path);
        break;
    case IconDatabaseStorage:
        path = defaultLocation(QStandardPaths::AppLocalDataLocation);
        QDir::root().mkpath(path);
        path += QStringLiteral("WebpageIcons.db");
        break;
    default:
        Q_ASSERT(false);
    }

    return path;
}

} // namespace WebKit
