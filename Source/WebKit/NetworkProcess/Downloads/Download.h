/*
 * Copyright (C) 2010-2016 Apple Inc. All rights reserved.
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

#pragma once

#include "DownloadID.h"
#include "DownloadManager.h"
#include "DownloadMonitor.h"
#include "MessageSender.h"
#include "NetworkDataTask.h"
#include "SandboxExtension.h"
#include "UseDownloadPlaceholder.h"
#include <WebCore/AuthenticationChallenge.h>
#include <WebCore/ResourceRequest.h>
#include <memory>
#include <pal/SessionID.h>
#include <wtf/Noncopyable.h>
#include <wtf/RefCountedAndCanMakeWeakPtr.h>
#include <wtf/RetainPtr.h>
#include <wtf/TZoneMalloc.h>
#include <wtf/WeakPtr.h>

#if PLATFORM(COCOA)
OBJC_CLASS NSProgress;
OBJC_CLASS NSURLSessionDownloadTask;
#endif

namespace WebCore {
class AuthenticationChallenge;
class BlobDataFileReference;
class Credential;
class ResourceError;
class ResourceRequest;
class ResourceResponse;
}

namespace WebKit {

class DownloadMonitor;
class NetworkDataTask;
class NetworkSession;
class WebPage;

#if PLATFORM(QT)
    class QtFileDownloader;
#endif
    
class Download final : public IPC::MessageSender, public RefCountedAndCanMakeWeakPtr<Download> {
    WTF_MAKE_TZONE_ALLOCATED(Download);
    WTF_MAKE_NONCOPYABLE(Download);
public:
    static Ref<Download> create(DownloadManager&, DownloadID, NetworkDataTask&, NetworkSession&, const String& suggestedFilename = { });
#if PLATFORM(COCOA)
    static Ref<Download> create(DownloadManager&, DownloadID, NSURLSessionDownloadTask*, NetworkSession&, const String& suggestedFilename = { });
#endif

    ~Download();

    void resume(std::span<const uint8_t> resumeData, const String& path, SandboxExtension::Handle&&, std::span<const uint8_t> activityAccessToken);
    enum class IgnoreDidFailCallback : bool { No, Yes };
    void cancel(CompletionHandler<void(std::span<const uint8_t>)>&&, IgnoreDidFailCallback);
#if PLATFORM(COCOA)
    void publishProgress(const URL&, SandboxExtension::Handle&&);
#endif

#if HAVE(MODERN_DOWNLOADPROGRESS)
    void publishProgress(const URL&, std::span<const uint8_t>, WebKit::UseDownloadPlaceholder, std::span<const uint8_t>);
    void setPlaceholderURL(NSURL *, NSData *);
    void setFinalURL(NSURL *, NSData *);
#endif

    DownloadID downloadID() const { return m_downloadID; }
    PAL::SessionID sessionID() const { return m_sessionID; }

    void setSandboxExtension(RefPtr<SandboxExtension>&& sandboxExtension) { m_sandboxExtension = WTFMove(sandboxExtension); }
    void didReceiveChallenge(const WebCore::AuthenticationChallenge&, ChallengeCompletionHandler&&);
    void didCreateDestination(const String& path);
    void didReceiveData(uint64_t bytesWritten, uint64_t totalBytesWritten, uint64_t totalBytesExpectedToWrite);
    void didFinish();
    void didFail(const WebCore::ResourceError&, std::span<const uint8_t> resumeData);
    
#if PLATFORM(QT)
    void startTransfer(const String& destination);
#endif

    void applicationDidEnterBackground() { protectedMonitor()->applicationDidEnterBackground(); }
    void applicationWillEnterForeground() { protectedMonitor()->applicationWillEnterForeground(); }
    DownloadManager* manager() const { return m_downloadManager.get(); }
    void clearManager() { m_downloadManager = nullptr; }

    unsigned testSpeedMultiplier() const { return m_testSpeedMultiplier; }

private:
    Download(DownloadManager&, DownloadID, NetworkDataTask&, NetworkSession&, const String& suggestedFilename = { });
#if PLATFORM(COCOA)
    Download(DownloadManager&, DownloadID, NSURLSessionDownloadTask*, NetworkSession&, const String& suggestedFilename = { });
#endif

    Ref<DownloadMonitor> protectedMonitor() { return m_monitor; }

    // IPC::MessageSender
    IPC::Connection* messageSenderConnection() const override;
    uint64_t messageSenderDestinationID() const override;

    void platformCancelNetworkLoad(CompletionHandler<void(std::span<const uint8_t>)>&&);
    void platformDestroyDownload();
    void platformDidFinish(CompletionHandler<void()>&&);

#if HAVE(MODERN_DOWNLOADPROGRESS)
    void startUpdatingProgress();
    void updateProgress(uint64_t totalBytesWritten, uint64_t totalBytesExpectedToWrite);
    static Vector<uint8_t> updateResumeDataWithPlaceholderURL(NSURL *, std::span<const uint8_t> resumeData);
#endif

    CheckedPtr<DownloadManager> m_downloadManager;
    DownloadID m_downloadID;
    Ref<DownloadManager::Client> m_client;

    Vector<RefPtr<WebCore::BlobDataFileReference>> m_blobFileReferences;
    RefPtr<SandboxExtension> m_sandboxExtension;

    RefPtr<NetworkDataTask> m_download;
#if PLATFORM(COCOA)
    RetainPtr<NSURLSessionDownloadTask> m_downloadTask;
    RetainPtr<NSProgress> m_progress;
    RetainPtr<NSURL> m_placeholderURL;
#if HAVE(MODERN_DOWNLOADPROGRESS)
    RetainPtr<NSData> m_bookmarkData;
    RetainPtr<NSURL> m_bookmarkURL;
    bool m_canUpdateProgress { false };
    std::optional<uint64_t> m_totalBytesWritten;
    std::optional<uint64_t> m_totalBytesExpectedToWrite;
#endif
#endif
    PAL::SessionID m_sessionID;
    bool m_hasReceivedData { false };
    IgnoreDidFailCallback m_ignoreDidFailCallback { IgnoreDidFailCallback::No };
#if PLATFORM(QT)
    QtFileDownloader* m_qtDownloader { nullptr };
#endif
    DownloadMonitor m_monitor { *this };
    unsigned m_testSpeedMultiplier { 1 };
    CompletionHandler<void(std::span<const uint8_t>)> m_cancelCompletionHandler;
};

} // namespace WebKit
