/*
 * Copyright (C) 2020-2023 Apple Inc. All rights reserved.
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
#include "RemoteRenderingBackendProxy.h"

#if ENABLE(GPU_PROCESS)

#include "BufferIdentifierSet.h"
#include "GPUConnectionToWebProcess.h"
#include "ImageBufferShareableBitmapBackend.h"
#include "Logging.h"
#include "RemoteDisplayListRecorderProxy.h"
#include "RemoteImageBufferMessages.h"
#include "RemoteImageBufferProxy.h"
#include "RemoteImageBufferProxyMessages.h"
#include "RemoteImageBufferSetProxy.h"
#include "RemoteImageBufferSetProxyMessages.h"
#include "RemoteRenderingBackendMessages.h"
#include "RemoteRenderingBackendProxyMessages.h"
#include "RemoteSharedResourceCacheProxy.h"
#include "SwapBuffersDisplayRequirement.h"
#include "WebPage.h"
#include "WebProcess.h"
#include <JavaScriptCore/TypedArrayInlines.h>
#include <WebCore/FontCustomPlatformData.h>
#include <WebCore/ImageBufferPixelFormat.h>
#include <wtf/StdLibExtras.h>
#include <wtf/text/TextStream.h>

#if HAVE(IOSURFACE)
#include "ImageBufferRemoteIOSurfaceBackend.h"
#include "ImageBufferShareableMappedIOSurfaceBackend.h"
#endif

namespace WebKit {

using namespace WebCore;

Ref<RemoteRenderingBackendProxy> RemoteRenderingBackendProxy::create(WebPage& webPage)
{
    Ref instance = adoptRef(*new RemoteRenderingBackendProxy(RunLoop::main()));
    RELEASE_LOG(RemoteLayerBuffers, "[renderingBackend=%" PRIu64 "] Created rendering backend for pageProxyID=%" PRIu64 ", webPageID=%" PRIu64, instance->renderingBackendIdentifier().toUInt64(),  webPage.webPageProxyIdentifier().toUInt64(), webPage.identifier().toUInt64());
    return instance;
}

Ref<RemoteRenderingBackendProxy> RemoteRenderingBackendProxy::create(SerialFunctionDispatcher& dispatcher)
{
    Ref instance = adoptRef(*new RemoteRenderingBackendProxy(dispatcher));
    RELEASE_LOG(RemoteLayerBuffers, "[renderingBackend=%" PRIu64 "] Created rendering backend for a worker", instance->renderingBackendIdentifier().toUInt64());
    return instance;
}

RemoteRenderingBackendProxy::RemoteRenderingBackendProxy(SerialFunctionDispatcher& dispatcher)
    : m_dispatcher(dispatcher)
    , m_queue(WorkQueue::create("RemoteRenderingBackendProxy"_s, WorkQueue::QOS::UserInteractive))
{
}

RemoteRenderingBackendProxy::~RemoteRenderingBackendProxy()
{
    for (auto& markAsVolatileHandlers : m_markAsVolatileRequests.values())
        markAsVolatileHandlers(false);

    if (!m_connection)
        return;

    ensureOnMainRunLoop([identifier = m_identifier, weakGPUProcessConnection = WTFMove(m_gpuProcessConnection)]() {
        RefPtr gpuProcessConnection = weakGPUProcessConnection.get();
        if (!gpuProcessConnection)
            return;
        gpuProcessConnection->releaseRenderingBackend(identifier);
    });
    m_remoteResourceCacheProxy.clear();
    disconnectGPUProcess();
}

void RemoteRenderingBackendProxy::ensureGPUProcessConnection()
{
    if (m_connection)
        return;
    constexpr unsigned connectionBufferSizeLog2 = 21u;
    auto connectionPair = IPC::StreamClientConnection::create(connectionBufferSizeLog2, WebProcess::singleton().gpuProcessTimeoutDuration());
    if (!connectionPair)
        CRASH();
    auto [streamConnection, serverHandle] = WTFMove(*connectionPair);
    m_connection = streamConnection.ptr();
    // RemoteRenderingBackendProxy behaves as the dispatcher for the connection to obtain isolated state for its
    // connection. This prevents waits on RemoteRenderingBackendProxy to process messages from other connections.
    streamConnection->open(*this, *this);
    m_isResponsive = true;
    callOnMainRunLoopAndWait([&, serverHandle = WTFMove(serverHandle)]() mutable {
        Ref gpuProcessConnection = WebProcess::singleton().ensureGPUProcessConnection();
        gpuProcessConnection->createRenderingBackend(m_identifier, WTFMove(serverHandle));
        m_gpuProcessConnection = gpuProcessConnection.get();
        m_sharedResourceCache = gpuProcessConnection->sharedResourceCache();
    });
}

template<typename T, typename U, typename V, typename W>
auto RemoteRenderingBackendProxy::send(T&& message, ObjectIdentifierGeneric<U, V, W> destination)
{
    RefPtr connection = this->connection();
    if (UNLIKELY(!connection))
        return IPC::Error::InvalidConnection;
    auto result = connection->send(std::forward<T>(message), destination);
    if (UNLIKELY(result != IPC::Error::NoError)) {
        RELEASE_LOG(RemoteLayerBuffers, "[renderingBackend=%" PRIu64 "] RemoteRenderingBackendProxy::send - failed, name:%" PUBLIC_LOG_STRING ", error:%" PUBLIC_LOG_STRING, m_identifier.toUInt64(), IPC::description(T::name()).characters(), IPC::errorAsString(result).characters());
        didBecomeUnresponsive();
    }
    return result;
}

template<typename T, typename U, typename V, typename W>
auto RemoteRenderingBackendProxy::sendSync(T&& message, ObjectIdentifierGeneric<U, V, W> destination)
{
    RefPtr connection = this->connection();
    if (!connection)
        return IPC::StreamClientConnection::SendSyncResult<T> { IPC::Error::InvalidConnection };
    auto result = connection->sendSync(std::forward<T>(message), destination);
    if (UNLIKELY(!result.succeeded())) {
        RELEASE_LOG(RemoteLayerBuffers, "[renderingBackend=%" PRIu64 "] RemoteRenderingBackendProxy::sendSync - failed, name:%" PUBLIC_LOG_STRING ", error:%" PUBLIC_LOG_STRING,  m_identifier.toUInt64(), IPC::description(T::name()).characters(), IPC::errorAsString(result.error()).characters());
        didBecomeUnresponsive();
    }
    return result;
}

template<typename T, typename C, typename U, typename V, typename W>
auto RemoteRenderingBackendProxy::sendWithAsyncReply(T&& message, C&& callback, ObjectIdentifierGeneric<U, V, W> destination)
{
    RefPtr connection = this->connection();
    if (UNLIKELY(!connection))
        return IPC::Error::InvalidConnection;
    auto replyID = connection->sendWithAsyncReply(std::forward<T>(message), std::forward<C>(callback), destination);
    if (UNLIKELY(!replyID)) {
        RELEASE_LOG(RemoteLayerBuffers, "[renderingBackend=%" PRIu64 "] RemoteRenderingBackendProxy::sendWithAsyncReply - failed, name:%" PUBLIC_LOG_STRING, m_identifier.toUInt64(), IPC::description(T::name()).characters());
        didBecomeUnresponsive();
        return IPC::Error::Unspecified;
    }
    return IPC::Error::NoError;
}

void RemoteRenderingBackendProxy::dispatch(Function<void()>&& function)
{
    if (RefPtr dispatcher = m_dispatcher.get())
        dispatcher->dispatch(WTFMove(function));
}

bool RemoteRenderingBackendProxy::isCurrent() const
{
    RefPtr dispatcher = m_dispatcher.get();
    return dispatcher && dispatcher->isCurrent();
}

void RemoteRenderingBackendProxy::didClose(IPC::Connection&)
{
    if (!m_connection)
        return;
    disconnectGPUProcess();
    // Note: The cache will call back to this to setup a new connection.
    m_remoteResourceCacheProxy.remoteResourceCacheWasDestroyed();

    for (auto bufferSet : m_bufferSets) {
        bufferSet.value->remoteBufferSetWasDestroyed();
        send(Messages::RemoteRenderingBackend::CreateRemoteImageBufferSet(bufferSet.value->identifier(), bufferSet.value->displayListResourceIdentifier()));
    }
}

void RemoteRenderingBackendProxy::didBecomeUnresponsive()
{
    if (!m_isResponsive)
        return;
    RELEASE_LOG(RemoteLayerBuffers, "[renderingBackend=%" PRIu64 "] RemoteRenderingBackendProxy::didBecomeUnresponsive", m_identifier.toUInt64());
    m_isResponsive = false;
    ensureOnMainRunLoop([weakGPUProcessConnection = WTFMove(m_gpuProcessConnection)]() {
        RefPtr gpuProcessConnection = weakGPUProcessConnection.get();
        if (!gpuProcessConnection)
            return;
        gpuProcessConnection->didBecomeUnresponsive();
    });
}

void RemoteRenderingBackendProxy::disconnectGPUProcess()
{
    if (m_destroyGetPixelBufferSharedMemoryTimer.isActive())
        m_destroyGetPixelBufferSharedMemoryTimer.stop();
    m_getPixelBufferSharedMemory = nullptr;
    m_renderingUpdateID = { };
    m_didRenderingUpdateID = { };
    protectedConnection()->invalidate();
    m_connection = nullptr;
    m_isResponsive = false;
    m_gpuProcessConnection = nullptr;
}

void RemoteRenderingBackendProxy::createRemoteImageBuffer(ImageBuffer& imageBuffer)
{
    send(Messages::RemoteRenderingBackend::CreateImageBuffer(imageBuffer.logicalSize(), imageBuffer.renderingMode(), imageBuffer.renderingPurpose(), imageBuffer.resolutionScale(), imageBuffer.colorSpace(), imageBuffer.pixelFormat(), imageBuffer.renderingResourceIdentifier()));
}

bool RemoteRenderingBackendProxy::canMapRemoteImageBufferBackendBackingStore()
{
    // When DOM rendering happens in WebContent process, we need to paint 2D contexts
    // into the tiles. In this case ImageBuffers should be created as mappable.
    return !WebProcess::singleton().shouldUseRemoteRenderingFor(RenderingPurpose::DOM);
}

RefPtr<ImageBuffer> RemoteRenderingBackendProxy::createImageBuffer(const FloatSize& size, RenderingMode renderingMode, RenderingPurpose purpose, float resolutionScale, const DestinationColorSpace& colorSpace, ImageBufferPixelFormat pixelFormat)
{
    RefPtr<ImageBuffer> imageBuffer;

    switch (renderingMode) {
    case RenderingMode::Accelerated:
#if HAVE(IOSURFACE)
        if (canMapRemoteImageBufferBackendBackingStore())
            imageBuffer = RemoteImageBufferProxy::create<ImageBufferShareableMappedIOSurfaceBackend>(size, resolutionScale, colorSpace, pixelFormat, purpose, *this);
        else
            imageBuffer = RemoteImageBufferProxy::create<ImageBufferRemoteIOSurfaceBackend>(size, resolutionScale, colorSpace, pixelFormat, purpose, *this);
#endif
        [[fallthrough]];

    case RenderingMode::Unaccelerated:
        if (!imageBuffer)
            imageBuffer = RemoteImageBufferProxy::create<ImageBufferShareableBitmapBackend>(size, resolutionScale, colorSpace, pixelFormat, purpose, *this);
        break;

    case RenderingMode::PDFDocument:
    case RenderingMode::DisplayList:
        break;
    }

    if (!imageBuffer)
        return nullptr;

    createRemoteImageBuffer(*imageBuffer);
    return imageBuffer;
}

std::unique_ptr<RemoteDisplayListRecorderProxy> RemoteRenderingBackendProxy::createDisplayListRecorder(WebCore::RenderingResourceIdentifier renderingResourceIdentifier, const FloatSize& size, RenderingMode renderingMode, RenderingPurpose purpose, float resolutionScale, const DestinationColorSpace& colorSpace, ImageBufferPixelFormat pixelFormat)
{
    ASSERT(WebProcess::singleton().shouldUseRemoteRenderingFor(RenderingPurpose::DOM));
    ImageBufferParameters parameters { size, resolutionScale, colorSpace, pixelFormat, purpose };
    auto transform = ImageBufferBackend::calculateBaseTransform(ImageBuffer::backendParameters(parameters));
    return makeUnique<RemoteDisplayListRecorderProxy>(*this, renderingResourceIdentifier, colorSpace, renderingMode, FloatRect { { }, size }, transform);
}

void RemoteRenderingBackendProxy::releaseImageBuffer(RenderingResourceIdentifier renderingResourceIdentifier)
{
    if (!m_connection)
        return;
    send(Messages::RemoteRenderingBackend::ReleaseImageBuffer(renderingResourceIdentifier));
}

RefPtr<RemoteImageBufferSetProxy>  RemoteRenderingBackendProxy::createRemoteImageBufferSet()
{
    RefPtr<RemoteImageBufferSetProxy> result = adoptRef(new RemoteImageBufferSetProxy(*this));
    send(Messages::RemoteRenderingBackend::CreateRemoteImageBufferSet(result->identifier(), result->displayListResourceIdentifier()));

    auto addResult = m_bufferSets.add(result->identifier(), result);
    ASSERT_UNUSED(addResult, addResult.isNewEntry);
    return result;
}

void RemoteRenderingBackendProxy::releaseRemoteImageBufferSet(RemoteImageBufferSetProxy& bufferSet)
{
    bool success = m_bufferSets.remove(bufferSet.identifier());
    ASSERT_UNUSED(success, success);
    if (!m_connection)
        return;
    send(Messages::RemoteRenderingBackend::ReleaseRemoteImageBufferSet(bufferSet.identifier()));
}

void RemoteRenderingBackendProxy::moveToSerializedBuffer(WebCore::RenderingResourceIdentifier identifier)
{
    send(Messages::RemoteRenderingBackend::MoveToSerializedBuffer(identifier));
}

void RemoteRenderingBackendProxy::moveToImageBuffer(WebCore::RenderingResourceIdentifier identifier)
{
    send(Messages::RemoteRenderingBackend::MoveToImageBuffer(identifier));
}

bool RemoteRenderingBackendProxy::getPixelBufferForImageBuffer(RenderingResourceIdentifier imageBuffer, const PixelBufferFormat& destinationFormat, const IntRect& srcRect, std::span<uint8_t> result)
{
    if (auto handle = updateSharedMemoryForGetPixelBuffer(result.size())) {
        auto sendResult = sendSync(Messages::RemoteImageBuffer::GetPixelBufferWithNewMemory(WTFMove(*handle), destinationFormat, srcRect.location(), srcRect.size()), imageBuffer);
        if (!sendResult.succeeded())
            return false;
    } else {
        if (!m_getPixelBufferSharedMemory)
            return false;
        auto sendResult = sendSync(Messages::RemoteImageBuffer::GetPixelBuffer(destinationFormat, srcRect.location(), srcRect.size()), imageBuffer);
        if (!sendResult.succeeded())
            return false;
    }
    memcpySpan(result, m_getPixelBufferSharedMemory->span().first(result.size()));
    return true;
}

void RemoteRenderingBackendProxy::putPixelBufferForImageBuffer(RenderingResourceIdentifier imageBuffer, const PixelBuffer& pixelBuffer, const IntRect& srcRect, const IntPoint& destPoint, AlphaPremultiplication destFormat)
{
    send(Messages::RemoteImageBuffer::PutPixelBuffer(Ref { const_cast<PixelBuffer&>(pixelBuffer) }, srcRect.location(), srcRect.size(), destPoint, destFormat), imageBuffer);
}

std::optional<SharedMemory::Handle> RemoteRenderingBackendProxy::updateSharedMemoryForGetPixelBuffer(size_t dataSize)
{
    if (m_destroyGetPixelBufferSharedMemoryTimer.isActive())
        m_destroyGetPixelBufferSharedMemoryTimer.stop();

    if (m_getPixelBufferSharedMemory && dataSize <= m_getPixelBufferSharedMemory->size()) {
        m_destroyGetPixelBufferSharedMemoryTimer.startOneShot(5_s);
        return std::nullopt;
    }
    destroyGetPixelBufferSharedMemory();
    auto memory = SharedMemory::allocate(dataSize);
    if (!memory)
        return std::nullopt;
    auto handle = memory->createHandle(SharedMemory::Protection::ReadWrite);
    if (!handle)
        return std::nullopt;

    m_getPixelBufferSharedMemory = WTFMove(memory);
    handle->takeOwnershipOfMemory(MemoryLedger::Graphics);
    m_destroyGetPixelBufferSharedMemoryTimer.startOneShot(5_s);
    return handle;
}

void RemoteRenderingBackendProxy::destroyGetPixelBufferSharedMemory()
{
    if (!m_getPixelBufferSharedMemory)
        return;
    m_getPixelBufferSharedMemory = nullptr;
    send(Messages::RemoteRenderingBackend::DestroyGetPixelBufferSharedMemory());
}

RefPtr<ShareableBitmap> RemoteRenderingBackendProxy::getShareableBitmap(RenderingResourceIdentifier imageBuffer, PreserveResolution preserveResolution)
{
    auto sendResult = sendSync(Messages::RemoteImageBuffer::GetShareableBitmap(preserveResolution), imageBuffer);
    if (!sendResult.succeeded())
        return { };
    auto [handle] = sendResult.takeReply();
    if (!handle)
        return { };
    handle->takeOwnershipOfMemory(MemoryLedger::Graphics);
    return ShareableBitmap::create(WTFMove(*handle));
}

void RemoteRenderingBackendProxy::cacheNativeImage(ShareableBitmap::Handle&& handle, RenderingResourceIdentifier renderingResourceIdentifier)
{
    send(Messages::RemoteRenderingBackend::CacheNativeImage(WTFMove(handle), renderingResourceIdentifier));
}

void RemoteRenderingBackendProxy::cacheFont(const WebCore::Font::Attributes& fontAttributes, const WebCore::FontPlatformDataAttributes& platformData, std::optional<WebCore::RenderingResourceIdentifier> ident)
{
    send(Messages::RemoteRenderingBackend::CacheFont(fontAttributes, platformData, ident));
}

void RemoteRenderingBackendProxy::cacheFontCustomPlatformData(Ref<const FontCustomPlatformData>&& customPlatformData)
{
    Ref<FontCustomPlatformData> data = adoptRef(const_cast<FontCustomPlatformData&>(customPlatformData.leakRef()));
    send(Messages::RemoteRenderingBackend::CacheFontCustomPlatformData(data->serializedData()));
}

void RemoteRenderingBackendProxy::cacheDecomposedGlyphs(Ref<DecomposedGlyphs>&& decomposedGlyphs)
{
    send(Messages::RemoteRenderingBackend::CacheDecomposedGlyphs(WTFMove(decomposedGlyphs)));
}

void RemoteRenderingBackendProxy::cacheGradient(Ref<Gradient>&& gradient)
{
    send(Messages::RemoteRenderingBackend::CacheGradient(WTFMove(gradient)));
}

void RemoteRenderingBackendProxy::cacheFilter(Ref<Filter>&& filter)
{
    send(Messages::RemoteRenderingBackend::CacheFilter(WTFMove(filter)));
}

void RemoteRenderingBackendProxy::releaseAllDrawingResources()
{
    if (!m_connection)
        return;
    send(Messages::RemoteRenderingBackend::ReleaseAllDrawingResources());
}

void RemoteRenderingBackendProxy::releaseRenderingResource(RenderingResourceIdentifier renderingResourceIdentifier)
{
    if (!m_connection)
        return;
    send(Messages::RemoteRenderingBackend::ReleaseRenderingResource(renderingResourceIdentifier));
}

void RemoteRenderingBackendProxy::releaseAllImageResources()
{
    if (!m_connection)
        return;
    send(Messages::RemoteRenderingBackend::ReleaseAllImageResources());
}

#if PLATFORM(COCOA)
Vector<SwapBuffersDisplayRequirement> RemoteRenderingBackendProxy::prepareImageBufferSetsForDisplay(Vector<LayerPrepareBuffersData>&& prepareBuffersInput)
{
    if (prepareBuffersInput.isEmpty())
        return Vector<SwapBuffersDisplayRequirement>();

    bool needsSync = false;

    auto inputData = WTF::map(prepareBuffersInput, [&](auto& perLayerData) {
        // If the front buffer might be volatile, then we have to wait for the callback
        // to find out we were able to copy pixels from it or if it had been discarded.
        if (perLayerData.bufferSet->requestedVolatility().contains(BufferInSetType::Front))
            needsSync = true;

        // Using the  will mark buffers as non-volatile and
        // we don't know exactly which. Assume they all are non-volatile.
        perLayerData.bufferSet->clearVolatility();
        perLayerData.bufferSet->willPrepareForDisplay();

        return ImageBufferSetPrepareBufferForDisplayInputData {
            perLayerData.bufferSet->identifier(),
            perLayerData.dirtyRegion,
            perLayerData.supportsPartialRepaint,
            perLayerData.hasEmptyDirtyRegion,
            perLayerData.requiresClearedPixels,
        };
    });

    LOG_WITH_STREAM(RemoteLayerBuffers, stream << "RemoteRenderingBackendProxy::prepareImageBufferSetsForDisplay - input buffers  " << inputData);

    Vector<SwapBuffersDisplayRequirement> result;
    if (needsSync) {
        auto sendResult = sendSync(Messages::RemoteRenderingBackend::PrepareImageBufferSetsForDisplaySync(inputData));
        if (!sendResult.succeeded()) {
            result.grow(inputData.size());
            for (auto& displayRequirement : result)
                displayRequirement = SwapBuffersDisplayRequirement::NeedsFullDisplay;
        } else
            std::tie(result) = sendResult.takeReply();
    } else {
        send(Messages::RemoteRenderingBackend::PrepareImageBufferSetsForDisplay(inputData));
        result.grow(inputData.size());
        for (auto& displayRequirement : result)
            displayRequirement = SwapBuffersDisplayRequirement::NeedsNormalDisplay;
    }
    return result;
}
#endif

void RemoteRenderingBackendProxy::markSurfacesVolatile(Vector<std::pair<Ref<RemoteImageBufferSetProxy>, OptionSet<BufferInSetType>>>&& bufferSets, CompletionHandler<void(bool)>&& completionHandler, bool forcePurge)
{
    Vector<std::pair<RemoteImageBufferSetIdentifier, OptionSet<BufferInSetType>>> identifiers;
    for (auto& pair : bufferSets) {
        identifiers.append(std::make_pair(pair.first->identifier(), pair.second));
        pair.first->addRequestedVolatility(pair.second);
    }
    auto requestIdentifier = MarkSurfacesAsVolatileRequestIdentifier::generate();
    auto result = send(Messages::RemoteRenderingBackend::MarkSurfacesVolatile(requestIdentifier, identifiers, forcePurge));
    if (result == IPC::Error::NoError)
        m_markAsVolatileRequests.add(requestIdentifier, WTFMove(completionHandler));
    else
        completionHandler(false);
}

void RemoteRenderingBackendProxy::didMarkLayersAsVolatile(MarkSurfacesAsVolatileRequestIdentifier requestIdentifier, Vector<std::pair<RemoteImageBufferSetIdentifier, OptionSet<BufferInSetType>>> markedBufferSets, bool didMarkAllLayersAsVolatile)
{
    auto completionHandler = m_markAsVolatileRequests.take(requestIdentifier);
    if (!completionHandler)
        return;

    for (auto& bufferSetIdentifierAndType : markedBufferSets) {
        auto bufferSet = m_bufferSets.get(bufferSetIdentifierAndType.first);
        if (!bufferSet)
            continue;

        bufferSet->setConfirmedVolatility(bufferSetIdentifierAndType.second);
    }

    completionHandler(didMarkAllLayersAsVolatile);
}

void RemoteRenderingBackendProxy::finalizeRenderingUpdate()
{
    if (!m_connection)
        return;
    send(Messages::RemoteRenderingBackend::FinalizeRenderingUpdate(m_renderingUpdateID));
    m_renderingUpdateID.increment();
}

void RemoteRenderingBackendProxy::didPaintLayers()
{
    m_remoteResourceCacheProxy.didPaintLayers();
}

bool RemoteRenderingBackendProxy::dispatchMessage(IPC::Connection& connection, IPC::Decoder& decoder)
{
    if (decoder.messageReceiverName() == Messages::RemoteImageBufferProxy::messageReceiverName()) {
        auto imageBuffer = m_remoteResourceCacheProxy.cachedImageBuffer(RenderingResourceIdentifier { decoder.destinationID() });
        if (imageBuffer)
            imageBuffer->didReceiveMessage(connection, decoder);
        // Messages to already removed instances are ok.
        return true;
    }
    return false;
}

bool RemoteRenderingBackendProxy::dispatchSyncMessage(IPC::Connection&, IPC::Decoder&, UniqueRef<IPC::Encoder>&)
{
    return false;
}

void RemoteRenderingBackendProxy::didFinalizeRenderingUpdate(RenderingUpdateID didRenderingUpdateID)
{
    ASSERT(didRenderingUpdateID <= m_renderingUpdateID);
    m_didRenderingUpdateID = std::min(didRenderingUpdateID, m_renderingUpdateID);
}

RenderingBackendIdentifier RemoteRenderingBackendProxy::renderingBackendIdentifier() const
{
    return m_identifier;
}

RenderingBackendIdentifier RemoteRenderingBackendProxy::ensureBackendCreated()
{
    ensureGPUProcessConnection();
    return m_identifier;
}

RefPtr<IPC::StreamClientConnection> RemoteRenderingBackendProxy::connection()
{
    ensureGPUProcessConnection();
    if (!m_isResponsive)
        return nullptr;

    RefPtr connection = m_connection;
    if (UNLIKELY(!connection->hasSemaphores())) {
        auto error = connection->waitForAndDispatchImmediately<Messages::RemoteRenderingBackendProxy::DidInitialize>(renderingBackendIdentifier());
        if (error != IPC::Error::NoError) {
            RELEASE_LOG(RemoteLayerBuffers, "[renderingBackend=%" PRIu64 "] RemoteRenderingBackendProxy::connection() - waitForAndDispatchImmediately returned error: %" PUBLIC_LOG_STRING, renderingBackendIdentifier().toUInt64(), IPC::errorAsString(error).characters());
            didBecomeUnresponsive();
        }
    }
    if (!m_isResponsive)
        return nullptr;
    return connection;
}

void RemoteRenderingBackendProxy::didInitialize(IPC::Semaphore&& wakeUp, IPC::Semaphore&& clientWait)
{
    RefPtr connection = m_connection;
    if (!connection) {
        ASSERT_NOT_REACHED();
        return;
    }
    connection->setSemaphores(WTFMove(wakeUp), WTFMove(clientWait));
}

bool RemoteRenderingBackendProxy::isCached(const ImageBuffer& imageBuffer) const
{
    if (auto cachedImageBuffer = m_remoteResourceCacheProxy.cachedImageBuffer(imageBuffer.renderingResourceIdentifier())) {
        ASSERT_UNUSED(cachedImageBuffer, cachedImageBuffer == &imageBuffer);
        return true;
    }
    return false;
}

#if USE(GRAPHICS_LAYER_WC)

// Because RemoteRenderingBackend and RemoteImageBuffer share a single stream connection at the moment,
// it is sufficient to flush the single command queue.
// This should take Vector<Ref<ImageBuffer>> as an argument if a RemoteImageBuffer uses own stream connection.
Function<bool()> RemoteRenderingBackendProxy::flushImageBuffers()
{
    IPC::Semaphore flushSemaphore;
    auto result = send(Messages::RemoteRenderingBackend::Flush(flushSemaphore));
    if (result != IPC::Error::NoError) {
        return []() {
            return false;
        };
    }
    return [flushSemaphore = WTFMove(flushSemaphore), timeoutDuration = m_connection->defaultTimeoutDuration()]() mutable {
        return flushSemaphore.waitFor(timeoutDuration);
    };
}

#endif // USE(GRAPHICS_LAYER_WC)

void RemoteRenderingBackendProxy::getImageBufferResourceLimitsForTesting(CompletionHandler<void(ImageBufferResourceLimits)>&& callback)
{
    sendWithAsyncReply(Messages::RemoteRenderingBackend::GetImageBufferResourceLimitsForTesting(), WTFMove(callback));
}

} // namespace WebKit

#endif // ENABLE(GPU_PROCESS)
