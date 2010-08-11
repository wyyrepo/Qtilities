/****************************************************************************
**
** Copyright (c) 2009-2010, Jaco Naude
**
** This file is part of Qtilities which is released under the following
** licensing options.
**
** Option 1: Open Source
** Under this license Qtilities is free software: you can
** redistribute it and/or modify it under the terms of the GNU General
** Public License as published by the Free Software Foundation, either
** version 3 of the License, or (at your option) any later version.
**
** Qtilities is distributed in the hope that it will be useful,
** but WITHOUT ANY WARRANTY; without even the implied warranty of
** MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
** GNU General Public License for more details.
**
** You should have received a copy of the GNU General Public License
** along with Qtilities. If not, see http://www.gnu.org/licenses/.
**
** Option 2: Commercial
** Alternatively, this library is also released under a commercial license
** that allows the development of closed source proprietary applications
** without restrictions on licensing. For more information on this option,
** please see the project website's licensing page:
** http://www.qtilities.org/licensing.html
**
** If you are unsure which license is appropriate for your use, please
** contact support@qtilities.org.
**
****************************************************************************/

#include "ObserverTreeItem.h"
#include "Observer.h"

#include <QStringList>
#include <QtDebug>

Qtilities::CoreGui::ObserverTreeItem::ObserverTreeItem(QObject* object, ObserverTreeItem *parent, const QVector<QVariant> &data, TreeItemType item_type)
{
    parentItem = parent;
    itemData = data;
    obj = object;
    type = item_type;
    contained_observer_ref = 0;

    if (obj) {
        setObjectName(obj->objectName());
    } else {
        setObjectName("Root item prior to tree construction");
    }
}

Qtilities::CoreGui::ObserverTreeItem::~ObserverTreeItem()
{
    //qDebug() << QString("Observer Tree Item (%1) destruction started.").arg(objectName());
    int count = childItems.count();
    for (int i = count-1; i >= 0; i--) {
        if (childItems.at(i)) {
            //qDebug() << QString("Observer Tree Item (%1) is deleting its children: %2.").arg(objectName()).arg(childItems.at(0)->objectName());
            delete childItems.at(i);
        }
    }
    if (obj)
        obj->disconnect(this);
    if (contained_observer_ref)
        contained_observer_ref->disconnect(this);
    //qDebug() << QString("Observer Tree Item (%1) destruction finished.").arg(objectName());
}

void Qtilities::CoreGui::ObserverTreeItem::appendChild(ObserverTreeItem *child_item)
{
    childItems.append(child_item);
}

Qtilities::CoreGui::ObserverTreeItem* Qtilities::CoreGui::ObserverTreeItem::child(int row)
{
    return childItems.value(row);
}

int Qtilities::CoreGui::ObserverTreeItem::childCount() const
{
    return childItems.count();
}

int Qtilities::CoreGui::ObserverTreeItem::columnCount() const
{
    return itemData.count();
}

QVariant Qtilities::CoreGui::ObserverTreeItem::data(int column) const
{
    if (itemData.count() == 0) {
        if (obj)
            return objectName();
        else
            return QObject::tr("Contents Tree"); // Will be used for headers
    } else {
        if (obj)
            return itemData.value(column);
        else
            return QObject::tr("Contents Tree"); // Will be used for headers
    }

    return "";
}

Qtilities::CoreGui::ObserverTreeItem* Qtilities::CoreGui::ObserverTreeItem::parent()
{
    return parentItem;
}

int Qtilities::CoreGui::ObserverTreeItem::row() const
{
    if (parentItem)
        return parentItem->childItems.indexOf(const_cast<ObserverTreeItem*>(this));

    return 0;
}