/*
 *  alarmlistview.h  -  widget showing list of outstanding alarms
 *  Program:  kalarm
 *  (C) 2001, 2002 by David Jarvie  software@astrojar.org.uk
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
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 *
 *  As a special exception, permission is given to link this program
 *  with any edition of Qt, and distribute the resulting executable,
 *  without including the source code for Qt in the source distribution.
 */

#ifndef ALARMLISTVIEW_H
#define ALARMLISTVIEW_H

#include "kalarm.h"

#include <qmap.h>
#include <klistview.h>

#include "msgevent.h"
using namespace KCal;


class AlarmListViewItem;
struct AlarmItemData
{
		KAlarmEvent    event;
		QString        messageText;       // message as displayed
		QString        dateTimeText;      // date/time as displayed
		QString        repeatText;        // repeat interval/type as displayed
		int            messageWidth;      // width required to display 'messageText'
};


class AlarmListView : public KListView
{
		Q_OBJECT
	public:
		enum { TIME_COLUMN, REPEAT_COLUMN, COLOUR_COLUMN, MESSAGE_COLUMN };

		AlarmListView(QWidget* parent = 0, const char* name = 0);
		virtual void         clear();
		void                 refresh();
		void                 showExpired(bool show)      { mShowExpired = show; }
		AlarmListViewItem*   addEntry(const KAlarmEvent&, bool setSize = false);
		AlarmListViewItem*   updateEntry(AlarmListViewItem*, const KAlarmEvent& newEvent, bool setSize = false);
		void                 deleteEntry(AlarmListViewItem*, bool setSize = false);
		const KAlarmEvent&   getEntry(AlarmListViewItem* item) const	{ return getData(item)->event; }
		AlarmListViewItem*   getEntry(const QString& eventID);
		const AlarmItemData* getData(AlarmListViewItem*) const;
		bool                 expired(AlarmListViewItem*) const;
		void                 resizeLastColumn();
		int                  itemHeight();
		bool                 drawMessageInColour() const		      { return drawMessageInColour_; }
		void                 setDrawMessageInColour(bool inColour)	{ drawMessageInColour_ = inColour; }
		AlarmListViewItem*   selectedItem() const	{ return (AlarmListViewItem*)KListView::selectedItem(); }
		AlarmListViewItem*   currentItem() const	{ return (AlarmListViewItem*)KListView::currentItem(); }
	signals:
		void                 itemDeleted();
	private:
		typedef QMap<AlarmListViewItem*, AlarmItemData> EntryMap;
		EntryMap             entries;
		int                  lastColumnHeaderWidth_;
		bool                 drawMessageInColour_;
		bool                 mShowExpired;              // true to show expired alarms
};


class AlarmListViewItem : public QListViewItem
{
	public:
		AlarmListViewItem(QListView* parent, const QString&, const QString&);
		virtual void         paintCell(QPainter*, const QColorGroup&, int column, int width, int align);
		AlarmListView*       alarmListView() const              { return (AlarmListView*)listView(); }
	private:
		static QPixmap*      textIcon;
		static QPixmap*      fileIcon;
		static QPixmap*      commandIcon;
		static QPixmap*      emailIcon;
		static int           iconWidth;
};

#endif // ALARMLISTVIEW_H

