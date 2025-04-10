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
#include "ProcessLauncher.h"

#include "Connection.h"
#include "ProcessExecutablePath.h"
#include "WebProcess.h"
#include <QDebug>
#include <QLocalServer>
#include <QMetaType>
#include <QProcess>
#include <QString>
#include <QtCore/qglobal.h>
#include <wtf/RunLoop.h>
#include <wtf/text/WTFString.h>

#if defined(Q_OS_UNIX)
#include <errno.h>
#include <fcntl.h>
#include <string>
#include <sys/resource.h>
#include <sys/socket.h>
#include <unistd.h>
#include <wtf/UniStdExtras.h>
#endif

#if defined(Q_OS_LINUX)
#include <signal.h>
#include <sys/prctl.h>
#endif

#if OS(WINDOWS)
#include <windows.h>
#endif

#if ENABLE(SUID_SANDBOX_LINUX)
#include <QCoreApplication>
#endif

#if USE(MACH_PORTS)
#include <mach/mach_init.h>
#include <servers/bootstrap.h>

extern "C" kern_return_t bootstrap_register2(mach_port_t, name_t, mach_port_t, uint64_t);
#endif

// for QNX we need SOCK_DGRAM, see https://bugs.webkit.org/show_bug.cgi?id=95553
#if defined(SOCK_SEQPACKET) && !defined(Q_OS_MACOS) && !OS(QNX)
#define SOCKET_TYPE SOCK_SEQPACKET
#else
#define SOCKET_TYPE SOCK_DGRAM
#endif

using namespace WebCore;

namespace WebKit {

class QtWebProcess final : public QProcess {
    Q_OBJECT
public:
    QtWebProcess(QObject* parent = 0)
        : QProcess(parent)
    {
        setChildProcessModifier(&QtWebProcess::processModifier);
    }

protected:
    static void processModifier();
};

void QtWebProcess::processModifier()
{
#if defined(Q_OS_LINUX)
#ifndef NDEBUG
    if (qgetenv("QT_WEBKIT2_DEBUG") == "1")
        return;
#endif
    prctl(PR_SET_PDEATHSIG, SIGKILL);
#endif
#if defined(Q_OS_MACOS)
    qputenv("QT_MAC_DISABLE_FOREGROUND_APPLICATION_TRANSFORM", QByteArray("1"));
#endif
}

void ProcessLauncher::launchProcess()
{
    QString commandLine;
    if (m_launchOptions.processType == ProcessType::Web) {
        commandLine = QLatin1String("%1 \"%2\" %3 %4");
        QByteArray webProcessPrefix = qgetenv("QT_WEBKIT2_WP_CMD_PREFIX");
        commandLine = commandLine.arg(QLatin1String(webProcessPrefix.constData())).arg(QString(executablePathOfWebProcess()));
    } else if (m_launchOptions.processType == ProcessType::Network) {
        commandLine = QLatin1String("%1 \"%2\" %3 %4");
        QByteArray networkProcessPrefix = qgetenv("QT_WEBKIT2_NP_CMD_PREFIX");
        commandLine = commandLine.arg(QLatin1String(networkProcessPrefix.constData())).arg(QString(executablePathOfNetworkProcess()));
#if ENABLE(PLUGIN_PROCESS)
    } else if (m_launchOptions.processType == ProcessType::Plugin32 || m_launchOptions.processType == ProcessType::Plugin64) {
        commandLine = QLatin1String("%1 \"%2\" %3 %4");
        QByteArray pluginProcessPrefix = qgetenv("QT_WEBKIT2_PP_CMD_PREFIX");
        commandLine = commandLine.arg(QLatin1String(pluginProcessPrefix.constData())).arg(QString(executablePathOfPluginProcess()));
#endif
#if ENABLE(DATABASE_PROCESS)
    } else if (m_launchOptions.processType == ProcessType::Database) {
        commandLine = QLatin1String("%1 \"%2\" %3 %4");
        QByteArray processPrefix = qgetenv("QT_WEBKIT2_SP_CMD_PREFIX");
        commandLine = commandLine.arg(QLatin1String(processPrefix.constData())).arg(QString(executablePathOfDatabaseProcess()));
#endif
    } else {
        qDebug() << "Unsupported process type" << (int)m_launchOptions.processType;
        ASSERT_NOT_REACHED();
    }

#if USE(MACH_PORTS)
    // Create the listening port.
    mach_port_t connector;
    mach_port_allocate(mach_task_self(), MACH_PORT_RIGHT_RECEIVE, &connector);

    // Insert a send right so we can send to it.
    mach_port_insert_right(mach_task_self(), connector, connector, MACH_MSG_TYPE_MAKE_SEND);

    // Register port with a service name to the system.
    QString serviceName = QStringLiteral("org.qt-project.Qt.WebKit.QtWebProcess-%1-%2");
    serviceName = serviceName.arg(QString().setNum(getpid()), QString().setNum((size_t)this));
    kern_return_t kr = bootstrap_register2(bootstrap_port, const_cast<char*>(serviceName.toUtf8().data()), connector, 0);
    ASSERT_UNUSED(kr, kr == KERN_SUCCESS);

    commandLine = commandLine.arg(serviceName);
#elif OS(WINDOWS)
    IPC::Connection::Identifier connector, clientIdentifier;
    if (!IPC::Connection::createServerAndClientIdentifiers(connector, clientIdentifier)) {
        // FIXME: What should we do here?
        ASSERT_NOT_REACHED();
    }
    commandLine = commandLine.arg(qulonglong(clientIdentifier));
    // Ensure that the child process inherits the client identifier.
    ::SetHandleInformation(clientIdentifier, HANDLE_FLAG_INHERIT, HANDLE_FLAG_INHERIT);
#else
    int sockets[2];
    if (socketpair(AF_UNIX, SOCKET_TYPE, 0, sockets) == -1) {
        qDebug() << "Creation of socket failed with errno:" << errno;
        ASSERT_NOT_REACHED();
        return;
    }

    // Don't expose the ui socket to the web process
    while (fcntl(sockets[1], F_SETFD, FD_CLOEXEC)  == -1) {
        if (errno != EINTR) {
            ASSERT_NOT_REACHED();
            closeWithRetry(sockets[0]);
            closeWithRetry(sockets[1]);
            return;
        }
    }

    int connector = sockets[1];
    commandLine = commandLine.arg(sockets[0]);
#endif

    commandLine = commandLine.arg(m_launchOptions.processIdentifier.toUInt64());

#if ENABLE(PLUGIN_PROCESS)
    if (m_launchOptions.processType == ProcessType::Plugin32 || m_launchOptions.processType == ProcessType::Plugin64)
        commandLine = commandLine.arg(QString(m_launchOptions.extraInitializationData.get("plugin-path")));
#endif

    QProcess* webProcessOrSUIDHelper = new QtWebProcess();
    webProcessOrSUIDHelper->setProcessChannelMode(QProcess::ForwardedChannels);

#if ENABLE(SUID_SANDBOX_LINUX)
    if (m_launchOptions.processType == WebProcess) {
        QString sandboxCommandLine = QLatin1String("\"%1\" \"%2\" %3");
        sandboxCommandLine = sandboxCommandLine.arg(QCoreApplication::applicationDirPath() + QLatin1String("/SUIDSandboxHelper"));
        sandboxCommandLine = sandboxCommandLine.arg(executablePathOfWebProcess());
        sandboxCommandLine = sandboxCommandLine.arg(sockets[0]);

        webProcessOrSUIDHelper->start(sandboxCommandLine);
    } else
        webProcessOrSUIDHelper->start(commandLine);
#else
    webProcessOrSUIDHelper->start(commandLine);
#endif

#if OS(UNIX) && !OS(DARWIN)
    // Don't expose the web socket to possible future web processes
    while (fcntl(sockets[0], F_SETFD, FD_CLOEXEC) == -1) {
        if (errno != EINTR) {
            ASSERT_NOT_REACHED();
            delete webProcessOrSUIDHelper;
            return;
        }
    }
#endif

    if (!webProcessOrSUIDHelper->waitForStarted()) {
        qDebug() << "Failed to start" << commandLine;
        ASSERT_NOT_REACHED();
#if USE(MACH_PORTS)
        mach_port_deallocate(mach_task_self(), connector);
        mach_port_mod_refs(mach_task_self(), connector, MACH_PORT_RIGHT_RECEIVE, -1);
#endif
        delete webProcessOrSUIDHelper;
        return;
    }
#if OS(UNIX)
    setpriority(PRIO_PROCESS, webProcessOrSUIDHelper->processId(), 10);
#endif
    m_processObject = webProcessOrSUIDHelper;
    RefPtr<ProcessLauncher> protector(this);
    RunLoop::main().dispatch([protector, webProcessOrSUIDHelper, connector] {
        protector->didFinishLaunchingProcess(webProcessOrSUIDHelper->processId(), connector);
    });
}

void ProcessLauncher::terminateProcess()
{
    if (!m_processObject)
        return;

    QObject::connect(m_processObject, SIGNAL(finished()), m_processObject, SLOT(deleteLater()), Qt::QueuedConnection);
    m_processObject->terminate();
}

void ProcessLauncher::platformInvalidate()
{

}

} // namespace WebKit

#include "ProcessLauncherQt.moc"
