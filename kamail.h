/*
 *  kamail.h  -  email functions
 *  Program:  kalarm
 *  (C) 2002 by David Jarvie  software@astrojar.org.uk
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

#ifndef KAMAIL_H
#define KAMAIL_H

#include <qstring.h>
class KAlarmEvent;
class EmailAddressList;


class KAMail
{
	public:
		static bool       send(const KAlarmEvent&);
		static int        checkAddress(QString& address);
		static int        checkAttachment(QString& attachment)  { return checkAttachment(attachment, true); }
		static QString    convertAddresses(const QString& addresses, EmailAddressList&);
		static QString    convertAttachments(const QString& attachments, QStringList& list, bool check);
	private:
#if QT_VERSION >= 300
		typedef QIODevice::Offset Offset;
#else
		typedef uint Offset;
#endif
		static bool       sendKMail(const KAlarmEvent&, const QString& from);
		static QString    appendBodyAttachments(QString& message, const KAlarmEvent&);
		static int        checkAttachment(QString& attachment, bool check);
		static Offset     base64Encode(char* in, char* out, Offset size);
};

#endif // KAMAIL_H
