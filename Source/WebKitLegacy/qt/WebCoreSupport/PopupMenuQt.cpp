/*
 * This file is part of the popup menu implementation for <select> elements in WebCore.
 *
 * Copyright (C) 2008, 2009, 2010 Nokia Corporation and/or its subsidiary(-ies)
 * Copyright (C) 2006 Apple Computer, Inc.
 * Copyright (C) 2006 Michael Emmel mike.emmel@gmail.com
 * Coypright (C) 2006 Nikolas Zimmermann <zimmermann@kde.org>
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

#include "PopupMenuQt.h"

#include "ChromeClientQt.h"
#include <WebCore/FrameView.h>
#include <WebCore/PopupMenuClient.h>

class SelectData : public QWebSelectData {
public:
    SelectData(WebCore::PopupMenuClient*& data) : d(data) { }

    ItemType itemType(int) const override;
    QString itemText(int idx) const override { return QString(d ? d->itemText(idx) : ""_s); }
    QString itemToolTip(int idx) const override { return QString(d ? d->itemToolTip(idx) : ""_s); }
    bool itemIsEnabled(int idx) const override { return d ? d->itemIsEnabled(idx) : false; }
    int itemCount() const override { return d ? d->listSize() : 0; }
    bool itemIsSelected(int idx) const override { return d ? d->itemIsSelected(idx) : false; }
    bool multiple() const override;
    QColor backgroundColor() const override { return d ? QColor(d->menuStyle().backgroundColor()) : QColor(); }
    QColor foregroundColor() const override { return d ? QColor(d->menuStyle().foregroundColor()) : QColor(); }
    QColor itemBackgroundColor(int idx) const override { return d ? QColor(d->itemStyle(idx).backgroundColor()) : QColor(); }
    QColor itemForegroundColor(int idx) const override { return d ? QColor(d->itemStyle(idx).foregroundColor()) : QColor(); }

private:
    WebCore::PopupMenuClient*& d;
};

bool SelectData::multiple() const
{
    if (!d)
        return false;

    return d->multiple();
}

SelectData::ItemType SelectData::itemType(int idx) const
{
    if (!d)
        return SelectData::Option;

    if (d->itemIsSeparator(idx))
        return SelectData::Separator;
    if (d->itemIsLabel(idx))
        return SelectData::Group;
    return SelectData::Option;
}

namespace WebCore {

PopupMenuQt::PopupMenuQt(PopupMenuClient* client, const ChromeClientQt* chromeClient)
    : m_popupClient(client)
    , m_chromeClient(chromeClient)
{
}

PopupMenuQt::~PopupMenuQt()
{
}

void PopupMenuQt::disconnectClient()
{
    m_popupClient = 0;
}

void PopupMenuQt::show(const IntRect& rect, LocalFrameView& view, int index)
{
    if (!m_popupClient)
        return;

    if (!m_popup) {
        m_popup = m_chromeClient->createSelectPopup();
        connect(m_popup.get(), SIGNAL(didHide()), this, SLOT(didHide()));
        connect(m_popup.get(), SIGNAL(selectItem(int, bool, bool)), this, SLOT(selectItem(int, bool, bool)));
    }

    QRect geometry(rect);
    geometry.moveTopLeft(view.contentsToWindow(rect.location()));
    m_popup->setGeometry(geometry);
    m_popup->setFont(m_popupClient->menuStyle().font().syntheticFont());

    m_selectData = std::make_unique<SelectData>(m_popupClient);
    m_popup->show(*m_selectData.get());
}

void PopupMenuQt::didHide()
{
    if (m_popupClient)
        m_popupClient->popupDidHide();
}

void PopupMenuQt::hide()
{
    if (m_popup)
        m_popup->hide();
}

void PopupMenuQt::updateFromElement()
{
    if (m_popupClient)
        m_popupClient->setTextFromItem(m_popupClient->selectedIndex());
}

void PopupMenuQt::selectItem(int index, bool ctrl, bool shift)
{
    if (!m_popupClient)
        return;

    m_popupClient->listBoxSelectItem(index, ctrl, shift);
    return;

    m_popupClient->valueChanged(index);
}

}

#include "moc_PopupMenuQt.cpp"

// vim: ts=4 sw=4 et
