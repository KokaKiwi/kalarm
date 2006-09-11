/*
 *  dbushandler.cpp  -  handler for D-Bus calls by other applications
 *  Program:  kalarm
 *  Copyright © 2002-2006 by David Jarvie <software@astrojar.org.uk>
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

#include <stdlib.h>

#include <QtDBus>
#include <kdebug.h>

#include <libkpimidentities/identitymanager.h>
#include <libkpimidentities/identity.h>

#include "alarmcalendar.h"
#include "daemon.h"
#include "functions.h"
#include "kalarmapp.h"
#include "kamail.h"
#include "karecurrence.h"
#include "mainwindow.h"
#include "preferences.h"
#include "dbushandler.moc"
#include <requestadaptor.h>
static const char* REQUEST_DBUS_OBJECT = "/request";   // D-Bus object path of KAlarm's request interface


/*=============================================================================
= DBusHandler
= This class's function is to handle D-Bus requests by other applications.
=============================================================================*/
DBusHandler::DBusHandler()
{
	kDebug(5950) << "DBusHandler::DBusHandler()\n";
	new RequestAdaptor(this);
	QDBusConnection::sessionBus().registerObject(REQUEST_DBUS_OBJECT, this);
}


bool DBusHandler::cancelEvent(const QString& eventId)
{
	return theApp()->dcopDeleteEvent(eventId);
}

bool DBusHandler::triggerEvent(const QString& eventId)
{
	return theApp()->dcopTriggerEvent(eventId);
}

bool DBusHandler::scheduleMessage(const QString& message, const QString& startDateTime, int lateCancel, unsigned flags,
                                  const QString& bgColor, const QString& fgColor, const QString& font,
                                  const QString& audioUrl, int reminderMins, const QString& recurrence,
                                  int repeatInterval, int repeatCount)
{
	KDateTime start;
	KARecurrence recur;
	if (!convertRecurrence(start, recur, startDateTime, recurrence))
		return false;
	return scheduleMessage(message, start, lateCancel, flags, bgColor, fgColor, font, KUrl(audioUrl), reminderMins, recur, repeatInterval, repeatCount);
}

bool DBusHandler::scheduleMessage(const QString& message, const QString& startDateTime, int lateCancel, unsigned flags,
                                  const QString& bgColor, const QString& fgColor, const QString& font,
                                  const QString& audioUrl, int reminderMins,
                                  int recurType, int recurInterval, int recurCount)
{
	KDateTime start;
	KARecurrence recur;
	if (!convertRecurrence(start, recur, startDateTime, recurType, recurInterval, recurCount))
		return false;
	return scheduleMessage(message, start, lateCancel, flags, bgColor, fgColor, font, KUrl(audioUrl), reminderMins, recur);
}

bool DBusHandler::scheduleMessage(const QString& message, const QString& startDateTime, int lateCancel, unsigned flags,
                                  const QString& bgColor, const QString& fgColor, const QString& font,
                                  const QString& audioUrl, int reminderMins,
                                  int recurType, int recurInterval, const QString& endDateTime)
{
	KDateTime start;
	KARecurrence recur;
	if (!convertRecurrence(start, recur, startDateTime, recurType, recurInterval, endDateTime))
		return false;
	return scheduleMessage(message, start, lateCancel, flags, bgColor, fgColor, font, KUrl(audioUrl), reminderMins, recur);
}

bool DBusHandler::scheduleFile(const QString& url, const QString& startDateTime, int lateCancel, unsigned flags, const QString& bgColor,
                               const QString& audioUrl, int reminderMins, const QString& recurrence,
                               int repeatInterval, int repeatCount)
{
	KDateTime start;
	KARecurrence recur;
	if (!convertRecurrence(start, recur, startDateTime, recurrence))
		return false;
	return scheduleFile(KUrl(url), start, lateCancel, flags, bgColor, KUrl(audioUrl), reminderMins, recur, repeatInterval, repeatCount);
}

bool DBusHandler::scheduleFile(const QString& url, const QString& startDateTime, int lateCancel, unsigned flags, const QString& bgColor,
                               const QString& audioUrl, int reminderMins, int recurType, int recurInterval, int recurCount)
{
	KDateTime start;
	KARecurrence recur;
	if (!convertRecurrence(start, recur, startDateTime, recurType, recurInterval, recurCount))
		return false;
	return scheduleFile(KUrl(url), start, lateCancel, flags, bgColor, KUrl(audioUrl), reminderMins, recur);
}

bool DBusHandler::scheduleFile(const QString& url, const QString& startDateTime, int lateCancel, unsigned flags, const QString& bgColor,
                               const QString& audioUrl, int reminderMins, int recurType, int recurInterval, const QString& endDateTime)
{
	KDateTime start;
	KARecurrence recur;
	if (!convertRecurrence(start, recur, startDateTime, recurType, recurInterval, endDateTime))
		return false;
	return scheduleFile(KUrl(url), start, lateCancel, flags, bgColor, KUrl(audioUrl), reminderMins, recur);
}

bool DBusHandler::scheduleCommand(const QString& commandLine, const QString& startDateTime, int lateCancel, unsigned flags,
                                  const QString& recurrence, int repeatInterval, int repeatCount)
{
	KDateTime start;
	KARecurrence recur;
	if (!convertRecurrence(start, recur, startDateTime, recurrence))
		return false;
	return scheduleCommand(commandLine, start, lateCancel, flags, recur, repeatInterval, repeatCount);
}

bool DBusHandler::scheduleCommand(const QString& commandLine, const QString& startDateTime, int lateCancel, unsigned flags,
                                  int recurType, int recurInterval, int recurCount)
{
	KDateTime start = convertDateTime(startDateTime, true);
	if (!start.isValid())
		return false;
	KARecurrence recur;
	if (!convertRecurrence(start, recur, startDateTime, recurType, recurInterval, recurCount))
		return false;
	return scheduleCommand(commandLine, start, lateCancel, flags, recur);
}

bool DBusHandler::scheduleCommand(const QString& commandLine, const QString& startDateTime, int lateCancel, unsigned flags,
                                  int recurType, int recurInterval, const QString& endDateTime)
{
	KDateTime start;
	KARecurrence recur;
	if (!convertRecurrence(start, recur, startDateTime, recurType, recurInterval, endDateTime))
		return false;
	return scheduleCommand(commandLine, start, lateCancel, flags, recur);
}

bool DBusHandler::scheduleEmail(const QString& fromID, const QString& addresses, const QString& subject, const QString& message,
                                const QString& attachments, const QString& startDateTime, int lateCancel, unsigned flags,
                                const QString& recurrence, int repeatInterval, int repeatCount)
{
	KDateTime start;
	KARecurrence recur;
	if (!convertRecurrence(start, recur, startDateTime, recurrence))
		return false;
	return scheduleEmail(fromID, addresses, subject, message, attachments, start, lateCancel, flags, recur, repeatInterval, repeatCount);
}

bool DBusHandler::scheduleEmail(const QString& fromID, const QString& addresses, const QString& subject, const QString& message,
                                const QString& attachments, const QString& startDateTime, int lateCancel, unsigned flags,
                                int recurType, int recurInterval, int recurCount)
{
	KDateTime start;
	KARecurrence recur;
	if (!convertRecurrence(start, recur, startDateTime, recurType, recurInterval, recurCount))
		return false;
	return scheduleEmail(fromID, addresses, subject, message, attachments, start, lateCancel, flags, recur);
}

bool DBusHandler::scheduleEmail(const QString& fromID, const QString& addresses, const QString& subject, const QString& message,
                                const QString& attachments, const QString& startDateTime, int lateCancel, unsigned flags,
                                int recurType, int recurInterval, const QString& endDateTime)
{
	KDateTime start;
	KARecurrence recur;
	if (!convertRecurrence(start, recur, startDateTime, recurType, recurInterval, endDateTime))
		return false;
	return scheduleEmail(fromID, addresses, subject, message, attachments, start, lateCancel, flags, recur);
}

bool DBusHandler::edit(const QString& eventID)
{
	return KAlarm::edit(eventID);
}

bool DBusHandler::editNew(const QString& templateName)
{
	return KAlarm::editNew(templateName);
}


/******************************************************************************
* Schedule a message alarm, after converting the parameters from strings.
*/
bool DBusHandler::scheduleMessage(const QString& message, const KDateTime& start, int lateCancel, unsigned flags,
                                  const QString& bgColor, const QString& fgColor, const QString& fontStr,
                                  const KUrl& audioFile, int reminderMins, const KARecurrence& recurrence,
                                  int repeatInterval, int repeatCount)
{
	unsigned kaEventFlags = convertStartFlags(start, flags);
	QColor bg = convertBgColour(bgColor);
	if (!bg.isValid())
		return false;
	QColor fg;
	if (fgColor.isEmpty())
		fg = Preferences::defaultFgColour();
	else
	{
		fg.setNamedColor(fgColor);
		if (!fg.isValid())
		{
			kError(5950) << "D-Bus call: invalid foreground color: " << fgColor << endl;
			return false;
		}
	}
	QFont font;
	if (fontStr.isEmpty())
		kaEventFlags |= KAEvent::DEFAULT_FONT;
	else
	{
		if (!font.fromString(fontStr))    // N.B. this doesn't do good validation
		{
			kError(5950) << "D-Bus call: invalid font: " << fontStr << endl;
			return false;
		}
	}
	return theApp()->scheduleEvent(KAEvent::MESSAGE, message, start, lateCancel, kaEventFlags, bg, fg, font,
	                               audioFile.url(), -1, reminderMins, recurrence, repeatInterval, repeatCount);
}

/******************************************************************************
* Schedule a file alarm, after converting the parameters from strings.
*/
bool DBusHandler::scheduleFile(const KUrl& file,
                               const KDateTime& start, int lateCancel, unsigned flags, const QString& bgColor,
                               const KUrl& audioFile, int reminderMins, const KARecurrence& recurrence,
                               int repeatInterval, int repeatCount)
{
	unsigned kaEventFlags = convertStartFlags(start, flags);
	QColor bg = convertBgColour(bgColor);
	if (!bg.isValid())
		return false;
	return theApp()->scheduleEvent(KAEvent::FILE, file.url(), start, lateCancel, kaEventFlags, bg, Qt::black, QFont(),
	                               audioFile.url(), -1, reminderMins, recurrence, repeatInterval, repeatCount);
}

/******************************************************************************
* Schedule a command alarm, after converting the parameters from strings.
*/
bool DBusHandler::scheduleCommand(const QString& commandLine,
                                  const KDateTime& start, int lateCancel, unsigned flags,
                                  const KARecurrence& recurrence, int repeatInterval, int repeatCount)
{
	unsigned kaEventFlags = convertStartFlags(start, flags);
	return theApp()->scheduleEvent(KAEvent::COMMAND, commandLine, start, lateCancel, kaEventFlags, Qt::black, Qt::black, QFont(),
	                               QString(), -1, 0, recurrence, repeatInterval, repeatCount);
}

/******************************************************************************
* Schedule an email alarm, after validating the addresses and attachments.
*/
bool DBusHandler::scheduleEmail(const QString& fromID, const QString& addresses, const QString& subject,
                                const QString& message, const QString& attachments,
                                const KDateTime& start, int lateCancel, unsigned flags,
                                const KARecurrence& recurrence, int repeatInterval, int repeatCount)
{
	unsigned kaEventFlags = convertStartFlags(start, flags);
	if (!fromID.isEmpty())
	{
		if (KAMail::identityManager()->identityForName(fromID).isNull())
		{
			kError(5950) << "D-Bus call scheduleEmail(): unknown sender ID: " << fromID << endl;
			return false;
		}
	}
	EmailAddressList addrs;
	QString bad = KAMail::convertAddresses(addresses, addrs);
	if (!bad.isEmpty())
	{
		kError(5950) << "D-Bus call scheduleEmail(): invalid email addresses: " << bad << endl;
		return false;
	}
	if (addrs.isEmpty())
	{
		kError(5950) << "D-Bus call scheduleEmail(): no email address\n";
		return false;
	}
	QStringList atts;
	bad = KAMail::convertAttachments(attachments, atts);
	if (!bad.isEmpty())
	{
		kError(5950) << "D-Bus call scheduleEmail(): invalid email attachment: " << bad << endl;
		return false;
	}
	return theApp()->scheduleEvent(KAEvent::EMAIL, message, start, lateCancel, kaEventFlags, Qt::black, Qt::black, QFont(),
	                               QString(), -1, 0, recurrence, repeatInterval, repeatCount, fromID, addrs, subject, atts);
}


/******************************************************************************
* Convert the start date/time string to a KDateTime. The date/time string is in
* the format YYYY-MM-DD[THH:MM[:SS]][ TZ] or [T]HH:MM[:SS].
* The time zone specifier (TZ) is a system time zone name, e.g. "Europe/London".
* If no time zone is specified, it defaults to the local clock time (which is
* not the same as the local time zone).
*/
KDateTime DBusHandler::convertDateTime(const QString& dateTime, bool start)
{
	bool error = false;
	const KTimeZone* tz = 0;
	QString dtString;
	int space = dateTime.indexOf(QChar(' '));
	if (space > 0)
	{
		dtString = dateTime.left(space);
		tz = KSystemTimeZones::zone(dateTime.mid(space).trimmed());
		error = !tz;
	}
	else
		dtString = dateTime;
	KDateTime result;
	if (!error)
	{
		if (dtString.length() > 10)
		{
			// Both a date and a time are specified
			QDateTime dt = QDateTime::fromString(dtString, Qt::ISODate);
			if (tz)
				result = KDateTime(dt, tz);
			else
				result = KDateTime(dt, KDateTime::ClockTime);
		}
		else
		{
			// Check whether a time is specified
			QString t;
			if (dtString[0] == QLatin1Char('T'))
				t = dtString.mid(1);     // it's a time: remove the leading 'T'
			else if (!dtString[2].isDigit())
				t = dtString;            // it's a time with no leading 'T'

			if (t.isEmpty())
			{
				// It's a date
				QDate d = QDate::fromString(dtString, Qt::ISODate);
				if (tz)
					result = KDateTime(d, tz);
				else
					result = KDateTime(d, KDateTime::ClockTime);
			}
			else if (start)
			{
				// It's a time, so use today as the date
				if (!tz)
					result = KDateTime(QDate::currentDate(), QTime::fromString(t, Qt::ISODate), KDateTime::ClockTime);
			}
		}
	}
	if (!result.isValid())
	{
		if (start)
			kError(5950) << "D-Bus call: invalid start date/time: '" << dateTime << "'" << endl;
		else
			kError(5950) << "D-Bus call: invalid recurrence end date/time: '" << dateTime << "'" << endl;
	}
	return result;
}

/******************************************************************************
* Convert the flag bits to KAEvent flag bits.
*/
unsigned DBusHandler::convertStartFlags(const KDateTime& start, unsigned flags)
{
	unsigned kaEventFlags = 0;
	if (flags & REPEAT_AT_LOGIN) kaEventFlags |= KAEvent::REPEAT_AT_LOGIN;
	if (flags & BEEP)            kaEventFlags |= KAEvent::BEEP;
	if (flags & SPEAK)           kaEventFlags |= KAEvent::SPEAK;
	if (flags & CONFIRM_ACK)     kaEventFlags |= KAEvent::CONFIRM_ACK;
	if (flags & REPEAT_SOUND)    kaEventFlags |= KAEvent::REPEAT_SOUND;
	if (flags & AUTO_CLOSE)      kaEventFlags |= KAEvent::AUTO_CLOSE;
	if (flags & EMAIL_BCC)       kaEventFlags |= KAEvent::EMAIL_BCC;
	if (flags & SCRIPT)          kaEventFlags |= KAEvent::SCRIPT;
	if (flags & EXEC_IN_XTERM)   kaEventFlags |= KAEvent::EXEC_IN_XTERM;
	if (flags & SHOW_IN_KORG)    kaEventFlags |= KAEvent::COPY_KORGANIZER;
	if (flags & DISABLED)        kaEventFlags |= KAEvent::DISABLED;
	if (start.isDateOnly())      kaEventFlags |= KAEvent::ANY_TIME;
	return kaEventFlags;
}

/******************************************************************************
* Convert the background colour string to a QColor.
*/
QColor DBusHandler::convertBgColour(const QString& bgColor)
{
	if (bgColor.isEmpty())
		return Preferences::defaultBgColour();
	QColor bg(bgColor);
	if (!bg.isValid())
			kError(5950) << "D-Bus call: invalid background color: " << bgColor << endl;
	return bg;
}

bool DBusHandler::convertRecurrence(KDateTime& start, KARecurrence& recurrence, 
                                    const QString& startDateTime, const QString& icalRecurrence)
{
	start = convertDateTime(startDateTime, true);
	if (!start.isValid())
		return false;
	return recurrence.set(icalRecurrence);
}

bool DBusHandler::convertRecurrence(KDateTime& start, KARecurrence& recurrence, const QString& startDateTime,
                                    int recurType, int recurInterval, int recurCount)
{
	start = convertDateTime(startDateTime, true);
	if (!start.isValid())
		return false;
	return convertRecurrence(recurrence, start, recurType, recurInterval, recurCount, KDateTime());
}

bool DBusHandler::convertRecurrence(KDateTime& start, KARecurrence& recurrence, const QString& startDateTime,
                                    int recurType, int recurInterval, const QString& endDateTime)
{
	start = convertDateTime(startDateTime, true);
	if (!start.isValid())
		return false;
	KDateTime end = convertDateTime(endDateTime, false);
	if (end.isDateOnly()  &&  !start.isDateOnly())
	{
		kError(5950) << "D-Bus call: alarm is date-only, but recurrence end is date/time" << endl;
		return false;
	}
	if (!end.isDateOnly()  &&  start.isDateOnly())
	{
		kError(5950) << "D-Bus call: alarm is timed, but recurrence end is date-only" << endl;
		return false;
	}
	return convertRecurrence(recurrence, start, recurType, recurInterval, 0, end);
}

bool DBusHandler::convertRecurrence(KARecurrence& recurrence, const KDateTime& start, int recurType,
                                    int recurInterval, int recurCount, const KDateTime& end)
{
	KARecurrence::Type type;
	switch (recurType)
	{
		case MINUTELY:  type = KARecurrence::MINUTELY;  break;
		case DAILY:     type = KARecurrence::DAILY;  break;
		case WEEKLY:    type = KARecurrence::WEEKLY;  break;
		case MONTHLY:   type = KARecurrence::MONTHLY_DAY;  break;
		case YEARLY:    type = KARecurrence::ANNUAL_DATE;  break;
			break;
		default:
			kError(5950) << "D-Bus call: invalid repeat type: " << recurType << endl;
			return false;
	}
	recurrence.set(type, recurInterval, recurCount, start, end);
	return true;
}