/*
    Copyright (C) 2008 Nokia Corporation and/or its subsidiary(-ies)

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

#include "qwebhistory.h"
#include "qwebhistory_p.h"

#include "BackForwardList.h"
#include "HistorySerialization.h"
#include "LegacyHistoryItemClient.h"
#include "QWebPageAdapter.h"
#include "VisitedLinkStoreQt.h"

#include <WebCore/KeyedDecoderQt.h>
#include <WebCore/KeyedEncoderQt.h>
#include <WebCore/Page.h>
#include <WebCore/ShouldTreatAsContinuingLoad.h>
#include <wtf/URL.h>
#include <wtf/text/WTFString.h>

#include <QSharedData>
#include <QJsonObject>

static const int HistoryStreamVersion = 3;

/*!
  \class QWebHistoryItem
  \since 4.4
  \brief The QWebHistoryItem class represents one item in the history of a QWebPage

  \inmodule QtWebKit

  Each QWebHistoryItem instance represents an entry in the history stack of a Web page,
  containing information about the page, its location, and when it was last visited.

  The following table shows the properties of the page held by the history item, and
  the functions used to access them.

  \table
  \header \li Function      \li Description
  \row    \li title()       \li The page title. (deprecated: returns empty string)
  \row    \li url()         \li The location of the page.
  \row    \li originalUrl() \li The URL used to access the page.
  \row    \li lastVisited() \li The date and time of the user's last visit to the page.
  \row    \li icon()        \li The icon associated with the page that was provided by the server.
  \row    \li userData()    \li The user specific data that was stored with the history item.
  \endtable

  \note QWebHistoryItem objects are value based, but \e{explicitly shared}. Changing
  a QWebHistoryItem instance by calling setUserData() will change all copies of that
  instance.

  \sa QWebHistory, QWebPage::history(), QWebHistoryInterface
*/

/*!
  Constructs a history item from \a other. The new item and \a other
  will share their data, and modifying either this item or \a other will
  modify both instances.
*/
QWebHistoryItem::QWebHistoryItem(const QWebHistoryItem &other)
    : d(other.d)
{
}

/*!
  Assigns the \a other history item to this. This item and \a other
  will share their data, and modifying either this item or \a other will
  modify both instances.
*/
QWebHistoryItem &QWebHistoryItem::operator=(const QWebHistoryItem &other)
{
    d = other.d;
    return *this;
}

/*!
  Destroys the history item.
*/
QWebHistoryItem::~QWebHistoryItem()
{
}

/*!
 Returns the original URL associated with the history item.

 \sa url()
*/
QUrl QWebHistoryItem::originalUrl() const
{
    if (d->item)
        return d->item->originalURL();
    return QUrl();
}


/*!
 Returns the URL associated with the history item.

 \sa originalUrl(), title(), lastVisited()
*/
QUrl QWebHistoryItem::url() const
{
    if (d->item)
        return d->item->url();
    return QUrl();
}


/*!
 Deprecated: title is no longer stored; returns an empty string.

 \sa icon(), url(), lastVisited()
*/
QString QWebHistoryItem::title() const
{
    return QString();
}


/*!
 Returns the date and time that the page associated with the item was last visited.

 \sa title(), icon(), url()
*/
QDateTime QWebHistoryItem::lastVisited() const
{
    // FIXME: See r162808
    //FIXME : this will be wrong unless we correctly set lastVisitedTime ourselves
//    if (d->item)
//        return QDateTime::fromTime_t((uint)d->item->lastVisitedTime());
    return QDateTime();
}


/*!
 Returns the icon associated with the history item.

 \sa title(), url(), lastVisited()
*/
QIcon QWebHistoryItem::icon() const
{
#if ENABLE(ICONDATABASE)
    if (d->item)
        return *WebCore::iconDatabase().synchronousNativeIconForPageURL(d->item->url(), WebCore::IntSize(16, 16));
#endif

    return QIcon();
}

/*!
  \since 4.5
  Returns the user specific data that was stored with the history item.

  \sa setUserData()
*/
QVariant QWebHistoryItem::userData() const
{
    if (d->item)
        return d->item->userData();
    return QVariant();
}

/*!
  \since 4.5

 Stores user specific data \a userData with the history item.
 
 \note All copies of this item will be modified.

 \sa userData()
*/
void QWebHistoryItem::setUserData(const QVariant& userData)
{
    if (d->item)
        d->item->setUserData(userData);
}

/*!*
  \internal
*/
QWebHistoryItem::QWebHistoryItem(QWebHistoryItemPrivate *priv)
{
    d = priv;
}

/*!
    \since 4.5
    Returns whether this is a valid history item.
*/
bool QWebHistoryItem::isValid() const
{
    return d->item;
}

QVariantMap QWebHistoryItem::toMap() const
{
    if (!isValid())
        return QVariantMap();

    WebCore::KeyedEncoderQt encoder;
    WebCore::encodeBackForwardTree(encoder, *d->item);
    return encoder.toMap();
}

void QWebHistoryItem::loadFromMap(const QVariantMap& map)
{
    WebCore::KeyedDecoderQt decoder { QVariantMap(map) };
    auto item = WebCore::HistoryItem::create(LegacyHistoryItemClient::singleton());
    if (WebCore::decodeBackForwardTree(decoder, item))
        d = new QWebHistoryItemPrivate(item);
}

/*!
  \class QWebHistory
  \since 4.4
  \brief The QWebHistory class represents the history of a QWebPage

  \inmodule QtWebKit

  Each QWebPage instance contains a history of visited pages that can be accessed
  by QWebPage::history(). QWebHistory represents this history and makes it possible
  to navigate it.

  The history uses the concept of a \e{current item}, dividing the pages visited
  into those that can be visited by navigating \e back and \e forward using the
  back() and forward() functions. The current item can be obtained by calling
  currentItem(), and an arbitrary item in the history can be made the current
  item by passing it to goToItem().

  A list of items describing the pages that can be visited by going back can be
  obtained by calling the backItems() function; similarly, items describing the
  pages ahead of the current page can be obtained with the forwardItems() function.
  The total list of items is obtained with the items() function.

  Just as with containers, functions are available to examine the history in terms
  of a list. Arbitrary items in the history can be obtained with itemAt(), the total
  number of items is given by count(), and the history can be cleared with the
  clear() function.

  QWebHistory's state can be saved to a QDataStream using the >> operator and loaded
  by using the << operator.

  \sa QWebHistoryItem, QWebHistoryInterface, QWebPage
*/


QWebHistory::QWebHistory()
    : d(0)
{
}

QWebHistory::~QWebHistory()
{
    delete d;
}

/*!
  Clears the history.

  \sa count(), items()
*/
void QWebHistory::clear()
{
    //shortcut to private BackForwardList
    BackForwardList* lst = d->lst;

    VisitedLinkStoreQt::singleton().removeAllVisitedLinks();

    //if count() == 0 then just return
    if (!lst->entries().size())
        return;

    RefPtr<WebCore::HistoryItem> current = lst->currentItem();
    int capacity = lst->capacity();
    lst->setCapacity(0);
    lst->setCapacity(capacity);   //revert capacity

    if (current) {
        auto page = lst->page().page;
        if (page) {
            lst->addItem(page->mainFrame().frameID(), *current); // insert old current item
            lst->goToItem(*current); // and set it as current again
        }
    }

    d->page()->updateNavigationActions();
}

/*!
  Returns a list of all items currently in the history.

  \sa count(), clear()
*/
QList<QWebHistoryItem> QWebHistory::items() const
{
    HistoryItemVector &items = d->lst->entries();

    QList<QWebHistoryItem> ret;
    for (const auto& item : items) {
        QWebHistoryItemPrivate *priv = new QWebHistoryItemPrivate(item);
        ret.append(QWebHistoryItem(priv));
    }
    return ret;
}

/*!
  Returns the list of items in the backwards history list.
  At most \a maxItems entries are returned.

  \sa forwardItems()
*/
QList<QWebHistoryItem> QWebHistory::backItems(int maxItems) const
{
    HistoryItemVector items(maxItems);
    d->lst->backListWithLimit(maxItems, items);

    QList<QWebHistoryItem> ret;
    for (unsigned i = 0; i < items.size(); ++i) {
        QWebHistoryItemPrivate *priv = new QWebHistoryItemPrivate(items[i].get());
        ret.append(QWebHistoryItem(priv));
    }
    return ret;
}

/*!
  Returns the list of items in the forward history list.
  At most \a maxItems entries are returned.

  \sa backItems()
*/
QList<QWebHistoryItem> QWebHistory::forwardItems(int maxItems) const
{
    HistoryItemVector items(maxItems);
    d->lst->forwardListWithLimit(maxItems, items);

    QList<QWebHistoryItem> ret;
    for (unsigned i = 0; i < items.size(); ++i) {
        QWebHistoryItemPrivate *priv = new QWebHistoryItemPrivate(items[i].get());
        ret.append(QWebHistoryItem(priv));
    }
    return ret;
}

/*!
  Returns true if there is an item preceding the current item in the history;
  otherwise returns false.

  \sa canGoForward()
*/
bool QWebHistory::canGoBack() const
{
    return d->lst->backListCount() > 0;
}

/*!
  Returns true if we have an item to go forward to; otherwise returns false.

  \sa canGoBack()
*/
bool QWebHistory::canGoForward() const
{
    return d->lst->forwardListCount() > 0;
}

/*!
  Set the current item to be the previous item in the history and goes to the
  corresponding page; i.e., goes back one history item.

  \sa forward(), goToItem()
*/
void QWebHistory::back()
{
    if (canGoBack())
        d->goToItem(d->lst->backItem());
}

/*!
  Sets the current item to be the next item in the history and goes to the
  corresponding page; i.e., goes forward one history item.

  \sa back(), goToItem()
*/
void QWebHistory::forward()
{
    if (canGoForward())
        d->goToItem(d->lst->forwardItem());
}

/*!
  Sets the current item to be the specified \a item in the history and goes to the page.

  \sa back(), forward()
*/
void QWebHistory::goToItem(const QWebHistoryItem &item)
{
    d->goToItem(item.d->item);
}

/*!
  Returns the item before the current item in the history.
*/
QWebHistoryItem QWebHistory::backItem() const
{
    WebCore::HistoryItem *i = d->lst->backItem().get();
    QWebHistoryItemPrivate *priv = new QWebHistoryItemPrivate(i);
    return QWebHistoryItem(priv);
}

/*!
  Returns the current item in the history.
*/
QWebHistoryItem QWebHistory::currentItem() const
{
    WebCore::HistoryItem *i = d->lst->currentItem().get();
    QWebHistoryItemPrivate *priv = new QWebHistoryItemPrivate(i);
    return QWebHistoryItem(priv);
}

/*!
  Returns the item after the current item in the history.
*/
QWebHistoryItem QWebHistory::forwardItem() const
{
    WebCore::HistoryItem *i = d->lst->forwardItem().get();
    QWebHistoryItemPrivate *priv = new QWebHistoryItemPrivate(i);
    return QWebHistoryItem(priv);
}

/*!
  \since 4.5
  Returns the index of the current item in history.
*/
int QWebHistory::currentItemIndex() const
{
    return d->lst->backListCount();
}

/*!
  Returns the item at index \a i in the history.
*/
QWebHistoryItem QWebHistory::itemAt(int i) const
{
    QWebHistoryItemPrivate *priv;
    if (i < 0 || i >= count())
        priv = new QWebHistoryItemPrivate(0);
    else {
        WebCore::HistoryItem& item = d->lst->entries()[i].get();
        priv = new QWebHistoryItemPrivate(item);
    }
    return QWebHistoryItem(priv);
}

/*!
    Returns the total number of items in the history.
*/
int QWebHistory::count() const
{
    return d->lst->entries().size();
}

/*!
  \since 4.5
  Returns the maximum number of items in the history.

  \sa setMaximumItemCount()
*/
int QWebHistory::maximumItemCount() const
{
    return d->lst->capacity();
}

/*!
  \since 4.5
  Sets the maximum number of items in the history to \a count.

  \sa maximumItemCount()
*/
void QWebHistory::setMaximumItemCount(int count)
{
    d->lst->setCapacity(count);
}

QVariantMap QWebHistory::toMap() const
{
    WebCore::KeyedEncoderQt encoder;
    encoder.encodeUInt32("currentItemIndex"_s, currentItemIndex());

    const HistoryItemVector &items = d->lst->entries();
    encoder.encodeObjects("history"_s, items.begin(), items.end(), [&encoder](WebCore::KeyedEncoder&, const WebCore::HistoryItem& item) {
        WebCore::encodeBackForwardTree(encoder, item);
    });

    return encoder.toMap();
}

void QWebHistory::loadFromMap(const QVariantMap& map)
{
    clear();

    // after clear() is new clear HistoryItem (at the end we had to remove it)
    WebCore::HistoryItem* nullItem = d->lst->currentItem().get();

    WebCore::KeyedDecoderQt decoder { QVariantMap(map) };

    int currentIndex;
    if (!decoder.decodeInt32("currentItemIndex"_s, currentIndex))
        return;

    auto* lst = d->lst;
    Vector<int> ignore;
    bool result = decoder.decodeObjects("history"_s, ignore, [&lst, &decoder](WebCore::KeyedDecoder&, int&) -> bool {
        auto item = WebCore::HistoryItem::create(LegacyHistoryItemClient::singleton());
        if (!WebCore::decodeBackForwardTree(decoder, item))
            return false;

        auto page = lst->page().page;
        if (page) {
            lst->addItem(page->mainFrame().frameID(), WTFMove(item));
            return true;
        } else {
            return false;
        }
    });

    if (result && !d->lst->entries().isEmpty()) {
        d->lst->removeItem(nullItem);
        goToItem(itemAt(currentIndex));
    }

    d->page()->updateNavigationActions();
}

/*!
  \since 4.6
  \fn QDataStream& operator<<(QDataStream& stream, const QWebHistory& history)
  \relates QWebHistory

  \brief The operator<< function streams a history into a data stream.

  It saves the \a history into the specified \a stream.
*/

QDataStream& operator<<(QDataStream& target, const QWebHistory& history)
{
    target << HistoryStreamVersion;
    target << history.toMap();
    return target;
}

/*!
  \fn QDataStream& operator>>(QDataStream& stream, QWebHistory& history)
  \relates QWebHistory
  \since 4.6

  \brief The operator>> function loads a history from a data stream.

  Loads a QWebHistory from the specified \a stream into the given \a history.
*/

QDataStream& operator>>(QDataStream& source, QWebHistory& history)
{
    // Clear first, to have the same behavior if our version doesn't match and if the HistoryItem's version doesn't.
    history.clear();

    // This version covers every field we serialize in qwebhistory.cpp and HistoryItemQt.cpp (like the HistoryItem::userData()).
    // HistoryItem has its own version in the stream covering the work done in encodeBackForwardTree.
    // If any of those two stream version changes, the effect should be the same and the QWebHistory should fail to restore.
    int version;
    source >> version;
    if (version != HistoryStreamVersion) {
        // We do not try to decode previous history stream versions.
        // Make sure that our history is cleared and mark the rest of the stream as invalid.
        ASSERT(history.count() <= 1);
        source.setStatus(QDataStream::ReadCorruptData);
        return source;
    }

    QVariantMap map;
    source >> map;
    history.loadFromMap(map);

    return source;
}

void QWebHistoryPrivate::goToItem(RefPtr<WebCore::HistoryItem>&& item)
{
    if (!item)
        return;

    auto page = lst->page().page;
    if (RefPtr localMainFrame = dynamicDowncast<WebCore::LocalFrame>(page->mainFrame()))
        page->goToItem(*localMainFrame, *item, WebCore::FrameLoadType::IndexedBackForward, WebCore::ShouldTreatAsContinuingLoad::No);
}

QWebPageAdapter* QWebHistoryPrivate::page()
{
    return &lst->page();
}

WebCore::HistoryItem* QWebHistoryItemPrivate::core(const QWebHistoryItem* q)
{
    return q->d->item;
}
