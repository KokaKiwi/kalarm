/*
 *  undo.cpp  -  undo/redo facility
 *  Program:  kalarm
 *  Copyright © 2005-2006 by David Jarvie <software@astrojar.org.uk>
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

#include <qobject.h>

#include <kapplication.h>
#include <klocale.h>
#include <kmessagebox.h>
#include <kdebug.h>

#include "alarmcalendar.h"
#include "alarmevent.h"
#include "alarmtext.h"
#include "functions.h"
#include "undo.moc"

static int maxCount = 12;


class UndoItem
{
	public:
		enum Operation { ADD, EDIT, DELETE, REACTIVATE, DEACTIVATE, MULTI };
		UndoItem();           // needed by QList
		virtual ~UndoItem();
		virtual Operation operation() const = 0;
		virtual QString   actionText() const    { return !mName.isEmpty() ? mName : defaultActionText(); }
		virtual QString   defaultActionText() const = 0;
		virtual QString   description() const   { return QString(); }
		virtual QString   eventID() const       { return QString(); }
		virtual QString   oldEventID() const    { return QString(); }
		virtual QString   newEventID() const    { return QString(); }
		int               id() const            { return mId; }
		Undo::Type        type() const          { return mType; }
		void              setType(Undo::Type t) { mType = t; }
		KAEvent::Status   calendar() const      { return mCalendar; }
		virtual void      setCalendar(KAEvent::Status s) { mCalendar = s; }
		virtual UndoItem* restore() = 0;
		virtual bool      deleteID(const QString& /*id*/)  { return false; }

		enum Error   { ERR_NONE, ERR_PROG, ERR_NOT_FOUND, ERR_CREATE, ERR_TEMPLATE, ERR_EXPIRED };
		enum Warning { WARN_NONE, WARN_KORG_ADD, WARN_KORG_MODIFY, WARN_KORG_DELETE };
		static int        mLastId;
		static Error      mRestoreError;         // error code valid only if restore() returns 0
		static Warning    mRestoreWarning;       // warning code set by restore()
		static int        mRestoreWarningCount;  // item count for mRestoreWarning (to allow i18n messages to work correctly)

	protected:
		UndoItem(Undo::Type, const QString& name = QString());
		static QString    addDeleteActionText(KAEvent::Status, bool add);
		QString           description(const KAEvent&) const;
		void              replaceWith(UndoItem* item)   { Undo::replace(this, item); }

		QString           mName;      // specified action name (overrides default)
		int               mId;        // unique identifier (only for mType = UNDO, REDO)
		Undo::Type        mType;      // which list (if any) the object is in
		KAEvent::Status   mCalendar;
};

class UndoMultiBase : public UndoItem
{
	public:
		UndoMultiBase(Undo::Type t, const QString& name) : UndoItem(t, name) { }
		UndoMultiBase(Undo::Type t, Undo::List& undos, const QString& name) : UndoItem(t, name), mUndos(undos) { }
		~UndoMultiBase();
		const Undo::List& undos() const         { return mUndos; }
	protected:
		Undo::List  mUndos;    // this list must always have >= 2 entries
};

template <class T> class UndoMulti : public UndoMultiBase
{
	public:
		UndoMulti(Undo::Type, const QList<KAEvent>&, const QString& name);
		UndoMulti(Undo::Type t, Undo::List& undos, const QString& name)  : UndoMultiBase(t, undos, name) { }
		virtual Operation operation() const     { return MULTI; }
		virtual UndoItem* restore();
		virtual bool      deleteID(const QString& id);
		virtual UndoItem* createRedo(Undo::List&) = 0;
};

class UndoAdd : public UndoItem
{
	public:
		UndoAdd(Undo::Type, const KAEvent&, const QString& name = QString());
		UndoAdd(Undo::Type, const KAEvent&, const QString& name, KAEvent::Status);
		virtual Operation operation() const     { return ADD; }
		virtual QString   defaultActionText() const;
		virtual QString   description() const   { return mDescription; }
		virtual QString   eventID() const       { return mEventID; }
		virtual QString   newEventID() const    { return mEventID; }
		virtual UndoItem* restore()             { return doRestore(); }
	protected:
		UndoItem*         doRestore(bool setArchive = false);
		virtual UndoItem* createRedo(const KAEvent&);
	private:
		QString  mEventID;
		QString  mDescription;
};

class UndoEdit : public UndoItem
{
	public:
		UndoEdit(Undo::Type, const KAEvent& oldEvent, const QString& newEventID, const QString& description);
		~UndoEdit();
		virtual Operation operation() const     { return EDIT; }
		virtual QString   defaultActionText() const;
		virtual QString   description() const   { return mDescription; }
		virtual QString   eventID() const       { return mNewEventID; }
		virtual QString   oldEventID() const    { return mOldEvent->id(); }
		virtual QString   newEventID() const    { return mNewEventID; }
		virtual UndoItem* restore();
	private:
		KAEvent*  mOldEvent;
		QString   mNewEventID;
		QString   mDescription;
};

class UndoDelete : public UndoItem
{
	public:
		UndoDelete(Undo::Type, const KAEvent&, const QString& name = QString());
		~UndoDelete();
		virtual Operation operation() const     { return DELETE; }
		virtual QString   defaultActionText() const;
		virtual QString   description() const   { return UndoItem::description(*mEvent); }
		virtual QString   eventID() const       { return mEvent->id(); }
		virtual QString   oldEventID() const    { return mEvent->id(); }
		virtual UndoItem* restore();
		KAEvent*  event() const                 { return mEvent; }
	protected:
		virtual UndoItem* createRedo(const KAEvent&);
	private:
		KAEvent*  mEvent;
};

class UndoReactivate : public UndoAdd
{
	public:
		UndoReactivate(Undo::Type t, const KAEvent& e, const QString& name = QString())
		         : UndoAdd(t, e, name, KAEvent::ACTIVE) { }
		virtual Operation operation() const     { return REACTIVATE; }
		virtual QString   defaultActionText() const;
		virtual UndoItem* restore();
	protected:
		virtual UndoItem* createRedo(const KAEvent&);
};

class UndoDeactivate : public UndoDelete
{
	public:
		UndoDeactivate(Undo::Type t, const KAEvent& e, const QString& name = QString())  : UndoDelete(t, e, name) { }
		virtual Operation operation() const     { return DEACTIVATE; }
		virtual QString   defaultActionText() const;
		virtual UndoItem* restore();
	protected:
		virtual UndoItem* createRedo(const KAEvent&);
};

class UndoAdds : public UndoMulti<UndoAdd>
{
	public:
		UndoAdds(Undo::Type t, const QList<KAEvent>& events, const QString& name = QString())
		                  : UndoMulti<UndoAdd>(t, events, name) { }   // UNDO only
		UndoAdds(Undo::Type t, Undo::List& undos, const QString& name)
		                  : UndoMulti<UndoAdd>(t, undos, name) { }
		virtual QString   defaultActionText() const;
		virtual UndoItem* createRedo(Undo::List&);
};

class UndoDeletes : public UndoMulti<UndoDelete>
{
	public:
		UndoDeletes(Undo::Type t, const QList<KAEvent>& events, const QString& name = QString())
		                  : UndoMulti<UndoDelete>(t, events, name) { }   // UNDO only
		UndoDeletes(Undo::Type t, Undo::List& undos, const QString& name)
		                  : UndoMulti<UndoDelete>(t, undos, name) { }
		virtual QString   defaultActionText() const;
		virtual UndoItem* createRedo(Undo::List&);
};

class UndoReactivates : public UndoMulti<UndoReactivate>
{
	public:
		UndoReactivates(Undo::Type t, const QList<KAEvent>& events, const QString& name = QString())
		                  : UndoMulti<UndoReactivate>(t, events, name) { }   // UNDO only
		UndoReactivates(Undo::Type t, Undo::List& undos, const QString& name)
		                  : UndoMulti<UndoReactivate>(t, undos, name) { }
		virtual QString   defaultActionText() const;
		virtual UndoItem* createRedo(Undo::List&);
};

Undo*       Undo::mInstance = 0;
Undo::List  Undo::mUndoList;
Undo::List  Undo::mRedoList;


/******************************************************************************
*  Create the one and only instance of the Undo class.
*/
Undo* Undo::instance()
{
	if (!mInstance)
		mInstance = new Undo(kapp);
	return mInstance;
}

/******************************************************************************
*  Clear the lists of undo and redo items.
*/
void Undo::clear()
{
	if (!mUndoList.isEmpty()  ||  !mRedoList.isEmpty())
	{
		mInstance->blockSignals(true);
		while (mUndoList.count())
			delete mUndoList.first();    // N.B. 'delete' removes the object from the list
		while (mRedoList.count())
			delete mRedoList.first();    // N.B. 'delete' removes the object from the list
		mInstance->blockSignals(false);
		emitChanged();
	}
}

/******************************************************************************
*  Create an undo item and add it to the list of undos.
*  N.B. The base class constructor adds the object to the undo list.
*/
void Undo::saveAdd(const KAEvent& event, const QString& name)
{
	new UndoAdd(UNDO, event, name);
	emitChanged();
}

void Undo::saveAdds(const QList<KAEvent>& events, const QString& name)
{
	int count = events.count();
	if (count == 1)
		saveAdd(events[0], name);
	else if (count > 1)
	{
		new UndoAdds(UNDO, events, name);
		emitChanged();
	}
}

void Undo::saveEdit(const KAEvent& oldEvent, const KAEvent& newEvent)
{
	new UndoEdit(UNDO, oldEvent, newEvent.id(), AlarmText::summary(newEvent));
	removeRedos(oldEvent.id());    // remove any redos which are made invalid by this edit
	emitChanged();
}

void Undo::saveDelete(const KAEvent& event, const QString& name)
{
	new UndoDelete(UNDO, event, name);
	removeRedos(event.id());    // remove any redos which are made invalid by this deletion
	emitChanged();
}

void Undo::saveDeletes(const QList<KAEvent>& events, const QString& name)
{
	int count = events.count();
	if (count == 1)
		saveDelete(events[0], name);
	else if (count > 1)
	{
		new UndoDeletes(UNDO, events, name);
		for (int i = 0, end = events.count();  i < end;  ++i)
			removeRedos(events[i].id());    // remove any redos which are made invalid by these deletions
		emitChanged();
	}
}

void Undo::saveReactivate(const KAEvent& event, const QString& name)
{
	new UndoReactivate(UNDO, event, name);
	emitChanged();
}

void Undo::saveReactivates(const QList<KAEvent>& events, const QString& name)
{
	int count = events.count();
	if (count == 1)
		saveReactivate(events[0], name);
	else if (count > 1)
	{
		new UndoReactivates(UNDO, events, name);
		emitChanged();
	}
}

/******************************************************************************
*  Remove any redos which are made invalid by a new undo.
*/
void Undo::removeRedos(const QString& eventID)
{
	QString id = eventID;
	for (int i = 0, end = mRedoList.count();  i < end;  )
	{
		UndoItem* item = mRedoList[i];
//kDebug(5950)<<"removeRedos(): "<<item->eventID()<<" (looking for "<<id<<")"<<endl;
		if (item->operation() == UndoItem::MULTI)
		{
			if (item->deleteID(id))
				delete item;   // the old multi-redo was replaced with a new single redo
#warning Check for correct index after deletion
			++i;
		}
		else if (item->eventID() == id)
		{
			if (item->operation() == UndoItem::EDIT)
				id = item->oldEventID();   // continue looking for its post-edit ID
			item->setType(NONE);    // prevent the destructor removing it from the list
			delete item;
			mRedoList.removeAt(i);
		}
		else
			++i;
	}
}

/******************************************************************************
*  Undo or redo a specified item.
*  Reply = true if success, or if the item no longer exists.
*/
bool Undo::undo(int i, Undo::Type type, QWidget* parent, const QString& action)
{
	UndoItem::mRestoreError   = UndoItem::ERR_NONE;
	UndoItem::mRestoreWarning = UndoItem::WARN_NONE;
	UndoItem::mRestoreWarningCount = 0;
	List& list = (type == UNDO) ? mUndoList : mRedoList;
	if (i < list.count()  &&  list[i]->type() == type)
	{
		list[i]->restore();
		delete list[i];    // N.B. 'delete' removes the object from its list
		emitChanged();
	}

	QString err;
	switch (UndoItem::mRestoreError)
	{
		case UndoItem::ERR_NONE:
		{
			KAlarm::UpdateError errcode;
			switch (UndoItem::mRestoreWarning)
			{
				case UndoItem::WARN_KORG_ADD:     errcode = KAlarm::ERR_ADD;  break;
				case UndoItem::WARN_KORG_MODIFY:  errcode = KAlarm::ERR_MODIFY;  break;
				case UndoItem::WARN_KORG_DELETE:  errcode = KAlarm::ERR_DELETE;  break;
				case UndoItem::WARN_NONE:
				default:
					return true;
			}
			KAlarm::displayKOrgUpdateError(parent, errcode, UndoItem::mRestoreWarningCount);
			return true;
		}
		case UndoItem::ERR_NOT_FOUND:  err = i18n("Alarm not found");  break;
		case UndoItem::ERR_CREATE:     err = i18n("Error recreating alarm");  break;
		case UndoItem::ERR_TEMPLATE:   err = i18n("Error recreating alarm template");  break;
		case UndoItem::ERR_EXPIRED:    err = i18n("Cannot reactivate expired alarm");  break;
		case UndoItem::ERR_PROG:       err = i18n("Program error");  break;
		default:                       err = i18n("Unknown error");  break;
	}
	KMessageBox::sorry(parent, i18nc("Undo-action: message", "%1: %2", action, err));
	return false;
}

/******************************************************************************
*  Add an undo item to the start of one of the lists.
*/
void Undo::add(UndoItem* item, bool undo)
{
	if (item)
	{
		// Limit the number of items stored
		int undoCount = mUndoList.count();
		int redoCount = mRedoList.count();
		if (undoCount + redoCount >= maxCount - 1)
		{
			if (undoCount)
				mUndoList.pop_back();
			else
				mRedoList.pop_back();
		}

		// Append the new item
		List* list = undo ? &mUndoList : &mRedoList;
		list->prepend(item);
	}
}

/******************************************************************************
*  Remove an undo item from one of the lists.
*/
void Undo::remove(UndoItem* item, bool undo)
{
	List* list = undo ? &mUndoList : &mRedoList;
	if (!list->isEmpty())
		list->removeAt(list->indexOf(item));
}

/******************************************************************************
*  Replace an undo item in one of the lists.
*/
void Undo::replace(UndoItem* old, UndoItem* New)
{
	Type type = old->type();
	List* list = (type == UNDO) ? &mUndoList : (type == REDO) ? &mRedoList : 0;
	if (!list)
		return;
	int i = list->indexOf(old);
	if (i >= 0)
	{
		New->setType(type);    // ensure the item points to the correct list
		(*list)[i] = New;
		old->setType(NONE);    // mark the old item as no longer being in a list
	}
}

/******************************************************************************
*  Return the action description of the latest undo/redo item.
*/
QString Undo::actionText(Undo::Type type)
{
	List* list = (type == UNDO) ? &mUndoList : (type == REDO) ? &mRedoList : 0;
	return (list && !list->isEmpty()) ? (*list)[0]->actionText() : QString();
}

/******************************************************************************
*  Return the action description of the undo/redo item with the specified ID.
*/
QString Undo::actionText(Undo::Type type, int id)
{
	UndoItem* undo = getItem(id, type);
	return undo ? undo->actionText() : QString();
}

/******************************************************************************
*  Return the alarm description of the undo/redo item with the specified ID.
*/
QString Undo::description(Undo::Type type, int id)
{
	UndoItem* undo = getItem(id, type);
	return undo ? undo->description() : QString();
}

/******************************************************************************
*  Return the descriptions of all undo or redo items, in order latest first.
*  For alarms which have undergone more than one change, only the first one is
*  listed, to force dependent undos to be executed in their correct order.
*  If 'ids' is non-null, also returns a list of their corresponding IDs.
*/
QList<int> Undo::ids(Undo::Type type)
{
	QList<int> ids;
	QStringList ignoreIDs;
//int n=0;
	List* list = (type == UNDO) ? &mUndoList : (type == REDO) ? &mRedoList : 0;
	if (!list)
		return ids;
	for (int i = 0, end = list->count();  i < end;  ++i)
	{
		// Check whether this item should be ignored because it is a
		// deendent undo. If not, add this item's ID to the ignore list.
		UndoItem* item = (*list)[i];
		bool omit = false;
		if (item->operation() == UndoItem::MULTI)
		{
			// If any item in a multi-undo is disqualified, omit the whole multi-undo
			QStringList newIDs;
			const Undo::List& undos = ((UndoMultiBase*)item)->undos();
			for (int u = 0, uend = undos.count();  u  < uend;  ++u)
			{
				QString evid = undos[u]->eventID();
				if (ignoreIDs.contains(evid))
					omit = true;
				else if (omit)
					ignoreIDs.append(evid);
				else
					newIDs.append(evid);
			}
			if (omit)
			{
				for (QStringList::ConstIterator i = newIDs.begin();  i != newIDs.end();  ++i)
					ignoreIDs.append(*i);
			}
		}
		else
		{
			omit = ignoreIDs.contains(item->eventID());
			if (!omit)
				ignoreIDs.append(item->eventID());
			if (item->operation() == UndoItem::EDIT)
				ignoreIDs.append(item->oldEventID());   // continue looking for its post-edit ID
		}
		if (!omit)
			ids.append(item->id());
//else kDebug(5950)<<"Undo::ids(): omit "<<item->actionText()<<": "<<item->description()<<endl;
	}
//kDebug(5950)<<"Undo::ids(): "<<n<<" -> "<<ids.count()<<endl;
	return ids;
}

/******************************************************************************
*  Emit the appropriate 'changed' signal.
*/
void Undo::emitChanged()
{
	if (mInstance)
		mInstance->emitChanged(actionText(UNDO), actionText(REDO));
}

/******************************************************************************
*  Return the item with the specified ID.
*/
UndoItem* Undo::getItem(int id, Undo::Type type)
{
	List* list = (type == UNDO) ? &mUndoList : (type == REDO) ? &mRedoList : 0;
	if (list)
	{
		for (int i = 0, end = list->count();  i < end;  ++i)
		{
			if ((*list)[i]->id() == id)
				return (*list)[i];
		}
	}
	return 0;
}

/******************************************************************************
*  Find an item with the specified ID.
*/
int Undo::findItem(int id, Undo::Type type)
{
	List& list = (type == UNDO) ? mUndoList : mRedoList;
	int i = 0;
	for (int end = list.count();  i < end;  ++i)
	{
		if (list[i]->id() == id)
			break;
	}
	return i;
}


/*=============================================================================
=  Class: UndoItem
=  A single undo action.
=============================================================================*/
int               UndoItem::mLastId = 0;
UndoItem::Error   UndoItem::mRestoreError;
UndoItem::Warning UndoItem::mRestoreWarning;
int               UndoItem::mRestoreWarningCount;

/******************************************************************************
*  Constructor.
*  Optionally appends the undo to the list of undos.
*/
UndoItem::UndoItem(Undo::Type type, const QString& name)
	: mName(name),
	  mId(0),
	  mType(type)
{
	if (type != Undo::NONE)
	{
		mId = ++mLastId;
		if (mId < 0)
			mId = mLastId = 1;    // wrap round if we reach a negative number
		Undo::add(this, (mType == Undo::UNDO));
	}
}

/******************************************************************************
*  Destructor.
*  Removes the undo from the list (if it's in the list).
*/
UndoItem::~UndoItem()
{
	if (mType != Undo::NONE)
		Undo::remove(this, (mType == Undo::UNDO));
}

/******************************************************************************
*  Return the description of an event.
*/
QString UndoItem::description(const KAEvent& event) const
{
	return (mCalendar == KAEvent::TEMPLATE) ? event.templateName() : AlarmText::summary(event);
}

/******************************************************************************
*  Return the action description of an add or delete Undo/Redo item for displaying.
*/
QString UndoItem::addDeleteActionText(KAEvent::Status calendar, bool add)
{
	switch (calendar)
	{
		case KAEvent::ACTIVE:
			if (add)
				return i18nc("Action to create a new alarm", "New alarm");
			else
				return i18nc("Action to delete an alarm", "Delete alarm");
		case KAEvent::TEMPLATE:
			if (add)
				return i18nc("Action to create a new alarm template", "New template");
			else
				return i18nc("Action to delete an alarm template", "Delete template");
		case KAEvent::EXPIRED:
			return i18n("Delete expired alarm");
		default:
			break;
	}
	return QString();
}


/*=============================================================================
=  Class: UndoMultiBase
=  Undo item for multiple alarms.
=============================================================================*/

template <class T>
UndoMulti<T>::UndoMulti(Undo::Type type, const QList<KAEvent>& events, const QString& name)
	: UndoMultiBase(type, name)    // UNDO only
{
	for (int i = 0, end = events.count();  i < end;  ++i)
		mUndos.append(new T(Undo::NONE, events[i]));
}

UndoMultiBase::~UndoMultiBase()
{
	for (int i = 0, end = mUndos.count();  i < end;  ++i)
		delete mUndos[i];
}

/******************************************************************************
*  Undo the item, i.e. restore multiple alarms which were deleted (or delete
*  alarms which were restored).
*  Create a redo item to delete (or restore) the alarms again.
*  Reply = redo item.
*/
template <class T>
UndoItem* UndoMulti<T>::restore()
{
	Undo::List newUndos;
	for (int i = 0, end = mUndos.count();  i < end;  ++i)
	{
		UndoItem* undo = mUndos[i]->restore();
		if (undo)
			newUndos.append(undo);
	}
	if (newUndos.isEmpty())
		return 0;

	// Create a redo item to delete the alarm again
	return createRedo(newUndos);
}

/******************************************************************************
*  If one of the multiple items has the specified ID, delete it.
*  If an item is deleted and there is only one item left, the UndoMulti
*  instance is removed from its list and replaced by the remaining UndoItem instead.
*  Reply = true if this instance was replaced. The caller must delete it.
*        = false otherwise.
*/
template <class T>
bool UndoMulti<T>::deleteID(const QString& id)
{
	for (int i = 0, end = mUndos.count();  i < end;  ++i)
	{
		UndoItem* item = mUndos[i];
		if (item->eventID() == id)
		{
			// Found a matching entry - remove it
			mUndos.removeAt(i);
			if (mUndos.count() == 1)
			{
				// There is only one entry left after removal.
				// Replace 'this' multi instance with the remaining single entry.
				replaceWith(item);
				return true;
			}
			else
			{
				delete item;
				return false;
			}
		}
	}
	return false;
}


/*=============================================================================
=  Class: UndoAdd
=  Undo item for alarm creation.
=============================================================================*/

UndoAdd::UndoAdd(Undo::Type type, const KAEvent& event, const QString& name)
	: UndoItem(type, name),
	  mEventID(event.id())
{
	setCalendar(event.category());
	mDescription = UndoItem::description(event);    // calendar must be set before calling this
}

UndoAdd::UndoAdd(Undo::Type type, const KAEvent& event, const QString& name, KAEvent::Status cal)
	: UndoItem(type, name),
	  mEventID(KAEvent::uid(event.id(), cal))
{
	setCalendar(cal);
	mDescription = UndoItem::description(event);    // calendar must be set before calling this
}

/******************************************************************************
*  Undo the item, i.e. delete the alarm which was added.
*  Create a redo item to add the alarm back again.
*  Reply = redo item.
*/
UndoItem* UndoAdd::doRestore(bool setArchive)
{
	// Retrieve the current state of the alarm
	kDebug(5950) << "UndoAdd::doRestore(" << mEventID << ")\n";
	const KCal::Event* kcalEvent = AlarmCalendar::getEvent(mEventID);
	if (!kcalEvent)
	{
		mRestoreError = ERR_NOT_FOUND;    // alarm is no longer in calendar
		return 0;
	}
	KAEvent event(*kcalEvent); 

	// Create a redo item to recreate the alarm.
	// Do it now, since 'event' gets modified by KAlarm::deleteEvent()
	UndoItem* undo = createRedo(event);

	switch (calendar())
	{
		case KAEvent::ACTIVE:
			if (setArchive)
				event.setArchive();
			// Archive it if it has already triggered
			switch (KAlarm::deleteEvent(event, true))
			{
				case KAlarm::UPDATE_ERROR:
				case KAlarm::UPDATE_FAILED:
				case KAlarm::SAVE_FAILED:
					mRestoreError = ERR_CREATE;
					break;
				case KAlarm::UPDATE_KORG_ERR:
					mRestoreWarning = WARN_KORG_DELETE;
					++mRestoreWarningCount;
					break;
				default:
					break;
			}
			break;
		case KAEvent::TEMPLATE:
			if (KAlarm::deleteTemplate(event.id()) != KAlarm::UPDATE_OK)
				mRestoreError = ERR_TEMPLATE;
			break;
		case KAEvent::EXPIRED:    // redoing the deletion of an expired alarm
			KAlarm::deleteEvent(event);
			break;
		default:
			delete undo;
			mRestoreError = ERR_PROG;
			return 0;
	}
	return undo;
}

/******************************************************************************
*  Create a redo item to add the alarm back again.
*/
UndoItem* UndoAdd::createRedo(const KAEvent& event)
{
	Undo::Type t = (type() == Undo::UNDO) ? Undo::REDO : (type() == Undo::REDO) ? Undo::UNDO : Undo::NONE;
	return new UndoDelete(t, event, mName);
}

/******************************************************************************
*  Return the action description of the Undo item for displaying.
*/
QString UndoAdd::defaultActionText() const
{
	return addDeleteActionText(calendar(), (type() == Undo::UNDO));
}


/*=============================================================================
=  Class: UndoAdds
=  Undo item for multiple alarm creation.
=============================================================================*/

/******************************************************************************
*  Create a redo item to add the alarms again.
*/
UndoItem* UndoAdds::createRedo(Undo::List& undos)
{
	Undo::Type t = (type() == Undo::UNDO) ? Undo::REDO : (type() == Undo::REDO) ? Undo::UNDO : Undo::NONE;
	return new UndoAdds(t, undos, mName);
}

/******************************************************************************
*  Return the action description of the Undo item for displaying.
*/
QString UndoAdds::defaultActionText() const
{
	return i18n("Create multiple alarms");
}


/*=============================================================================
=  Class: UndoEdit
=  Undo item for alarm edit.
=============================================================================*/

UndoEdit::UndoEdit(Undo::Type type, const KAEvent& oldEvent, const QString& newEventID, const QString& description)
	: UndoItem(type),
	  mOldEvent(new KAEvent(oldEvent)),
	  mNewEventID(newEventID),
	  mDescription(description)
{
	setCalendar(KAEvent::uidStatus(mNewEventID));
}

UndoEdit::~UndoEdit()
{
	delete mOldEvent;
}

/******************************************************************************
*  Undo the item, i.e. undo an edit to a previously existing alarm.
*  Create a redo item to reapply the edit.
*  Reply = redo item.
*/
UndoItem* UndoEdit::restore()
{
	kDebug(5950) << "UndoEdit::restore(" << mNewEventID << ")\n";
	// Retrieve the current state of the alarm
	const KCal::Event* kcalEvent = AlarmCalendar::getEvent(mNewEventID);
	if (!kcalEvent)
	{
		mRestoreError = ERR_NOT_FOUND;    // alarm is no longer in calendar
		return 0;
	}
	KAEvent newEvent(*kcalEvent); 

	// Create a redo item to restore the edit
	Undo::Type t = (type() == Undo::UNDO) ? Undo::REDO : (type() == Undo::REDO) ? Undo::UNDO : Undo::NONE;
	UndoItem* undo = new UndoEdit(t, newEvent, mOldEvent->id(), mDescription);

	switch (calendar())
	{
		case KAEvent::ACTIVE:
			switch (KAlarm::modifyEvent(newEvent, *mOldEvent, 0))
			{
				case KAlarm::UPDATE_ERROR:
				case KAlarm::UPDATE_FAILED:
				case KAlarm::SAVE_FAILED:
					mRestoreError = ERR_CREATE;
					break;
				case KAlarm::UPDATE_KORG_ERR:
					mRestoreWarning = WARN_KORG_MODIFY;
					++mRestoreWarningCount;
					break;
				default:
					break;
			}
			break;
		case KAEvent::TEMPLATE:
			if (KAlarm::updateTemplate(*mOldEvent, 0) != KAlarm::UPDATE_OK)
				mRestoreError = ERR_TEMPLATE;
			break;
		case KAEvent::EXPIRED:    // editing of expired events is not allowed
		default:
			delete undo;
			mRestoreError = ERR_PROG;
			return 0;
	}
	return undo;
}

/******************************************************************************
*  Return the action description of the Undo item for displaying.
*/
QString UndoEdit::defaultActionText() const
{
	switch (calendar())
	{
		case KAEvent::ACTIVE:
			return i18nc("Action to edit an alarm", "Edit alarm");
		case KAEvent::TEMPLATE:
			return i18nc("Action to edit an alarm template", "Edit template");
		default:
			break;
	}
	return QString();
}


/*=============================================================================
=  Class: UndoDelete
=  Undo item for alarm deletion.
=============================================================================*/

UndoDelete::UndoDelete(Undo::Type type, const KAEvent& event, const QString& name)
	: UndoItem(type, name),
	  mEvent(new KAEvent(event))
{
	setCalendar(mEvent->category());
}

UndoDelete::~UndoDelete()
{
	delete mEvent;
}

/******************************************************************************
*  Undo the item, i.e. restore an alarm which was deleted.
*  Create a redo item to delete the alarm again.
*  Reply = redo item.
*/
UndoItem* UndoDelete::restore()
{
	kDebug(5950) << "UndoDelete::restore(" << mEvent->id() << ")\n";
	// Restore the original event
	switch (calendar())
	{
		case KAEvent::ACTIVE:
			if (mEvent->toBeArchived())
			{
				// It was archived when it was deleted
				mEvent->setUid(KAEvent::EXPIRED);
				switch (KAlarm::reactivateEvent(*mEvent, 0))
				{
					case KAlarm::UPDATE_KORG_ERR:
						mRestoreWarning = WARN_KORG_ADD;
						++mRestoreWarningCount;
						break;
					case KAlarm::UPDATE_ERROR:
					case KAlarm::UPDATE_FAILED:
					case KAlarm::SAVE_FAILED:
						mRestoreError = ERR_EXPIRED;
						return 0;
					case KAlarm::UPDATE_OK:
						break;
				}
			}
			else
			{
				switch (KAlarm::addEvent(*mEvent, 0, true))
				{
					case KAlarm::UPDATE_KORG_ERR:
						mRestoreWarning = WARN_KORG_ADD;
						++mRestoreWarningCount;
						break;
					case KAlarm::UPDATE_ERROR:
					case KAlarm::UPDATE_FAILED:
					case KAlarm::SAVE_FAILED:
						mRestoreError = ERR_CREATE;
						return 0;
					case KAlarm::UPDATE_OK:
						break;
				}
			}
			break;
		case KAEvent::TEMPLATE:
			if (KAlarm::addTemplate(*mEvent, 0) != KAlarm::UPDATE_OK)
			{
				mRestoreError = ERR_CREATE;
				return 0;
			}
			break;
		case KAEvent::EXPIRED:
			if (!KAlarm::addExpiredEvent(*mEvent))
			{
				mRestoreError = ERR_CREATE;
				return 0;
			}
			break;
		default:
			mRestoreError = ERR_PROG;
			return 0;
	}

	// Create a redo item to delete the alarm again
	return createRedo(*mEvent);
}

/******************************************************************************
*  Create a redo item to archive the alarm again.
*/
UndoItem* UndoDelete::createRedo(const KAEvent& event)
{
	Undo::Type t = (type() == Undo::UNDO) ? Undo::REDO : (type() == Undo::REDO) ? Undo::UNDO : Undo::NONE;
	return new UndoAdd(t, event, mName);
}

/******************************************************************************
*  Return the action description of the Undo item for displaying.
*/
QString UndoDelete::defaultActionText() const
{
	return addDeleteActionText(calendar(), (type() == Undo::REDO));
}


/*=============================================================================
=  Class: UndoDeletes
=  Undo item for multiple alarm deletion.
=============================================================================*/

/******************************************************************************
*  Create a redo item to delete the alarms again.
*/
UndoItem* UndoDeletes::createRedo(Undo::List& undos)
{
	Undo::Type t = (type() == Undo::UNDO) ? Undo::REDO : (type() == Undo::REDO) ? Undo::UNDO : Undo::NONE;
	return new UndoDeletes(t, undos, mName);
}

/******************************************************************************
*  Return the action description of the Undo item for displaying.
*/
QString UndoDeletes::defaultActionText() const
{
	if (mUndos.isEmpty())
		return QString();
	for (int i = 0, end = mUndos.count();  i < end;  ++i)
	{
		switch (mUndos[i]->calendar())
		{
			case KAEvent::ACTIVE:
				return i18n("Delete multiple alarms");
			case KAEvent::TEMPLATE:
				return i18n("Delete multiple templates");
			case KAEvent::EXPIRED:
				break;    // check if they are ALL expired
			default:
				return QString();
		}
	}
	return i18n("Delete multiple expired alarms");
}


/*=============================================================================
=  Class: UndoReactivate
=  Undo item for alarm reactivation.
=============================================================================*/

/******************************************************************************
*  Undo the item, i.e. re-archive the alarm which was reactivated.
*  Create a redo item to reactivate the alarm back again.
*  Reply = redo item.
*/
UndoItem* UndoReactivate::restore()
{
	kDebug(5950) << "UndoReactivate::restore()\n";
	// Validate the alarm's calendar
	switch (calendar())
	{
		case KAEvent::ACTIVE:
			break;
		default:
			mRestoreError = ERR_PROG;
			return 0;
	}
	return UndoAdd::doRestore(true);     // restore alarm, ensuring that it is re-archived
}

/******************************************************************************
*  Create a redo item to add the alarm back again.
*/
UndoItem* UndoReactivate::createRedo(const KAEvent& event)
{
	Undo::Type t = (type() == Undo::UNDO) ? Undo::REDO : (type() == Undo::REDO) ? Undo::UNDO : Undo::NONE;
	return new UndoDeactivate(t, event, mName);
}

/******************************************************************************
*  Return the action description of the Undo item for displaying.
*/
QString UndoReactivate::defaultActionText() const
{
	return i18n("Reactivate alarm");
}


/*=============================================================================
=  Class: UndoDeactivate
=  Redo item for alarm reactivation.
=============================================================================*/

/******************************************************************************
*  Undo the item, i.e. reactivate an alarm which was archived.
*  Create a redo item to archive the alarm again.
*  Reply = redo item.
*/
UndoItem* UndoDeactivate::restore()
{
	kDebug(5950) << "UndoDeactivate::restore()\n";
	// Validate the alarm's calendar
	switch (calendar())
	{
		case KAEvent::ACTIVE:
			break;
		default:
			mRestoreError = ERR_PROG;
			return 0;
	}

	return UndoDelete::restore();
}

/******************************************************************************
*  Create a redo item to archive the alarm again.
*/
UndoItem* UndoDeactivate::createRedo(const KAEvent& event)
{
	Undo::Type t = (type() == Undo::UNDO) ? Undo::REDO : (type() == Undo::REDO) ? Undo::UNDO : Undo::NONE;
	return new UndoReactivate(t, event, mName);
}

/******************************************************************************
*  Return the action description of the Undo item for displaying.
*/
QString UndoDeactivate::defaultActionText() const
{
	return i18n("Reactivate alarm");
}


/*=============================================================================
=  Class: UndoReactivates
=  Undo item for multiple alarm reactivation.
=============================================================================*/

/******************************************************************************
*  Create a redo item to reactivate the alarms again.
*/
UndoItem* UndoReactivates::createRedo(Undo::List& undos)
{
	Undo::Type t = (type() == Undo::UNDO) ? Undo::REDO : (type() == Undo::REDO) ? Undo::UNDO : Undo::NONE;
	return new UndoReactivates(t, undos, mName);
}

/******************************************************************************
*  Return the action description of the Undo item for displaying.
*/
QString UndoReactivates::defaultActionText() const
{
	return i18n("Reactivate multiple alarms");
}
