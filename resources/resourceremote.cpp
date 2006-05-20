/*
 *  resourceremote.cpp  -  KAlarm remote alarm calendar resource
 *  Program:  kalarm
 *  Copyright © 2006 by David Jarvie <software@astrojar.org.uk>
 *  Based on resourceremote.cpp in kresources,
 *  Copyright (c) 2003,2004 Cornelius Schumacher <schumacher@kde.org>
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

#include <klocale.h>
#include <kio/job.h>
#include <kstandarddirs.h>
#include <kio/jobclasses.h>

//#include <libkdepim/progressmanager.h>

#include "resourceremote.moc"

using namespace KCal;


KAResourceRemote::KAResourceRemote(const KConfig* config)
	: AlarmResource(config),
	  mDownloadJob(0),
	  mUploadJob(0),
	  mShowProgress(true),
	  mDownloaded(false),
	  mUseCacheFile(true),
	  mLoadFromCache(false),
	  mLoadCachedExecuting(false)
{
	if (config)
	{
		mDownloadUrl = KUrl(config->readEntry("DownloadUrl"));
		mUploadUrl = KUrl(config->readEntry("UploadUrl"));
		ResourceCached::readConfig(config );
	}
	init();
}

KAResourceRemote::KAResourceRemote(Type type, const KUrl& downloadUrl, const KUrl& uploadUrl)
	: AlarmResource(type),
	  mDownloadUrl(downloadUrl),
	  mUploadUrl(uploadUrl.isEmpty() ? mDownloadUrl : uploadUrl),
	  mDownloadJob(0),
	  mUploadJob(0),
	  mShowProgress(false),
	  mDownloaded(false),
	  mUseCacheFile(false),
	  mLoadFromCache(false),
	  mLoadCachedExecuting(false)
{
	init();
}

void KAResourceRemote::init()
{
	setType("remote");   // set resource type
	lock(QString(""));   // don't use QString::null !
	enableChangeNotification();
}

KAResourceRemote::~KAResourceRemote()
{
	if (isOpen())
		close();
	cancelDownload();
	if (mUploadJob)
		mUploadJob->kill();
}

void KAResourceRemote::writeConfig(KConfig* config)
{
	config->writeEntry("DownloadUrl", mDownloadUrl.url());
	config->writeEntry("UploadUrl", mUploadUrl.url());
	AlarmResource::writeConfig(config);
}

void KAResourceRemote::startReconfig()
{
	mNewDownloadUrl = mDownloadUrl;
	mNewUploadUrl   = mUploadUrl;
	AlarmResource::startReconfig();
}

void KAResourceRemote::applyReconfig()
{
	if (mReconfiguring)
	{
		AlarmResource::applyReconfig();
		if (setUrls(mNewDownloadUrl, mNewUploadUrl))
			mReconfiguring = 3;    // indicate that location has changed
		AlarmResource::applyReconfig();
	}
}

void KAResourceRemote::enableResource(bool enable)
{
	if (!enable)
		cancelDownload(false);
}

bool KAResourceRemote::setLoadUpdateCache(bool update)
{
	if (!AlarmResource::setLoadUpdateCache(update))
		return false;
	if (update)
		mDownloaded = false;    // ensure that the resource is downloaded now
	setLoadingFromCache();
	return true;
}

// Set the default cache refresh action for the next load().
void KAResourceRemote::setLoadingFromCache()
{
	if (!mLoadCachedExecuting)
	{
		switch (reloadPolicy())
		{
			case ReloadNever:
				mLoadFromCache = true;
				return;
			case ReloadInterval:
				setReloadPolicy(ReloadInterval);     // restart the reload timer
				// fall through to ReloadOnStartup
			case ReloadOnStartup:
				mLoadFromCache = mDownloaded || !loadUpdateCache();
				break;
		}
		if (!mLoadFromCache  &&  !mDownloaded)
			load();
	}
}

bool KAResourceRemote::loadCached(bool refreshCache)
{
	if (mLoadCachedExecuting)
		return false;    // another loadCached() is already in progress
	mLoadCachedExecuting = true;
	kDebug(KARES_DEBUG) << "KAResourceRemote::loadCached(" << refreshCache << ")" << endl;
	mLoadFromCache = !refreshCache && KStandardDirs::exists(cacheFile());
//if (mLoadFromCache) kDebug(KARES_DEBUG)<<"***cacheFile="<<cacheFile()<<endl;
	bool success = AlarmResource::load();
	// Reset the load-from-cache indicator to the default, so that a subsequent
	// load() works as expected.
	mLoadCachedExecuting = false;
	setLoadingFromCache();
	return success;
}

bool KAResourceRemote::doLoad()
{
	kDebug(KARES_DEBUG) << "KAResourceRemote::doLoad(" << mDownloadUrl.prettyURL() << ")" << endl;
	if (mDownloadJob)
	{
		kWarning(KARES_DEBUG) << "KAResourceRemote::doLoad(): download still in progress" << endl;
		return false;
	}
	if (mUploadJob  &&  !mLoadFromCache)
	{
		kWarning(KARES_DEBUG) << "KAResourceRemote::doLoad(): upload still in progress" << endl;
		return false;
	}
	mLoaded = false;
	mCalendar.close();
	if (!isActive())
		return false;
	mLoading = true;

	if (mUseCacheFile  ||  mLoadFromCache)
	{
		disableChangeNotification();
		loadFromCache();
		mUseCacheFile = false;
		enableChangeNotification();
	}
	clearChanges();
	emit resourceChanged(this);

	if (mLoadFromCache)
	{
		kDebug(KARES_DEBUG) << "KAResourceRemote::doLoad(" << mDownloadUrl.prettyURL() << "): from cache" << endl;
		slotLoadJobResult(0);
	}
	else
	{
		kDebug(KARES_DEBUG) << "KAResourceRemote::slotLoadJobResult(" << mDownloadUrl.prettyURL() << "): success" << endl;
		mDownloaded = true;    // the resource has now been downloaded at least once
		kDebug(KARES_DEBUG) << "Download from: " << mDownloadUrl.prettyURL() << "): success"<< endl;
		mDownloadJob = KIO::file_copy(mDownloadUrl, KUrl(cacheFile()), -1, true,
					      false, mShowProgress);
		connect(mDownloadJob, SIGNAL(result(KIO::Job*)), SLOT(slotLoadJobResult(KIO::Job*)));
#if 0
		if (mShowProgress)
		{
			connect(mDownloadJob, SIGNAL(percent(KIO::Job*, unsigned long)),
					      SLOT(slotPercent(KIO::Job*, unsigned long)));
			emit downloading(this, 0);
		}
#endif
	}
	return true;
}

void KAResourceRemote::slotPercent(KIO::Job*, unsigned long percent)
{
#if 0
	emit downloading(this, percent);
#endif
}

void KAResourceRemote::slotLoadJobResult(KIO::Job* job)
{
	bool err = false;
	if (job)
	{
		mCalendar.close();
		if (job->error())
		{
			job->showErrorDialog(0);
			setEnabled(false);
			err = true;
		}
		else
		{
			kDebug(KARES_DEBUG) << "KAResourceRemote::slotLoadJobResult(" << mDownloadUrl.prettyURL() << "): success" << endl;
			emit cacheDownloaded(this);
			disableChangeNotification();
			loadFromCache();
			enableChangeNotification();
			emit resourceChanged(this);
		}

		mDownloadJob = 0;
#if 0
		emit downloading(this, (unsigned long)-1);
#endif
	}

	if (!err)
	{
		checkCompatibility(cacheFile());
		mLoaded = true;
	}
	mLoading = false;
	emit loaded(this);
}

void KAResourceRemote::cancelDownload(bool disable)
{
	if (mDownloadJob)
	{
		mDownloadJob->kill();
		mDownloadJob = 0;
		if (disable)
			setEnabled(false);
#if 0
		emit downloading(this, (unsigned long)-1);
#endif
		mLoading = false;
		emit loaded(this);
	}
}

bool KAResourceRemote::doSave()
{
	kDebug(KARES_DEBUG) << "KAResourceRemote::doSave(" << mUploadUrl.prettyURL() << ")" << endl;
	if (readOnly()  ||  !hasChanges())
		return true;
	if (mDownloadJob)
	{
		kWarning(KARES_DEBUG) << "KAResourceRemote::doSave(): download still in progress" << endl;
		return false;
	}
	if (mUploadJob)
	{
		kWarning(KARES_DEBUG) << "KAResourceRemote::doSave(): upload still in progress" << endl;
		return false;
	}

	mChangedIncidences = allChanges();
	saveToCache();
	mUploadJob = KIO::file_copy(KUrl(cacheFile()), mUploadUrl, -1, true);
	connect(mUploadJob, SIGNAL(result(KIO::Job*)), SLOT(slotSaveJobResult(KIO::Job*)));
	return true;
}

void KAResourceRemote::slotSaveJobResult(KIO::Job* job)
{
	if (job->error())
		job->showErrorDialog(0);
	else
	{
		kDebug(KARES_DEBUG) << "KAResourceRemote::slotSaveJobResult(" << mUploadUrl.prettyURL() << "): success" << endl;
		for(Incidence::List::ConstIterator it = mChangedIncidences.begin();  it != mChangedIncidences.end();  ++it)
			clearChange(*it);
		mChangedIncidences.clear();
	}

	mUploadJob = 0;
	emit resourceSaved(this);
}

bool KAResourceRemote::setUrls(const KUrl& downloadUrl, const KUrl& uploadUrl)
{
	if (mReconfiguring == 1)
	{
		mNewDownloadUrl = downloadUrl;
		mNewUploadUrl   = uploadUrl;
		return true;
	}
	if (downloadUrl.equals(mDownloadUrl)
	&&  uploadUrl.equals(mUploadUrl))
		return false;
	kDebug(KARES_DEBUG) << "KAResourceRemote::setUrls(" << downloadUrl.prettyURL() << ", " << uploadUrl.prettyURL() << ")\n";
	if (isOpen())
		close();
	bool active = isActive();
	if (active)
		enableResource(false);
	mDownloadUrl = downloadUrl;
	mUploadUrl   = uploadUrl;
	if (active)
		enableResource(true);
	// Trigger loading the new resource, and ensure that the new configuration is saved
	emit locationChanged(this);
	return true;
}

QString KAResourceRemote::location(bool prefix) const
{
	QString loc = mDownloadUrl.prettyURL();
	return prefix ? i18n("URL: %1", loc) : loc;
}