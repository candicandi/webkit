/*
 * Copyright (C) 2015 The Qt Company Ltd.
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

#include "QWebPageAdapter.h"

#include "BackForwardList.h"
#include "ChromeClientQt.h"
#include "ContextMenuClientQt.h"
#include "CryptoClientQt.h"
#include "DragClientQt.h"
#include "EditorClientQt.h"
#include "FrameLoaderClientQt.h"
#include "GeolocationPermissionClientQt.h"
#include "InitWebCoreQt.h"
#include "InspectorClientQt.h"
#include "InspectorServerQt.h"
#include "NotificationPresenterClientQt.h"
#include "PluginInfoProviderQt.h"
#include "ProgressTrackerClientQt.h"
#include "LegacyHistoryItemClient.h"
#include "LegacySocketProvider.h"
#include "QWebFrameAdapter.h"
#include "QWebPageStorageSessionProvider.h"
#include "UndoStepQt.h"
#include "VisitedLinkStoreQt.h"
#include "WebBroadcastChannelRegistry.h"
#include "WebDatabaseProvider.h"
#include "WebEventConversion.h"
#include "WebKitVersion.h"
#include "WebStorageNamespaceProvider.h"
#include "qwebhistory_p.h"
#include "qwebpluginfactory.h"
#include "qwebsettings.h"
#include <QBitArray>
#include <QGuiApplication>
#include <QMimeData>
#include <QMouseEvent>
#include <QNetworkAccessManager>
#include <QNetworkCookie>
#include <QNetworkCookieJar>
#include <QStyleHints>
#include <QTextCharFormat>
#include <QTouchEvent>
#include <QWheelEvent>
#include <WebCore/ApplicationCacheStorage.h>
#include <WebCore/BackForwardController.h>
#include <WebCore/BroadcastChannelRegistry.h>
#include <WebCore/CacheStorageProvider.h>
#include <WebCore/Chrome.h>
#include <WebCore/CompositionHighlight.h>
#include <WebCore/ContextMenu.h>
#include <WebCore/ContextMenuClient.h>
#include <WebCore/ContextMenuController.h>
#include <WebCore/CookieJar.h>
#include <WebCore/DocumentLoader.h>
#include <WebCore/DragController.h>
#include <WebCore/DummyModelPlayerProvider.h>
#include <WebCore/DummySpeechRecognitionProvider.h>
#include <WebCore/DummyStorageProvider.h>
#include <WebCore/Editor.h>
#include <WebCore/EmptyBadgeClient.h>
#include <WebCore/EventHandler.h>
#include <WebCore/FocusController.h>
#include <WebCore/FrameLoadRequest.h>
#include <WebCore/FrameSelection.h>
#include <WebCore/HandleUserInputEventResult.h>
#include <WebCore/HTMLFormElement.h>
#include <WebCore/HTMLInputElement.h>
#include <WebCore/HitTestResult.h>
#include <WebCore/InspectorController.h>
#include <WebCore/LegacySchemeRegistry.h>
#include <WebCore/LocalDOMWindow.h>
#include <WebCore/LocalizedStrings.h>
#include <WebCore/MIMETypeRegistry.h>
#include <WebCore/MemoryCache.h>
#include <WebCore/NavigationAction.h>
#include <WebCore/NetworkStorageSession.h>
#include <WebCore/NetworkingContext.h>
#include <WebCore/PageConfiguration.h>
#include <WebCore/PageIdentifier.h>
#include <WebCore/PlatformKeyboardEvent.h>
#include <WebCore/PlatformMouseEvent.h>
#include <WebCore/PlatformWheelEvent.h>
#include <WebCore/ProcessSyncClient.h>
#include <WebCore/ProgressTracker.h>
#include <WebCore/QWebPageClient.h>
#include <WebCore/RemoteFrameClient.h>
#include <WebCore/RenderTextControl.h>
#include <WebCore/ScrollbarTheme.h>
#include <WebCore/ScrollingCoordinatorTypes.h>
#include <WebCore/Settings.h>
#include <WebCore/TextIterator.h>
#include <WebCore/UserAgentQt.h>
#include <WebCore/UserContentController.h>
#include <WebCore/UserGestureIndicator.h>
#include <WebCore/VisibilityState.h>
#include <WebCore/WebLockRegistry.h>
#include <WebCore/WebRTCProvider.h>
#include <WebCore/WindowFeatures.h>
#include <WebCore/markup.h>
#include <wtf/UniqueRef.h>

#if ENABLE(DEVICE_ORIENTATION)
#include "DeviceMotionClientMock.h"
#include "DeviceMotionController.h"
#include "DeviceOrientationClientMock.h"
#include "DeviceOrientationController.h"
#if HAVE(QTSENSORS)
#include "DeviceMotionClientQt.h"
#include "DeviceOrientationClientQt.h"
#endif
#endif

#if ENABLE(GEOLOCATION)
#include <WebCore/GeolocationClientMock.h>
#include <WebCore/GeolocationController.h>
#if HAVE(QTPOSITIONING)
#include "GeolocationClientQt.h"
#endif
#endif

#if ENABLE(VIDEO)
#include <WebCore/HTMLMediaElement.h>
#endif

// from text/qfont.cpp
QT_BEGIN_NAMESPACE
extern Q_GUI_EXPORT int qt_defaultDpi();
QT_END_NAMESPACE

using namespace WebCore;

bool QWebPageAdapter::drtRun = false;

typedef QWebPageAdapter::MenuItemDescription MenuItem;

static inline OptionSet<WebCore::DragOperation> dropActionToDragOp(Qt::DropActions actions)
{
    OptionSet<WebCore::DragOperation> result;
    if (actions & Qt::CopyAction)
        result.add(DragOperation::Copy);
    // DragOperationgeneric represents InternetExplorer's equivalent of Move operation,
    // hence it should be considered as "move"
    if (actions & Qt::MoveAction)
    {
        result.add(DragOperation::Move);
        result.add(DragOperation::Generic);
    }
    if (actions & Qt::LinkAction)
        result.add(DragOperation::Link);
    return result;
}

static inline Qt::DropAction dragOpToDropAction(std::variant<std::optional<DragOperation>, RemoteUserInputEventData> actionOrEvent)
{
    Qt::DropAction result = Qt::IgnoreAction;

    if (std::holds_alternative<std::optional<DragOperation>>(actionOrEvent)) {
        auto action = std::get<std::optional<DragOperation>>(actionOrEvent);
        if (!action)
            return result;
        if (*action == DragOperation::Copy)
            result = Qt::CopyAction;
        else if (*action == DragOperation::Move)
            result = Qt::MoveAction;
        // DragOperationgeneric represents InternetExplorer's equivalent of Move operation,
        // hence it should be considered as "move"
        else if (*action == DragOperation::Generic)
            result = Qt::MoveAction;
        else if (*action == DragOperation::Link)
            result = Qt::LinkAction;
    }
}

static inline QWebPageAdapter::VisibilityState webCoreVisibilityStateToWebPageVisibilityState(WebCore::VisibilityState state)
{
    switch (state) {
    case WebCore::VisibilityState::Visible:
        return QWebPageAdapter::VisibilityStateVisible;
    case WebCore::VisibilityState::Hidden:
        return QWebPageAdapter::VisibilityStateHidden;
    default:
        ASSERT(false);
        return QWebPageAdapter::VisibilityStateHidden;
    }
}

static WebCore::FrameLoadRequest frameLoadRequest(const QUrl &url, WebCore::LocalFrame& frame)
{
    WebCore::FrameLoadRequest frameLoadRequest(*frame.document(),
        frame.document()->securityOrigin(),
        WebCore::ResourceRequest(url, frame.loader().outgoingReferrer()),
        { },
        InitiatedByMainFrame::Yes
        );

    frameLoadRequest.setLockHistory(LockHistory::No);
    frameLoadRequest.setLockBackForwardList(LockBackForwardList::No);
    frameLoadRequest.setNewFrameOpenerPolicy(NewFrameOpenerPolicy::Allow);
    frameLoadRequest.setReferrerPolicy(ReferrerPolicy::EmptyString);
    frameLoadRequest.setShouldOpenExternalURLsPolicy(ShouldOpenExternalURLsPolicy::ShouldAllow);

    return frameLoadRequest;
}

static void openNewWindow(const QUrl& url, LocalFrame& frame)
{
    if (Page* oldPage = frame.page()) {
        WindowFeatures features;
        NavigationAction action;
        FrameLoadRequest request = frameLoadRequest(url, frame);
        if (auto newPage = oldPage->chrome().createWindow(frame, { }, features, action)) {
            WebCore::LocalFrame* newFrame = dynamicDowncast<WebCore::LocalFrame>(newPage->mainFrame());
            newFrame->loader().loadFrameRequest(WTFMove(request), /*event*/ nullptr, /*FormState*/ nullptr);
            newPage->chrome().show();
        }
    }
}

// FIXME: Find a better place
static UserContentController& userContentProvider()
{
    static NeverDestroyed<Ref<UserContentController>> s_userContentProvider(UserContentController::create());
    return s_userContentProvider.get();
}

static Ref<WebCore::LocalWebLockRegistry> getOrCreateWebLockRegistry(bool isPrivateBrowsingEnabled)
{
    static NeverDestroyed<WeakPtr<WebCore::LocalWebLockRegistry>> defaultRegistry;
    static NeverDestroyed<WeakPtr<WebCore::LocalWebLockRegistry>> privateRegistry;
    auto& existingRegistry = isPrivateBrowsingEnabled ? privateRegistry : defaultRegistry;
    if (existingRegistry.get())
        return *existingRegistry.get();
    auto registry = WebCore::LocalWebLockRegistry::create();
    existingRegistry.get() = registry;
    return registry;
}

QWebPageAdapter::QWebPageAdapter()
    : settings(0)
    , page(nullptr)
    , pluginFactory(0)
    , forwardUnsupportedContent(false)
    , insideOpenCall(false)
    , clickCausedFocus(false)
    , mousePressed(false)
    , m_useNativeVirtualKeyAsDOMKey(false)
    , m_totalBytes(0)
    , m_bytesReceived()
    , networkManager(0)
    , m_deviceOrientationClient(0)
    , m_deviceMotionClient(0)
{
    WebCore::initializeWebCoreQt();
}

void QWebPageAdapter::initializeWebCorePage()
{
#if ENABLE(GEOLOCATION) || ENABLE(DEVICE_ORIENTATION)
    const bool useMock = QWebPageAdapter::drtRun;
#endif
    auto storageProvider = WebKit::QWebPageStorageSessionProvider::create(*this);

    // QTFIXME: Fix per-page configuration of immutable properties
    bool isPrivateBrowsingEnabled = QWebSettings::globalSettings()->testAttribute(QWebSettings::PrivateBrowsingEnabled);
    auto sessionID = isPrivateBrowsingEnabled ? PAL::SessionID::legacyPrivateSessionID() : PAL::SessionID::defaultSessionID();

    PageConfiguration pageConfiguration {
        WebCore::PageIdentifier::generate(),
        sessionID,
        makeUniqueRef<EditorClientQt>(this),
        LegacySocketProvider::create(),
        WebRTCProvider::create(),
        CacheStorageProvider::create(),
        userContentProvider(),
        BackForwardList::create(*this),
        CookieJar::create(storageProvider.copyRef()),
        makeUniqueRef<ProgressTrackerClientQt>(this),
        WebCore::PageConfiguration::LocalMainFrameCreationParameters {
          CompletionHandler<UniqueRef<WebCore::LocalFrameLoaderClient>(WebCore::LocalFrame&, WebCore::FrameLoader&)> { [] (auto&, auto& frameLoader) {
              return makeUniqueRefWithoutRefCountedCheck<FrameLoaderClientQt>(frameLoader);
          } },
          WebCore::SandboxFlags { } // Set by updateSandboxFlags after instantiation.
        },
        WebCore::FrameIdentifier::generate(),
        nullptr, // Opener may be set by setOpenerForWebKitLegacy after instantiation.
        makeUniqueRef<WebCore::DummySpeechRecognitionProvider>(),
        WebBroadcastChannelRegistry::getOrCreate(isPrivateBrowsingEnabled),
        makeUniqueRef<WebCore::DummyStorageProvider>(),
        makeUniqueRef<WebCore::DummyModelPlayerProvider>(),
        WebCore::EmptyBadgeClient::create(),
        LegacyHistoryItemClient::singleton(),
#if ENABLE(CONTEXT_MENUS)
        makeUniqueRef<ContextMenuClientQt>(),
#endif
        makeUniqueRef<ChromeClientQt>(this),
        makeUniqueRef<CryptoClientQt>(),
        makeUniqueRef<WebCore::ProcessSyncClient>()
    };
    pageConfiguration.applicationCacheStorage = ApplicationCacheStorage::create({ }, { }); // QTFIXME
    pageConfiguration.dragClient = makeUnique<DragClientQt>(pageConfiguration.chromeClient.ptr());
    pageConfiguration.inspectorClient = makeUnique<InspectorClientQt>(this);
    pageConfiguration.databaseProvider = &WebDatabaseProvider::singleton();
    pageConfiguration.pluginInfoProvider = &PluginInfoProviderQt::singleton();
    pageConfiguration.storageNamespaceProvider = WebKitLegacy::WebStorageNamespaceProvider::create(
        QWebSettings::globalSettings()->localStoragePath());
    pageConfiguration.visitedLinkStore = &VisitedLinkStoreQt::singleton();
    page = WebCore::Page::create(WTFMove(pageConfiguration));

#if ENABLE(GEOLOCATION)
    if (useMock) {
        // In case running in DumpRenderTree mode set the controller to mock provider.
        GeolocationClientMock* mock = new GeolocationClientMock;
        WebCore::provideGeolocationTo(page.get(), *mock);
        mock->setController(WebCore::GeolocationController::from(page.get()));
    }
#if HAVE(QTPOSITIONING)
    else
        WebCore::provideGeolocationTo(page.get(), *new GeolocationClientQt(this));
#endif
#endif

#if ENABLE(DEVICE_ORIENTATION)
    if (useMock) {
        m_deviceOrientationClient = new DeviceOrientationClientMock;
        m_deviceMotionClient = new DeviceMotionClientMock;
    }
#if HAVE(QTSENSORS)
    else {
        m_deviceOrientationClient =  new DeviceOrientationClientQt;
        m_deviceMotionClient = new DeviceMotionClientQt;
    }
#endif
    if (m_deviceOrientationClient)
        WebCore::provideDeviceOrientationTo(page.get(), m_deviceOrientationClient);
    if (m_deviceMotionClient)
        WebCore::provideDeviceMotionTo(page.get(), m_deviceMotionClient);
#endif

    // By default each page is put into their own unique page group, which affects popup windows
    // and visited links. Page groups (per process only) is a feature making it possible to use
    // separate settings for each group, so that for instance an integrated browser/email reader
    // can use different settings for displaying HTML pages and HTML email. To make QtWebKit work
    // as expected out of the box, we use a default group similar to what other ports are doing.
    page->setGroupName("Default Group"_s);

    page->addLayoutMilestones(WebCore::LayoutMilestone::DidFirstVisuallyNonEmptyLayout);

    settings = new QWebSettings(page.get());

#if ENABLE(NOTIFICATIONS)
    WebCore::provideNotification(page.get(), NotificationPresenterClientQt::notificationPresenter());
#endif

    history.d = new QWebHistoryPrivate(static_cast<BackForwardList*>(&page->backForward().client()));
}

QWebPageAdapter::~QWebPageAdapter()
{
    delete settings;

#if ENABLE(NOTIFICATIONS)
    NotificationPresenterClientQt::notificationPresenter()->removeClient();
#endif
#if ENABLE(DEVICE_ORIENTATION)
    delete m_deviceMotionClient;
    delete m_deviceOrientationClient;
#endif
}

void QWebPageAdapter::deletePage()
{
    // Prevent xhr event listeners from firing after the loaders are cancelled
    mainFrameAdapter().frame->document()->domWindow()->stop();

    // Before we delete the page, detach the mainframe's loader
    FrameLoader& loader = mainFrameAdapter().frame->loader();
    loader.detachFromParent();
    page = nullptr;
}

QWebPageAdapter* QWebPageAdapter::kit(Page* page)
{
    return static_cast<ChromeClientQt&>(page->chrome().client()).m_webPage;
}

ViewportArguments QWebPageAdapter::viewportArguments() const
{
    return page ? page->viewportArguments() : WebCore::ViewportArguments();
}


void QWebPageAdapter::registerUndoStep(UndoStep& step)
{
    createUndoStep(QSharedPointer<UndoStepQt>(new UndoStepQt(step)));
}

void QWebPageAdapter::setVisibilityState(VisibilityState state)
{
    if (!page)
        return;
    page->setIsVisible(state == VisibilityStateVisible);
}

QWebPageAdapter::VisibilityState QWebPageAdapter::visibilityState() const
{
    return webCoreVisibilityStateToWebPageVisibilityState(page->visibilityState());
}

void QWebPageAdapter::setNetworkAccessManager(QNetworkAccessManager *manager)
{
    if (manager == networkManager)
        return;
    if (networkManager && networkManager->parent() == handle())
        delete networkManager;
    networkManager = manager;
}

QNetworkAccessManager* QWebPageAdapter::networkAccessManager()
{
    if (!networkManager) {
        networkManager = new QNetworkAccessManager(handle());
#if QT_VERSION >= QT_VERSION_CHECK(5, 9, 0)
        networkManager->setStrictTransportSecurityEnabled(true);
#endif
    }
    return networkManager;
}

bool QWebPageAdapter::hasSelection() const
{
    RefPtr frame = page->focusController().focusedOrMainFrame();
    if (!frame)
        return false;
    return !frame->selection().selection().isNone();
}

QString QWebPageAdapter::selectedText() const
{
    RefPtr frame = page->focusController().focusedOrMainFrame();
    if (!frame || frame->selection().selection().isNone())
        return QString();
    return frame->editor().selectedText();
}

QString QWebPageAdapter::selectedHtml() const
{
    RefPtr frame = page->focusController().focusedOrMainFrame();
    if (!frame)
        return QString();

    std::optional<SimpleRange> range = frame->editor().selectedRange();
    if (!range)
        return QString();
    return serializePreservingVisualAppearance(range.value());
}

bool QWebPageAdapter::isContentEditable() const
{
    return page->isEditable();
}

void QWebPageAdapter::setContentEditable(bool editable)
{
    page->setEditable(editable);
    page->setTabKeyCyclesThroughElements(!editable);

    LocalFrame* frame = mainFrameAdapter().frame;
    if (editable) {
        frame->editor().applyEditingStyleToBodyElement();
        // FIXME: mac port calls this if there is no selectedDOMRange
        // frame->setSelectionFromNone();
    }

}

bool QWebPageAdapter::findText(const QString& subString, FindFlag options)
{
    WebCore::FindOptions webCoreFindOptions;

    if (!(options & FindCaseSensitively))
        webCoreFindOptions.add(WebCore::FindOption::CaseInsensitive);

    if (options & FindBackward)
        webCoreFindOptions.add(WebCore::FindOption::Backwards);

    if (options & FindWrapsAroundDocument)
        webCoreFindOptions.add(WebCore::FindOption::WrapAround);

    if (options & FindAtWordBeginningsOnly)
        webCoreFindOptions.add(WebCore::FindOption::AtWordStarts);

    if (options & TreatMedialCapitalAsWordBeginning)
        webCoreFindOptions.add(WebCore::FindOption::TreatMedialCapitalAsWordStart);

    if (options & FindBeginsInSelection)
        webCoreFindOptions.add(WebCore::FindOption::StartInSelection);

    if (options & FindAtWordEndingsOnly)
        webCoreFindOptions.add(WebCore::FindOption::AtWordEnds);

    if (options & HighlightAllOccurrences) {
        if (subString.isEmpty()) {
            page->unmarkAllTextMatches();
            return true;
        }
        return page->markAllMatchesForText(subString, webCoreFindOptions, /*shouldHighlight*/ true, /*limit*/ 0);
    }

    if (subString.isEmpty()) {
        auto* localFrame = dynamicDowncast<WebCore::LocalFrame>(page->mainFrame());
        if (localFrame)
            localFrame->selection().clear();

        Frame* frame = page->mainFrame().tree().firstChild();
        while (frame) {
            auto* localFrame = dynamicDowncast<WebCore::LocalFrame>(frame);
            localFrame->selection().clear();
            frame = localFrame->tree().traverseNext(CanWrap::No);
        }
    }

    return page->findString(subString, webCoreFindOptions).has_value();
}

void QWebPageAdapter::adjustPointForClicking(QMouseEvent* ev)
{
#if ENABLE(TOUCH_ADJUSTMENT)
    QtPlatformPlugin platformPlugin;
    std::unique_ptr<QWebTouchModifier> touchModifier = platformPlugin.createTouchModifier();
    if (!touchModifier)
        return;

    unsigned topPadding = touchModifier->hitTestPaddingForTouch(QWebTouchModifier::Up);
    unsigned rightPadding = touchModifier->hitTestPaddingForTouch(QWebTouchModifier::Right);
    unsigned bottomPadding = touchModifier->hitTestPaddingForTouch(QWebTouchModifier::Down);
    unsigned leftPadding = touchModifier->hitTestPaddingForTouch(QWebTouchModifier::Left);

    touchModifier = nullptr;

    if (!topPadding && !rightPadding && !bottomPadding && !leftPadding)
        return;

    FrameView* view = page->mainFrame().view();
    ASSERT(view);
    if (view->scrollbarAtPoint(ev->pos()))
        return;

    IntRect touchRect(ev->pos().x() - leftPadding, ev->pos().y() - topPadding, leftPadding + rightPadding, topPadding + bottomPadding);
    IntPoint adjustedPoint;
    Node* adjustedNode;
    bool foundClickableNode = page->mainFrame().eventHandler().bestClickableNodeForTouchPoint(touchRect.center(), touchRect.size(), adjustedPoint, adjustedNode);
    if (!foundClickableNode)
        return;

    *ev = QMouseEvent(ev->type(), QPoint(adjustedPoint), ev->globalPos(), ev->button(), ev->buttons(), ev->modifiers());
#else
    Q_UNUSED(ev);
#endif
}

bool QWebPageAdapter::tryClosePage()
{
    return mainFrameAdapter().frame->loader().shouldClose();
}

void QWebPageAdapter::mouseMoveEvent(QMouseEvent* ev)
{
    WebCore::LocalFrame* frame = mainFrameAdapter().frame;
    if (!frame->view())
        return;
    if (ev->buttons() == Qt::NoButton)
        mousePressed = false;

    bool accepted = frame->eventHandler().mouseMoved(convertMouseEvent(ev, 0)).wasHandled();
    ev->setAccepted(accepted);
}

void QWebPageAdapter::mousePressEvent(QMouseEvent* ev)
{
    WebCore::LocalFrame* frame = mainFrameAdapter().frame;
    if (!frame->view())
        return;

    RefPtr<WebCore::Node> oldNode;
    LocalFrame* focusedFrame = page->focusController().focusedLocalFrame();
    if (Document* focusedDocument = focusedFrame ? focusedFrame->document() : 0)
        oldNode = focusedDocument->focusedElement();

    if (tripleClickTimer.isActive()
        && (ev->pos() - tripleClick).manhattanLength() < qGuiApp->styleHints()->startDragDistance()) {
        mouseTripleClickEvent(ev);
        return;
    }

    bool accepted = false;
    PlatformMouseEvent mev = convertMouseEvent(ev, 1);
    // ignore the event if we can't map Qt's mouse buttons to WebCore::MouseButton
    if (mev.button() != MouseButton::None)
        mousePressed = accepted = frame->eventHandler().handleMousePressEvent(mev).wasHandled();
    ev->setAccepted(accepted);

    RefPtr<WebCore::Node> newNode;
    focusedFrame = page->focusController().focusedLocalFrame();
    if (Document* focusedDocument = focusedFrame ? focusedFrame->document() : 0)
        newNode = focusedDocument->focusedElement();

    if (newNode && oldNode != newNode)
        clickCausedFocus = true;
}

void QWebPageAdapter::mouseDoubleClickEvent(QMouseEvent *ev)
{
    WebCore::LocalFrame* frame = mainFrameAdapter().frame;
    if (!frame->view())
        return;

    bool accepted = false;
    PlatformMouseEvent mev = convertMouseEvent(ev, 2);
    // ignore the event if we can't map Qt's mouse buttons to WebCore::MouseButton
    if (mev.button() != MouseButton::None)
        accepted = frame->eventHandler().handleMousePressEvent(mev).wasHandled();
    ev->setAccepted(accepted);

    tripleClickTimer.start(qGuiApp->styleHints()->mouseDoubleClickInterval(), handle());
    tripleClick = QPointF(ev->pos()).toPoint();
}

void QWebPageAdapter::mouseTripleClickEvent(QMouseEvent *ev)
{
    WebCore::LocalFrame* frame = mainFrameAdapter().frame;
    if (!frame->view())
        return;

    bool accepted = false;
    PlatformMouseEvent mev = convertMouseEvent(ev, 3);
    // ignore the event if we can't map Qt's mouse buttons to WebCore::MouseButton
    if (mev.button() != MouseButton::None)
        accepted = frame->eventHandler().handleMousePressEvent(mev).wasHandled();
    ev->setAccepted(accepted);
}

void QWebPageAdapter::mouseReleaseEvent(QMouseEvent *ev)
{
    WebCore::LocalFrame* frame = mainFrameAdapter().frame;
    if (!frame->view())
        return;

    bool accepted = false;
    PlatformMouseEvent mev = convertMouseEvent(ev, 0);
    // ignore the event if we can't map Qt's mouse buttons to WebCore::MouseButton
    if (mev.button() != MouseButton::None)
        accepted = frame->eventHandler().handleMouseReleaseEvent(mev).wasHandled();
    ev->setAccepted(accepted);

    if (ev->buttons() == Qt::NoButton)
        mousePressed = false;

    handleSoftwareInputPanel(ev->button(), QPointF(ev->pos()).toPoint());
}

void QWebPageAdapter::handleSoftwareInputPanel(Qt::MouseButton button, const QPoint& pos)
{
    LocalFrame* frame = page->focusController().focusedLocalFrame();
    if (!frame)
        return;

    if (client && client->inputMethodEnabled()
        && frame->document()->focusedElement()
            && button == Qt::LeftButton && qGuiApp->property("autoSipEnabled").toBool()) {
        if (!clickCausedFocus || requestSoftwareInputPanel()) {
            constexpr OptionSet<HitTestRequest::Type> hitType { HitTestRequest::Type::ReadOnly, HitTestRequest::Type::Active, HitTestRequest::Type::IgnoreClipping, HitTestRequest::Type::DisallowUserAgentShadowContent };
            HitTestResult result = frame->eventHandler().hitTestResultAtPoint(frame->view()->windowToContents(pos), hitType);
            if (result.isContentEditable()) {
                QEvent event(QEvent::RequestSoftwareInputPanel);
                QGuiApplication::sendEvent(client->ownerWidget(), &event);
            }
        }
    }

    clickCausedFocus = false;
}

#ifndef QT_NO_WHEELEVENT
void QWebPageAdapter::wheelEvent(QWheelEvent *ev, int wheelScrollLines)
{
    WebCore::LocalFrame* frame = mainFrameAdapter().frame;
    if (!frame->view())
        return;

    PlatformWheelEvent pev = convertWheelEvent(ev, wheelScrollLines);
    auto [result, _] = frame->eventHandler().handleWheelEvent(pev, { WheelEventProcessingSteps::SynchronousScrolling, WheelEventProcessingSteps::BlockingDOMEventDispatch });
    ev->setAccepted(result.wasHandled());
}
#endif // QT_NO_WHEELEVENT

#if ENABLE(DRAG_SUPPORT)

Qt::DropAction QWebPageAdapter::dragEntered(const QMimeData *data, const QPoint &pos, Qt::DropActions possibleActions)
{
    DragData dragData(data, pos, QCursor::pos(), dropActionToDragOp(possibleActions));
    WebCore::LocalFrame* localFrame = mainFrameAdapter().frame;
    return dragOpToDropAction(page->dragController().dragEnteredOrUpdated(*localFrame, WTFMove(dragData)));
}

void QWebPageAdapter::dragLeaveEvent()
{
    DragData dragData(0, IntPoint(), QCursor::pos(), DragOperation::Generic);
    WebCore::LocalFrame* localFrame = mainFrameAdapter().frame;
    page->dragController().dragExited(*localFrame, WTFMove(dragData));
}

Qt::DropAction QWebPageAdapter::dragUpdated(const QMimeData *data, const QPoint &pos, Qt::DropActions possibleActions)
{
    DragData dragData(data, pos, QCursor::pos(), dropActionToDragOp(possibleActions));
    WebCore::LocalFrame* localFrame = mainFrameAdapter().frame;
    return dragOpToDropAction(page->dragController().dragEnteredOrUpdated(*localFrame, WTFMove(dragData)));
}

bool QWebPageAdapter::performDrag(const QMimeData *data, const QPoint &pos, Qt::DropActions possibleActions)
{
    DragData dragData(data, pos, QCursor::pos(), dropActionToDragOp(possibleActions));
    return page->dragController().performDragOperation(WTFMove(dragData));
}

#endif // ENABLE(DRAG_SUPPORT)

void QWebPageAdapter::inputMethodEvent(QInputMethodEvent *ev)
{
    RefPtr frame = page->focusController().focusedOrMainFrame();
    if (!frame)
        return;
    WebCore::Editor& editor = frame->editor();

    if (!editor.canEdit()) {
        ev->ignore();
        return;
    }

    Node* node = 0;
    if (frame->selection().selection().rootEditableElement())
        node = frame->selection().selection().rootEditableElement()->shadowHost();

    Vector<CompositionUnderline> underlines;
    bool hasSelection = false;

    for (int i = 0; i < ev->attributes().size(); ++i) {
        const QInputMethodEvent::Attribute& a = ev->attributes().at(i);
        switch (a.type) {
        case QInputMethodEvent::TextFormat: {
            Color color = Color::black;
            CompositionUnderlineColor compositionUnderlineColor = CompositionUnderlineColor::TextColor;

            QTextCharFormat textCharFormat = a.value.value<QTextFormat>().toCharFormat();
            QColor qcolor = textCharFormat.underlineColor();
            if (qcolor.isValid()) {
                color = qcolor;
                compositionUnderlineColor = CompositionUnderlineColor::GivenColor;
            }
            underlines.append(CompositionUnderline(qMin(a.start, (a.start + a.length)), qMax(a.start, (a.start + a.length)), compositionUnderlineColor, color, false));
            break;
        }
        case QInputMethodEvent::Cursor: {
            if (a.length > 0) {
                frame->selection().removeCaretVisibilitySuppressionReason(CaretVisibilitySuppressionReason::IsNotFocusedOrActive);
                RenderObject* caretRenderer = frame->selection().caretRendererWithoutUpdatingLayout();
                if (caretRenderer) {
                    QColor qcolor = a.value.value<QColor>();
//                    caretRenderer->style().setColor(qcolor);
                }
            } else {
                frame->selection().addCaretVisibilitySuppressionReason(CaretVisibilitySuppressionReason::IsNotFocusedOrActive);
            }
            break;
        }
        case QInputMethodEvent::Selection: {
            hasSelection = true;
            // A selection in the inputMethodEvent is always reflected in the visible text
            if (node) {
                if (is<HTMLTextFormControlElement>(node))
                    downcast<HTMLTextFormControlElement>(node)->setSelectionRange(qMin(a.start, (a.start + a.length)), qMax(a.start, (a.start + a.length)));
            }

            if (!ev->preeditString().isEmpty())
                editor.setComposition(ev->preeditString(), underlines, { }, { }, qMin(a.start, (a.start + a.length)), qMax(a.start, (a.start + a.length)));
            else {
                // If we are in the middle of a composition, an empty pre-edit string and a selection of zero
                // cancels the current composition
                if (editor.hasComposition() && !(a.start + a.length))
                    editor.setComposition(QString(), underlines, { }, { }, 0, 0);
            }
            break;
        }
        default:
            break;
        }
    }

    if (node && ev->replacementLength() > 0) {
        int cursorPos = frame->selection().selection().extent().offsetInContainerNode();
        int start = cursorPos + ev->replacementStart();
        if (is<HTMLTextFormControlElement>(node))
            downcast<HTMLTextFormControlElement>(node)->setSelectionRange(start, start + ev->replacementLength());
        // Commit regardless of whether commitString is empty, to get rid of selection.
        editor.confirmComposition(ev->commitString());
    } else if (!ev->commitString().isEmpty()) {
        if (editor.hasComposition())
            editor.confirmComposition(ev->commitString());
        else
            editor.insertText(ev->commitString(), 0);
    } else if (!hasSelection && !ev->preeditString().isEmpty())
        editor.setComposition(ev->preeditString(), underlines, { }, { }, 0, 0);
    else if (ev->preeditString().isEmpty() && editor.hasComposition())
        editor.confirmComposition(String());

    ev->accept();
}

QVariant QWebPageAdapter::inputMethodQuery(Qt::InputMethodQuery property) const
{
    RefPtr frame = page->focusController().focusedLocalFrame();
    if (!frame)
        return QVariant();

    WebCore::Editor& editor = frame->editor();

    RenderObject* renderer = 0;
    RenderTextControl* renderTextControl = 0;

    if (auto* rootEditableElement = frame->selection().selection().rootEditableElement()) {
        if (auto* shadowHost = rootEditableElement->shadowHost())
            renderer = shadowHost->renderer();
        else
            renderer = rootEditableElement->renderer();
    }

    if (renderer && renderer->isRenderTextControl())
        renderTextControl = downcast<RenderTextControl>(renderer);

    switch (property) {
    case Qt::ImCursorRectangle: {
        WebCore::LocalFrameView* view = frame->view();
        if (view && view->needsLayout()) {
            // We can't access absoluteCaretBounds() while the view needs to layout.
            return QVariant();
        }
        return QVariant(view->contentsToWindow(frame->selection().absoluteCaretBounds()));
    }
    case Qt::ImFont: {
        if (renderTextControl) {
            const auto& renderStyle = renderTextControl->style();
            return QVariant(QFont(renderStyle.fontCascade().syntheticFont()));
        }
        return QVariant(QFont());
    }
    case Qt::ImCursorPosition: {
        if (editor.hasComposition())
            return QVariant(frame->selection().selection().end().offsetInContainerNode());
        return QVariant(frame->selection().selection().extent().offsetInContainerNode());
    }
    case Qt::ImSurroundingText: {
        if (renderTextControl) {
            QString text = renderTextControl->textFormControlElement().value();
            std::optional<SimpleRange> range = editor.compositionRange();
            if (range)
                text.remove(range->startOffset(), range->endOffset() - range->startOffset());
            return QVariant(text);
        }
        return QVariant();
    }
    case Qt::ImCurrentSelection: {
        if (!editor.hasComposition() && renderTextControl) {
            int start = frame->selection().selection().start().offsetInContainerNode();
            int end = frame->selection().selection().end().offsetInContainerNode();
            if (end > start)
                return QVariant(QString(renderTextControl->textFormControlElement().value()).mid(start, end - start));
        }
        return QVariant();

    }
    case Qt::ImAnchorPosition: {
        if (editor.hasComposition())
            return QVariant(frame->selection().selection().start().offsetInContainerNode());
        return QVariant(frame->selection().selection().base().offsetInContainerNode());
    }
    case Qt::ImMaximumTextLength: {
        if (frame->selection().selection().isContentEditable()) {
            if (frame->document() && frame->document()->focusedElement()) {
                if (is<HTMLInputElement>(frame->document()->focusedElement())) {
                    HTMLInputElement* inputElement = downcast<HTMLInputElement>(frame->document()->focusedElement());
                    return QVariant(inputElement->effectiveMaxLength());
                }
            }
            return QVariant(HTMLInputElement::maxEffectiveLength);
        }
        return QVariant(0);
    }
    default:
        return QVariant();
    }
}

typedef struct {
    const char* name;
    double deferredRepaintDelay;
    double initialDeferredRepaintDelayDuringLoading;
    double maxDeferredRepaintDelayDuringLoading;
    double deferredRepaintDelayIncrementDuringLoading;
} QRepaintThrottlingPreset;

void QWebPageAdapter::dynamicPropertyChangeEvent(QObject* obj, QDynamicPropertyChangeEvent* event)
{
    if (event->propertyName() == "_q_webInspectorServerPort") {
        QVariant port = obj->property("_q_webInspectorServerPort");
        if (port.isValid()) {
            InspectorServerQt* inspectorServer = InspectorServerQt::server();
            inspectorServer->listen(port.toInt());
        }
    } else if (event->propertyName() == "_q_deadDecodedDataDeletionInterval") {
        double interval = obj->property("_q_deadDecodedDataDeletionInterval").toDouble();
        MemoryCache::singleton().setDeadDecodedDataDeletionInterval(Seconds(interval));
    }  else if (event->propertyName() == "_q_useNativeVirtualKeyAsDOMKey") {
        m_useNativeVirtualKeyAsDOMKey = obj->property("_q_useNativeVirtualKeyAsDOMKey").toBool();
    }
}

#define MAP_ACTION_FROM_VALUE(Name, Value) \
    case Value: return QWebPageAdapter::Name

static int adapterActionForContextMenuAction(WebCore::ContextMenuAction action)
{
    if (action >= ContextMenuItemBaseCustomTag && action <= ContextMenuItemLastCustomTag)
        return action;

    switch (action) {
        FOR_EACH_MAPPED_MENU_ACTION(MAP_ACTION_FROM_VALUE, SEMICOLON_SEPARATOR);
    case WebCore::ContextMenuItemTagInspectElement:
        return QWebPageAdapter::InspectElement;
    default:
        break;
    }
    return QWebPageAdapter::NoAction;
}

QList<MenuItem> descriptionForPlatformMenu(const Vector<ContextMenuItem>& items, Page* page)
{
    QList<MenuItem> itemDescriptions;
    if (!items.size())
        return itemDescriptions;
    for (const auto& item : items) {
        MenuItem description;
        switch (item.type()) {
        case WebCore::ContextMenuItemType::CheckableAction: /* fall through */
        case WebCore::ContextMenuItemType::Action: {
            int action = adapterActionForContextMenuAction(item.action());
            if (action > QWebPageAdapter::NoAction) {
                description.type = MenuItem::Action;
                description.action = action;
                ContextMenuItem it(item);
                page->contextMenuController().checkOrEnableIfNeeded(it);
                if (it.enabled())
                    description.traits |= MenuItem::Enabled;
                if (item.type() == WebCore::ContextMenuItemType::CheckableAction) {
                    description.traits |= MenuItem::Checkable;
                    if (it.checked())
                        description.traits |= MenuItem::Checked;
                }
                description.title = item.title();
            }
            break;
        }
        case WebCore::ContextMenuItemType::Separator:
            description.type = MenuItem::Separator;
            break;
        case WebCore::ContextMenuItemType::Submenu: {
            description.type = MenuItem::SubMenu;
            description.subMenu = descriptionForPlatformMenu(item.subMenuItems(), page);
            description.title = item.title();
            // Don't append empty submenu descriptions.
            if (description.subMenu.isEmpty())
                continue;
        }
        }
        if (description.type > MenuItem::NoType)
            itemDescriptions.append(description);
    }
    return itemDescriptions;
}

QWebHitTestResultPrivate* QWebPageAdapter::updatePositionDependentMenuActions(const QPoint& pos, QBitArray* visitedWebActions)
{
    ASSERT(visitedWebActions);
    RefPtr focusedFrame = page->focusController().focusedOrMainFrame();
    if (!focusedFrame)
        return 0;

    constexpr OptionSet<HitTestRequest::Type> hitType { HitTestRequest::Type::ReadOnly, HitTestRequest::Type::Active, HitTestRequest::Type::IgnoreClipping, HitTestRequest::Type::DisallowUserAgentShadowContent };
    HitTestResult result = focusedFrame->eventHandler().hitTestResultAtPoint(focusedFrame->view()->windowToContents(pos), hitType);
    page->contextMenuController().setHitTestResult(result);

    if (page->inspectorController().enabled())
        page->contextMenuController().addDebuggingItems();

    WebCore::ContextMenu* webcoreMenu = page->contextMenuController().contextMenu();
    QList<MenuItem> itemDescriptions;
    if (client && webcoreMenu)
        itemDescriptions = descriptionForPlatformMenu(webcoreMenu->items(), page.get());
    createAndSetCurrentContextMenu(itemDescriptions, visitedWebActions);
    if (result.scrollbar())
        return 0;
    return new QWebHitTestResultPrivate(result);
}

QStringList QWebPageAdapter::supportedContentTypes() const
{
    QStringList mimeTypes;

    for (auto& type : MIMETypeRegistry::supportedImageMIMETypes())
        mimeTypes << String(type);

    for (auto& type : MIMETypeRegistry::supportedNonImageMIMETypes())
        mimeTypes << String(type);

    return mimeTypes;
}

void QWebPageAdapter::_q_cleanupLeakMessages()
{
#ifndef NDEBUG
    // Need this to make leak messages accurate.
    MemoryCache::singleton().setCapacities(0, 0, 0);
#endif
}

void QWebPageAdapter::_q_onLoadProgressChanged(int)
{
    m_totalBytes = page->progress().totalPageAndResourceBytesToLoad();
    m_bytesReceived = page->progress().totalBytesReceived();
}

bool QWebPageAdapter::supportsContentType(const QString& mimeType) const
{
    const String type = mimeType.toLower();
    if (MIMETypeRegistry::isSupportedImageMIMEType(type))
        return true;

    if (MIMETypeRegistry::isSupportedNonImageMIMEType(type))
        return true;

    return false;
}

void QWebPageAdapter::didShowInspector()
{
    page->inspectorController().show();
}

void QWebPageAdapter::didCloseInspector()
{
    InspectorClient* inspectorClient = page->inspectorController().inspectorClient();
    if (inspectorClient)
        static_cast<InspectorClientQt*>(inspectorClient)->closeFrontendWindow();
}

void QWebPageAdapter::updateActionInternal(QWebPageAdapter::MenuAction action, const char* commandName, bool* enabled, bool* checked)
{
    WebCore::FrameLoader& loader = mainFrameAdapter().frame->loader();
    RefPtr focusedFrame = page->focusController().focusedOrMainFrame();
    if (!focusedFrame)
        return;
    WebCore::Editor& editor = focusedFrame->editor();

    switch (action) {
    case QWebPageAdapter::Back:
        *enabled = page->backForward().canGoBackOrForward(-1);
        break;
    case QWebPageAdapter::Forward:
        *enabled = page->backForward().canGoBackOrForward(1);
        break;
    case QWebPageAdapter::Stop:
        *enabled = loader.isLoading();
        break;
    case QWebPageAdapter::Reload:
        *enabled = !loader.isLoading();
        break;
    case QWebPageAdapter::SetTextDirectionDefault:
    case QWebPageAdapter::SetTextDirectionLeftToRight:
    case QWebPageAdapter::SetTextDirectionRightToLeft:
        *enabled = editor.canEdit();
        *checked = false;
        break;
    default: {

        // if it's an editor command, let its logic determine state
        if (commandName) {
            Editor::Command command = editor.command(String::fromLatin1(commandName));
            *enabled = command.isEnabled();
            if (*enabled)
                *checked = command.state() != TriState::False;
            else
                *checked = false;
        }
        break;
    }
    }
}

#if ENABLE(VIDEO)
static WebCore::HTMLMediaElement* mediaElement(WebCore::Node* innerNonSharedNode)
{
    if (!innerNonSharedNode)
        return 0;

    if (!(innerNonSharedNode->renderer() && innerNonSharedNode->renderer()->isRenderMedia()))
        return 0;

    if (innerNonSharedNode->hasTagName(WebCore::HTMLNames::videoTag) || innerNonSharedNode->hasTagName(WebCore::HTMLNames::audioTag))
        return downcast<HTMLMediaElement>(innerNonSharedNode);
    return 0;
}
#endif

void QWebPageAdapter::triggerAction(QWebPageAdapter::MenuAction action, QWebHitTestResultPrivate* hitTestResult, const char* commandName, bool endToEndReload)
{
    RefPtr focusedFrame = page->focusController().focusedOrMainFrame();
    if (!focusedFrame)
        return;
    LocalFrame& frame = *focusedFrame;
    Editor& editor = frame.editor();

    // Convenience
    QWebHitTestResultPrivate hitTest;
    if (!hitTestResult)
        hitTestResult = &hitTest;

    switch (action) {
    case OpenLink:
        if (auto* targetFrame = dynamicDowncast<LocalFrame>(hitTestResult->webCoreFrame)) {
            targetFrame->loader().loadFrameRequest(frameLoadRequest(hitTestResult->linkUrl, *targetFrame), /*event*/ 0, /*FormState*/ nullptr);
            break;
        }
        // fall through
    case OpenLinkInNewWindow:
        openNewWindow(hitTestResult->linkUrl, frame);
        break;
    case OpenLinkInThisWindow:
        frame.loader().loadFrameRequest(frameLoadRequest(hitTestResult->linkUrl, frame), /*event*/ 0, /*FormState*/ nullptr);
        break;
    case OpenFrameInNewWindow: {
        URL url = frame.loader().documentLoader()->unreachableURL();
        if (url.isEmpty())
            url = frame.loader().documentLoader()->url();
        openNewWindow(url, frame);
        break;
        }
    case CopyLinkToClipboard:
#if defined(Q_WS_X11)
        editor.copyURL(hitTestResult->linkUrl, hitTestResult->linkText, *Pasteboard::createForGlobalSelection());
#endif
        editor.copyURL(hitTestResult->linkUrl, hitTestResult->linkText);
        break;
    case OpenImageInNewWindow:
        openNewWindow(hitTestResult->imageUrl, frame);
        break;
    case DownloadImageToDisk:
        frame.loader().client().startDownload(WebCore::ResourceRequest(hitTestResult->imageUrl, frame.loader().outgoingReferrer()));
        break;
    case DownloadLinkToDisk:
        frame.loader().client().startDownload(WebCore::ResourceRequest(hitTestResult->linkUrl, frame.loader().outgoingReferrer()));
        break;
    case DownloadMediaToDisk:
        frame.loader().client().startDownload(WebCore::ResourceRequest(hitTestResult->mediaUrl, frame.loader().outgoingReferrer()));
        break;
    case Back:
        page->backForward().goBack();
        break;
    case Forward:
        page->backForward().goForward();
        break;
    case Stop:
        mainFrameAdapter().frame->loader().stopForUserCancel();
        updateNavigationActions();
        break;
    case Reload: {
        OptionSet<ReloadOption> options;
        if (endToEndReload)
            options.add(ReloadOption::FromOrigin);
        mainFrameAdapter().frame->loader().reload(options);
        break;
    }

    case SetTextDirectionDefault:
        editor.setBaseWritingDirection(WritingDirection::Natural);
        break;
    case SetTextDirectionLeftToRight:
        editor.setBaseWritingDirection(WritingDirection::LeftToRight);
        break;
    case SetTextDirectionRightToLeft:
        editor.setBaseWritingDirection(WritingDirection::RightToLeft);
        break;
#if ENABLE(VIDEO)
    case ToggleMediaControls:
        if (HTMLMediaElement* mediaElt = mediaElement(hitTestResult->innerNonSharedNode))
            mediaElt->setControls(!mediaElt->controls());
        break;
    case ToggleMediaLoop:
        if (HTMLMediaElement* mediaElt = mediaElement(hitTestResult->innerNonSharedNode))
            mediaElt->setLoop(!mediaElt->loop());
        break;
    case ToggleMediaPlayPause:
        if (HTMLMediaElement* mediaElt = mediaElement(hitTestResult->innerNonSharedNode))
            mediaElt->togglePlayState();
        break;
    case ToggleMediaMute:
        if (HTMLMediaElement* mediaElt = mediaElement(hitTestResult->innerNonSharedNode))
            mediaElt->setMuted(!mediaElt->muted());
        break;
    case ToggleVideoFullscreen:
        if (HTMLMediaElement* mediaElt = mediaElement(hitTestResult->innerNonSharedNode)) {
            if (mediaElt->isVideo() && mediaElt->supportsFullscreen(HTMLMediaElementEnums::VideoFullscreenModeStandard)) {
                UserGestureIndicator indicator(IsProcessingUserGesture);
                mediaElt->toggleStandardFullscreenState();
            }
        }
        break;
#endif
    case InspectElement: {
        ASSERT(hitTestResult != &hitTest);
        page->inspectorController().inspect(hitTestResult->innerNonSharedNode);
        break;
    }
    default:
        if (commandName)
            editor.command(String::fromLatin1(commandName)).execute();
        break;
    }
}

void QWebPageAdapter::triggerCustomAction(int action, const QString &title)
{
    if (action >= ContextMenuItemBaseCustomTag && action <= ContextMenuItemLastCustomTag)
        page->contextMenuController().contextMenuItemSelected(static_cast<ContextMenuAction>(action), title);
    else
        ASSERT_NOT_REACHED();
}


QString QWebPageAdapter::contextMenuItemTagForAction(QWebPageAdapter::MenuAction action, bool* checkable) const
{
    ASSERT(checkable);
    switch (action) {
    case OpenLink:
        return contextMenuItemTagOpenLink();
    case OpenLinkInNewWindow:
        return contextMenuItemTagOpenLinkInNewWindow();
    case OpenFrameInNewWindow:
        return contextMenuItemTagOpenFrameInNewWindow();
    case OpenLinkInThisWindow:
        return contextMenuItemTagOpenLinkInThisWindow();

    case DownloadLinkToDisk:
        return contextMenuItemTagDownloadLinkToDisk();
    case CopyLinkToClipboard:
        return contextMenuItemTagCopyLinkToClipboard();

    case OpenImageInNewWindow:
        return contextMenuItemTagOpenImageInNewWindow();
    case DownloadImageToDisk:
        return contextMenuItemTagDownloadImageToDisk();
    case CopyImageToClipboard:
        return contextMenuItemTagCopyImageToClipboard();
    case CopyImageUrlToClipboard:
        return contextMenuItemTagCopyImageURLToClipboard();

    case Cut:
        return contextMenuItemTagCut();
    case Copy:
        return contextMenuItemTagCopy();
    case Paste:
        return contextMenuItemTagPaste();
    case SelectAll:
        return contextMenuItemTagSelectAll();

    case Back:
        return contextMenuItemTagGoBack();
    case Forward:
        return contextMenuItemTagGoForward();
    case Reload:
        return contextMenuItemTagReload();
    case Stop:
        return contextMenuItemTagStop();

    case SetTextDirectionDefault:
        return contextMenuItemTagDefaultDirection();
    case SetTextDirectionLeftToRight:
        *checkable = true;
        return contextMenuItemTagLeftToRight();
    case SetTextDirectionRightToLeft:
        *checkable = true;
        return contextMenuItemTagRightToLeft();

    case ToggleBold:
        *checkable = true;
        return contextMenuItemTagBold();
    case ToggleItalic:
        *checkable = true;
        return contextMenuItemTagItalic();
    case ToggleUnderline:
        *checkable = true;
        return contextMenuItemTagUnderline();
    case DownloadMediaToDisk:
        return contextMenuItemTagDownloadMediaToDisk();
    case CopyMediaUrlToClipboard:
        return contextMenuItemTagCopyMediaLinkToClipboard();
    case ToggleMediaControls:
        *checkable = true;
        return contextMenuItemTagShowMediaControls();
    case ToggleMediaLoop:
        *checkable = true;
        return contextMenuItemTagToggleMediaLoop();
    case ToggleMediaPlayPause:
        return QString();
//        return contextMenuItemTagMediaPlayPause();
    case ToggleMediaMute:
        *checkable = true;
        return contextMenuItemTagMediaMute();
    case ToggleVideoFullscreen:
        return contextMenuItemTagToggleVideoFullscreen();

    case InspectElement:
        return contextMenuItemTagInspectElement();
    default:
        ASSERT_NOT_REACHED();
        return QString();
    }
}

#if ENABLE(NOTIFICATIONS)
void QWebPageAdapter::setNotificationsAllowedForFrame(QWebFrameAdapter* frame, bool allowed)
{
    NotificationPresenterClientQt::notificationPresenter()->setNotificationsAllowedForFrame(frame->frame, allowed);
}

void QWebPageAdapter::addNotificationPresenterClient()
{
    NotificationPresenterClientQt::notificationPresenter()->addClient();
}

#ifndef QT_NO_SYSTEMTRAYICON
bool QWebPageAdapter::hasSystemTrayIcon() const
{
    return NotificationPresenterClientQt::notificationPresenter()->hasSystemTrayIcon();
}

void QWebPageAdapter::setSystemTrayIcon(QObject *icon)
{
    NotificationPresenterClientQt::notificationPresenter()->setSystemTrayIcon(icon);
}
#endif // QT_NO_SYSTEMTRAYICON
#endif // ENABLE(NOTIFICATIONS)

#if ENABLE(GEOLOCATION) && HAVE(QTPOSITIONING)
void QWebPageAdapter::setGeolocationEnabledForFrame(QWebFrameAdapter* frame, bool on)
{
    GeolocationPermissionClientQt::geolocationPermissionClient()->setPermission(frame, on);
}
#endif


QString QWebPageAdapter::defaultUserAgentString()
{
    return UserAgentQt::standardUserAgent(""_s);
}

bool QWebPageAdapter::treatSchemeAsLocal(const QString& scheme)
{
    return WebCore::LegacySchemeRegistry::shouldTreatURLSchemeAsLocal(String(scheme));
}

QObject* QWebPageAdapter::currentFrame() const
{
    RefPtr frame = page->focusController().focusedOrMainFrame();
    if (!frame)
        return 0;
    return frame->loader().networkingContext()->originatingObject();
}

bool QWebPageAdapter::hasFocusedNode() const
{
    bool hasFocus = false;
    RefPtr frame = page->focusController().focusedLocalFrame();
    if (frame) {
        Document* document = frame->document();
        hasFocus = document && document->focusedElement();
    }
    return hasFocus;
}

QWebPageAdapter::ViewportAttributes QWebPageAdapter::viewportAttributesForSize(const QSize &availableSize, const QSize &deviceSize) const
{
    static const int desktopWidth = 980;

    float devicePixelRatio = qt_defaultDpi() / WebCore::ViewportArguments::deprecatedTargetDPI;

    WebCore::ViewportAttributes conf = WebCore::computeViewportAttributes(viewportArguments(), desktopWidth, deviceSize.width(), deviceSize.height(), devicePixelRatio, availableSize);
    WebCore::restrictMinimumScaleFactorToViewportSize(conf, availableSize, devicePixelRatio);
    WebCore::restrictScaleFactorToInitialScaleIfNotUserScalable(conf);

    page->setDeviceScaleFactor(devicePixelRatio);
    QWebPageAdapter::ViewportAttributes result;

    result.size = QSizeF(conf.layoutSize.width(), conf.layoutSize.height());
    result.initialScaleFactor = conf.initialScale;
    result.minimumScaleFactor = conf.minimumScale;
    result.maximumScaleFactor = conf.maximumScale;
    result.devicePixelRatio = devicePixelRatio;
    result.isUserScalable = static_cast<bool>(conf.userScalable);

    return result;
}

void QWebPageAdapter::setDevicePixelRatio(float devicePixelRatio)
{
    page->setDeviceScaleFactor(devicePixelRatio);
}

float QWebPageAdapter::devicePixelRatio()
{
    return page->deviceScaleFactor();
}

bool QWebPageAdapter::isPlayingAudio() const
{
    return page->mediaState().contains(WebCore::MediaProducerMediaState::IsPlayingAudio);
}

const QWebElement& QWebPageAdapter::fullScreenElement() const
{
#if ENABLE(FULLSCREEN_API)
    return m_fullScreenElement;
#endif
}

void QWebPageAdapter::setFullScreenElement(const QWebElement& e)
{
#if ENABLE(FULLSCREEN_API)
    m_fullScreenElement = e;
#else
    UNUSED_PARAM(e);
#endif
}

bool QWebPageAdapter::handleKeyEvent(QKeyEvent *ev)
{
    RefPtr frame = page->focusController().focusedOrMainFrame();
    if (!frame)
        return false;
    return frame->eventHandler().keyEvent(PlatformKeyboardEvent(ev, m_useNativeVirtualKeyAsDOMKey));
}

bool QWebPageAdapter::handleScrolling(QKeyEvent *ev)
{
    RefPtr frame = page->focusController().focusedOrMainFrame();
    if (!frame)
        return false;
    WebCore::ScrollDirection direction;
    WebCore::ScrollGranularity granularity;

#ifndef QT_NO_SHORTCUT
    if (ev == QKeySequence::MoveToNextPage) {
        granularity = WebCore::ScrollGranularity::Page;
        direction = WebCore::ScrollDirection::ScrollDown;
    } else if (ev == QKeySequence::MoveToPreviousPage) {
        granularity = WebCore::ScrollGranularity::Page;
        direction = WebCore::ScrollDirection::ScrollUp;
    } else
#endif // QT_NO_SHORTCUT
    if ((ev->key() == Qt::Key_Up && ev->modifiers() & Qt::ControlModifier) || ev->key() == Qt::Key_Home) {
        granularity = WebCore::ScrollGranularity::Document;
        direction = WebCore::ScrollDirection::ScrollUp;
    } else if ((ev->key() == Qt::Key_Down && ev->modifiers() & Qt::ControlModifier) || ev->key() == Qt::Key_End) {
        granularity = WebCore::ScrollGranularity::Document;
        direction = WebCore::ScrollDirection::ScrollDown;
    } else {
        switch (ev->key()) {
        case Qt::Key_Up:
            granularity = WebCore::ScrollGranularity::Line;
            direction = WebCore::ScrollDirection::ScrollUp;
            break;
        case Qt::Key_Down:
            granularity = WebCore::ScrollGranularity::Line;
            direction = WebCore::ScrollDirection::ScrollDown;
            break;
        case Qt::Key_Left:
            granularity = WebCore::ScrollGranularity::Line;
            direction = WebCore::ScrollDirection::ScrollLeft;
            break;
        case Qt::Key_Right:
            granularity = WebCore::ScrollGranularity::Line;
            direction = WebCore::ScrollDirection::ScrollRight;
            break;
        default:
            return false;
        }
    }

    return frame->eventHandler().scrollRecursively(direction, granularity);
}

void QWebPageAdapter::focusInEvent(QFocusEvent *)
{
    FocusController& focusController = page->focusController();
    focusController.setActive(true);
    focusController.setFocused(true);
    if (!focusController.focusedFrame())
        focusController.setFocusedFrame(mainFrameAdapter().frame);
}

void QWebPageAdapter::focusOutEvent(QFocusEvent *)
{
    // only set the focused frame inactive so that we stop painting the caret
    // and the focus frame. But don't tell the focus controller so that upon
    // focusInEvent() we can re-activate the frame.
    FocusController& focusController = page->focusController();
    // Call setFocused first so that window.onblur doesn't get called twice
    focusController.setFocused(false);
    focusController.setActive(false);
}

bool QWebPageAdapter::handleShortcutOverrideEvent(QKeyEvent* event)
{
    RefPtr frame = page->focusController().focusedOrMainFrame();
    if (!frame)
        return false;
    WebCore::Editor& editor = frame->editor();
    if (!editor.canEdit())
        return false;
    if (event->modifiers() == Qt::NoModifier
        || event->modifiers() == Qt::ShiftModifier
        || event->modifiers() == Qt::KeypadModifier) {
        if (event->key() < Qt::Key_Escape)
            event->accept();
        else {
            switch (event->key()) {
            case Qt::Key_Return:
            case Qt::Key_Enter:
            case Qt::Key_Delete:
            case Qt::Key_Home:
            case Qt::Key_End:
            case Qt::Key_Backspace:
            case Qt::Key_Left:
            case Qt::Key_Right:
            case Qt::Key_Up:
            case Qt::Key_Down:
            case Qt::Key_Tab:
                event->accept();
            default:
                break;
            }
        }
    }
    return true;
}

bool QWebPageAdapter::touchEvent(QTouchEvent* event)
{
#if ENABLE(TOUCH_EVENTS)
    Frame* frame = mainFrameAdapter().frame;
    if (!frame->view() || !frame->document())
        return false;

    // If the document doesn't have touch-event handles, we just ignore it
    // and let QGuiApplication convert it to a mouse event.
    if (!frame->document()->hasTouchEventHandlers())
        return false;

    // Always accept the QTouchEvent so that we'll receive also TouchUpdate and TouchEnd events
    event->setAccepted(true);

    // Return whether the default action was cancelled in the JS event handler
    return frame->eventHandler().handleTouchEvent(convertTouchEvent(event));
#else
    event->ignore();
    return false;
#endif
}

bool QWebPageAdapter::swallowContextMenuEvent(QContextMenuEvent *event, QWebFrameAdapter *webFrame)
{
    // Check the first and last enum values match at least, since we cast.
    ASSERT(int(QWebPageAdapter::ScrollUp) == int(WebCore::ScrollDirection::ScrollUp));
    ASSERT(int(QWebPageAdapter::ScrollRight) == int(WebCore::ScrollDirection::ScrollRight));
    ASSERT(int(QWebPageAdapter::ScrollByLine) == int(WebCore::ScrollGranularity::Line));
    ASSERT(int(QWebPageAdapter::ScrollByDocument) == int(WebCore::ScrollGranularity::Document));

    page->contextMenuController().clearContextMenu();

    if (webFrame) {
        LocalFrame* frame = webFrame->frame;
        if (Scrollbar* scrollBar = frame->view()->scrollbarAtPoint(convertMouseEvent(event, 1).position())) {
            bool horizontal = (scrollBar->orientation() == WebCore::ScrollbarOrientation::Horizontal);
            QWebPageAdapter::ScrollDirection direction = QWebPageAdapter::InvalidScrollDirection;
            QWebPageAdapter::ScrollGranularity granularity = QWebPageAdapter::InvalidScrollGranularity;
            bool scroll = handleScrollbarContextMenuEvent(event, horizontal, &direction, &granularity);
            if (!scroll)
                return true;
            if (direction == QWebPageAdapter::InvalidScrollDirection || granularity == QWebPageAdapter::InvalidScrollGranularity) {
                ScrollbarTheme& theme = scrollBar->theme();
                // Set the pressed position to the middle of the thumb so that when we
                // do move, the delta will be from the current pixel position of the
                // thumb to the new position
                int position = theme.trackPosition(*scrollBar) + theme.thumbPosition(*scrollBar) + theme.thumbLength(*scrollBar) / 2;
                scrollBar->setPressedPos(position);
                const QPoint pos = scrollBar->convertFromContainingWindow(event->pos());
                scrollBar->moveThumb(horizontal ? pos.x() : pos.y());
            } else
                scrollBar->scrollableArea().scroll(WebCore::ScrollDirection(direction), WebCore::ScrollGranularity(granularity));
            return true;
        }
    }

    RefPtr focusedFrame = page->focusController().focusedOrMainFrame();
    if (!focusedFrame)
        return false;
    focusedFrame->eventHandler().sendContextMenuEvent(convertMouseEvent(event, 1));
    ContextMenu* menu = page->contextMenuController().contextMenu();
    // If the website defines its own handler then sendContextMenuEvent takes care of
    // calling/showing it and the context menu pointer will be zero. This is the case
    // on maps.google.com for example.

    return !menu;
}
