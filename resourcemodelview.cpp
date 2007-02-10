/*
 *  resourcemodelview.cpp  -  model/view classes for alarm resource lists
 *  Program:  kalarm
 *  Copyright © 2007 by David Jarvie <software@astrojar.org.uk>
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License along
 *  with this program; if not, write to the Free Software Foundation, Inc.,
 *  51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 */

#include "kalarm.h"

#include <QApplication>
#include <QToolTip>
#include <QMouseEvent>
#include <QKeyEvent>
#include <QHelpEvent>
#include <QTextLayout>
#include <QTextLine>

#include <klocale.h>
#include <kmessagebox.h>
#include <kdebug.h>

#include "preferences.h"
#include "resourcemodelview.moc"


ResourceModel* ResourceModel::mInstance = 0;


ResourceModel* ResourceModel::instance(QObject* parent)
{
	if (!mInstance)
		mInstance = new ResourceModel(parent);
	return mInstance;
}

ResourceModel::ResourceModel(QObject* parent)
	: QAbstractListModel(parent)
{
	refresh();
	AlarmResources* resources = AlarmResources::instance();
	connect(resources, SIGNAL(signalResourceAdded(AlarmResource*)), SLOT(addResource(AlarmResource*)));
	connect(resources, SIGNAL(signalResourceModified(AlarmResource*)), SLOT(updateResource(AlarmResource*)));
	connect(resources, SIGNAL(standardResourceChange(AlarmResource::Type)), SLOT(slotStandardChanged(AlarmResource::Type)));
	connect(resources, SIGNAL(resourceStatusChanged(AlarmResource*, AlarmResources::Change)), SLOT(slotStatusChanged(AlarmResource*, AlarmResources::Change)));
}

int ResourceModel::rowCount(const QModelIndex& parent) const
{
	if (parent.isValid())
		return 0;
	return mResources.count();
}

QModelIndex ResourceModel::index(int row, int column, const QModelIndex& parent) const
{
	if (parent.isValid()  ||  row >= mResources.count())
		return QModelIndex();
	return createIndex(row, column, mResources[row]);
}

QVariant ResourceModel::data(const QModelIndex& index, int role) const
{
	if (!index.isValid())
		return QVariant();
	AlarmResource* resource = static_cast<AlarmResource*>(index.internalPointer());
	if (!resource)
		return QVariant();
	switch (role)
	{
		case Qt::DisplayRole:
			return resource->resourceName();
		case Qt::CheckStateRole:
			return resource->isEnabled() ? Qt::Checked : Qt::Unchecked;
		case Qt::ForegroundRole:
			switch (resource->alarmType())
			{
				case AlarmResource::ACTIVE:    return resource->readOnly() ? Qt::darkGray : Qt::black;
				case AlarmResource::ARCHIVED:  return resource->readOnly() ? Qt::green : Qt::darkGreen;
				case AlarmResource::TEMPLATE:  return resource->readOnly() ? Qt::blue : Qt::darkBlue;
			}
			break;
		case Qt::FontRole:
		{
			if (!resource->isEnabled()  ||  !resource->standardResource())
				break;
			QFont font = mFont;
			font.setBold(true);
			return font;
		}
		case Qt::ToolTipRole:
		{
			QString tipText = resource->resourceName() + '\n';
			tipText += resource->displayLocation(true);
			bool inactive = !resource->isActive();
			if (inactive)
				tipText += '\n' + i18n("Disabled");
			if (resource->readOnly())
				tipText += (inactive ? ", " : "\n") + i18n("Read-only");
			return tipText;
		}
		default:
			break;
	}
	return QVariant();
}

/******************************************************************************
* Set the font to use for all items, or the checked state of one item.
* The font must always be set at initialisation.
*/
bool ResourceModel::setData(const QModelIndex& index, const QVariant& value, int role)
{
	mErrorPrompt.clear();
	if (role == Qt::FontRole)
	{
		// Set the font used in all views.
		// This enables data(index, Qt::FontRole) to return bold when appropriate.
		mFont = value.value<QFont>();
		return true;
	}
	if (role != Qt::CheckStateRole  ||  !index.isValid())
		return false;
	AlarmResource* resource = static_cast<AlarmResource*>(index.internalPointer());
	if (!resource)
		return false;
	Qt::CheckState state = static_cast<Qt::CheckState>(value.toInt());
	bool active = (state == Qt::Checked);
	bool saveChange = false;
	AlarmResources* resources = AlarmResources::instance();
	if (active)
	{
		// Enable the resource.
		// The new setting needs to be written before calling load(), since
		// load completion triggers daemon notification, and the daemon
		// needs to see when it is triggered what the new resource status is.
		resource->setActive(true);     // enable it now so that load() will work
		saveChange = resources->load(resource);
		resource->setActive(false);    // reset so that setEnabled() will work
	}
	else
	{
		// Disable the resource
		saveChange = resource->saveAndClose();   // close resource after it is saved
	}
	if (saveChange)
	{
		// Save the change and notify the alarm daemon
		resource->setEnabled(active);
//		Daemon::reloadResource(resource->identifier());
	}
	emit dataChanged(index, index);
	return true;
}

Qt::ItemFlags ResourceModel::flags(const QModelIndex&) const
{
	return Qt::ItemIsSelectable | Qt::ItemIsEnabled | Qt::ItemIsUserCheckable;
}

/******************************************************************************
* Return the resource referred to by an index.
*/
AlarmResource* ResourceModel::resource(const QModelIndex& index) const
{
	if (!index.isValid())
		return 0;
	return static_cast<AlarmResource*>(index.internalPointer());
}

/******************************************************************************
* Emit a signal that a resource has changed.
*/
void ResourceModel::notifyChange(const QModelIndex& index)
{
	if (index.isValid())
		emit dataChanged(index, index);
}

/******************************************************************************
* Initialise or update the birthday selection list by fetching all birthdays
* from the address book and displaying those which do not already have alarms.
*/
void ResourceModel::refresh()
{
	// This would be better done by a reset(), but the signals are private to QAbstractItemModel
	beginRemoveRows(QModelIndex(), 0, mResources.count() - 1);
	mResources.clear();
	endRemoveRows();
	QList<AlarmResource*> newResources;
	AlarmResourceManager* manager = AlarmResources::instance()->resourceManager();
	for (AlarmResourceManager::Iterator it = manager->begin();  it != manager->end();  ++it)
		newResources += *it;
	beginInsertRows(QModelIndex(), 0, newResources.count() - 1);
	mResources = newResources;
	endInsertRows();
}

/******************************************************************************
* Add the specified resource to the list.
*/
void ResourceModel::addResource(AlarmResource* resource)
{
	int row = mResources.count();
	beginInsertRows(QModelIndex(), row, row);
	mResources += resource;
	endInsertRows();
}

/******************************************************************************
* Called when the resource has been updated , to update the
* active status displayed for the resource item.
*/
void ResourceModel::updateResource(AlarmResource* resource)
{
	int row = mResources.indexOf(resource);
	if (row >= 0)
	{
		QModelIndex ix = index(row, 0, QModelIndex());
		emit dataChanged(ix, ix);
	}
}

/******************************************************************************
* Called when a different resource has been set as the standard resource.
*/
void ResourceModel::slotStandardChanged(AlarmResource::Type type)
{
	for (int row = 0, end = mResources.count();  row < end;  ++row)
	{
		if (mResources[row]->alarmType() == type)
		{
			QModelIndex ix = index(row, 0, QModelIndex());
			emit dataChanged(ix, ix);
		}
	}
}

/******************************************************************************
* Called when a resource status has changed, to update the list.
*/
void ResourceModel::slotStatusChanged(AlarmResource* resource, AlarmResources::Change change)
{
	if (change != AlarmResources::Enabled  &&  change != AlarmResources::ReadOnly)
		return;    // not interested in other types of change
	updateResource(resource);
}


/*=============================================================================
= Class: ResourceFilterModel
= Proxy model for filtering resource lists.
=============================================================================*/

ResourceFilterModel::ResourceFilterModel(QAbstractItemModel* baseModel, QObject* parent)
	: QSortFilterProxyModel(parent)
{
	setSourceModel(baseModel);
}

void ResourceFilterModel::setFilter(AlarmResource::Type type)
{
	if (type != mResourceType)
	{
		mResourceType = type;
		filterChanged();
	}
}

bool ResourceFilterModel::filterAcceptsRow(int sourceRow, const QModelIndex&) const
{
	return static_cast<ResourceModel*>(sourceModel())->resource(sourceModel()->index(sourceRow, 0))->alarmType() == mResourceType;
}

/******************************************************************************
* Return the resource referred to by an index.
*/
AlarmResource* ResourceFilterModel::resource(int row) const
{
	return static_cast<ResourceModel*>(sourceModel())->resource(mapToSource(index(row, 0)));
}

AlarmResource* ResourceFilterModel::resource(const QModelIndex& index) const
{
	return static_cast<ResourceModel*>(sourceModel())->resource(mapToSource(index));
}

/******************************************************************************
* Emit a signal that a resource has changed.
*/
void ResourceFilterModel::notifyChange(int row)
{
	static_cast<ResourceModel*>(sourceModel())->notifyChange(mapToSource(index(row, 0)));
}

void ResourceFilterModel::notifyChange(const QModelIndex& index)
{
	static_cast<ResourceModel*>(sourceModel())->notifyChange(mapToSource(index));
}


/*=============================================================================
= Class: ResourceDelegate
= Model/view delegate for resource list.
=============================================================================*/

/******************************************************************************
* Process a change of state of the checkbox for a resource.
*/
bool ResourceDelegate::editorEvent(QEvent* event, QAbstractItemModel* model, const QStyleOptionViewItem& option, const QModelIndex& index)
{
	if (!(model->flags(index) & Qt::ItemIsEnabled))
		return false;
	if (event->type() == QEvent::MouseButtonRelease
	||  event->type() == QEvent::MouseButtonDblClick)
	{
		const int textMargin = QApplication::style()->pixelMetric(QStyle::PM_FocusFrameHMargin) + 1;
		QRect checkRect = QStyle::alignedRect(option.direction, Qt::AlignLeft | Qt::AlignVCenter,
		                                      check(option, option.rect, Qt::Checked).size(),
		                                      QRect(option.rect.x() + textMargin, option.rect.y(), option.rect.width(), option.rect.height()));
		if (!checkRect.contains(static_cast<QMouseEvent*>(event)->pos()))
			return false;
		if (event->type() == QEvent::MouseButtonDblClick)
			return true;    // ignore double clicks
	}
	else if (event->type() == QEvent::KeyPress)
	{
		if (static_cast<QKeyEvent*>(event)->key() != Qt::Key_Space
		&&  static_cast<QKeyEvent*>(event)->key() != Qt::Key_Select)
			return false;
	}
	else
		return false;

	QVariant value = index.data(Qt::CheckStateRole);
	if (!value.isValid())
		return false;
	Qt::CheckState state = (static_cast<Qt::CheckState>(value.toInt()) == Qt::Checked ? Qt::Unchecked : Qt::Checked);
	if (state == Qt::Unchecked)
	{
		// The resource is to be disabled.
		// Check for eligibility.
		AlarmResource* resource = static_cast<ResourceFilterModel*>(model)->resource(index);
		if (!resource)
			return false;
		if (resource->standardResource())
		{
			// It's the standard resource for its type.
			if (resource->alarmType() == AlarmResource::ACTIVE)
			{
				KMessageBox::sorry(static_cast<QWidget*>(parent()),
				                   i18n("You cannot disable your default active alarm resource."));
				return false;

			}
			if (resource->alarmType() == AlarmResource::ARCHIVED  &&  Preferences::archivedKeepDays())
			{
				// Only allow the archived alarms standard resource to be disabled if
				// we're not saving archived alarms.
				KMessageBox::sorry(static_cast<QWidget*>(parent()),
				                   i18n("You cannot disable your default archived alarm resource "
				                        "while expired alarms are configured to be kept."));
				return false;
			}
			if (KMessageBox::warningContinueCancel(static_cast<QWidget*>(parent()),
			                                       i18n("Do you really want to disable your default resource?"))
			           == KMessageBox::Cancel)
				return false;
		}
	}
	return model->setData(index, state, Qt::CheckStateRole);
}


/*=============================================================================
= Class: ResourceView
= View displaying a list of resources.
=============================================================================*/

void ResourceView::setModel(QAbstractItemModel* model)
{
	model->setData(QModelIndex(), viewOptions().font, Qt::FontRole);
	QListView::setModel(model);
}

/******************************************************************************
* Return the resource for a given row.
*/
AlarmResource* ResourceView::resource(int row) const
{
	return static_cast<ResourceFilterModel*>(model())->resource(row);
}

AlarmResource* ResourceView::resource(const QModelIndex& index) const
{
	return static_cast<ResourceFilterModel*>(model())->resource(index);
}

/******************************************************************************
* Emit a signal that a resource has changed.
*/
void ResourceView::notifyChange(int row) const
{
	static_cast<ResourceFilterModel*>(model())->notifyChange(row);
}

void ResourceView::notifyChange(const QModelIndex& index) const
{
	static_cast<ResourceFilterModel*>(model())->notifyChange(index);
}

/******************************************************************************
* Called when a ToolTip or WhatsThis event occurs.
*/
bool ResourceView::viewportEvent(QEvent* e)
{
	if (e->type() == QEvent::ToolTip  &&  isActiveWindow())
	{
		QHelpEvent* he = static_cast<QHelpEvent*>(e);
		QModelIndex index = indexAt(he->pos());
		QVariant value = model()->data(index, Qt::ToolTipRole);
		if (qVariantCanConvert<QString>(value))
		{
			QString toolTip = value.toString();
			int i = toolTip.indexOf('\n');
			if (i > 0)
			{
				QString name = toolTip.left(i);
				value = model()->data(index, Qt::FontRole);
				QFontMetrics fm(qvariant_cast<QFont>(value).resolve(viewOptions().font));
				int textWidth = fm.boundingRect(name).width() + 1;
				const int margin = QApplication::style()->pixelMetric(QStyle::PM_FocusFrameHMargin) + 1;
				QStyleOptionButton opt;
				opt.QStyleOption::operator=(viewOptions());
				opt.rect = rectForIndex(index);
				int checkWidth = QApplication::style()->subElementRect(QStyle::SE_ViewItemCheckIndicator, &opt).width();
				int left = spacing() + 3*margin + checkWidth;   // left offset of text
				int right = left + textWidth;
				if (left >= horizontalOffset() + spacing()
				&&  right <= horizontalOffset() + width() - spacing() - 2*frameWidth())
				{
					// The whole of the resource name is already displayed,
					// so omit it from the tooltip.
					toolTip = toolTip.mid(i + 1);
				}
			}
			QToolTip::showText(he->globalPos(), toolTip, this);
			return true;
		}
	}
	return QListView::viewportEvent(e);
}
