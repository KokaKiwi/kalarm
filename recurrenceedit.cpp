/*
 *  recurrenceedit.cpp  -  widget to edit the event's recurrence definition
 *  Program:  kalarm
 *  Copyright (c) 2002 - 2005 by David Jarvie <software@astrojar.org.uk>
 *
 *  Based originally on KOrganizer module koeditorrecurrence.cpp,
 *  Copyright (c) 2000,2001 Cornelius Schumacher <schumacher@kde.org>
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

#include <qtooltip.h>
#include <qlayout.h>
#include <q3vbox.h>
#include <q3widgetstack.h>
#include <q3listbox.h>
#include <q3frame.h>
#include <q3groupbox.h>
#include <qlabel.h>
#include <qpushbutton.h>
#include <qlineedit.h>
#include <q3whatsthis.h>
//Added by qt3to4:
#include <QGridLayout>
#include <QHBoxLayout>
#include <QBoxLayout>
#include <Q3ValueList>
#include <QShowEvent>
#include <QVBoxLayout>

#include <kglobal.h>
#include <klocale.h>
#include <kiconloader.h>
#include <kdialog.h>
#include <kmessagebox.h>
#include <kcalendarsystem.h>
#include <kdebug.h>

#include <libkcal/event.h>

#include "alarmevent.h"
#include "alarmtimewidget.h"
#include "checkbox.h"
#include "combobox.h"
#include "dateedit.h"
#include "functions.h"
#include "kalarmapp.h"
#include "karecurrence.h"
#include "preferences.h"
#include "radiobutton.h"
#include "spinbox.h"
#include "timeedit.h"
#include "timespinbox.h"
#include "buttongroup.h"
using namespace KCal;

#include "recurrenceedit.moc"
#include "recurrenceeditprivate.moc"


// Collect these widget labels together to ensure consistent wording and
// translations across different modules.
QString RecurrenceEdit::i18n_Norecur()           { return i18n("No recurrence"); }
QString RecurrenceEdit::i18n_NoRecur()           { return i18n("No Recurrence"); }
QString RecurrenceEdit::i18n_AtLogin()           { return i18n("At Login"); }
QString RecurrenceEdit::i18n_l_Atlogin()         { return i18n("At &login"); }
QString RecurrenceEdit::i18n_HourlyMinutely()    { return i18n("Hourly/Minutely"); }
QString RecurrenceEdit::i18n_u_HourlyMinutely()  { return i18n("Ho&urly/Minutely"); }
QString RecurrenceEdit::i18n_Daily()             { return i18n("Daily"); }
QString RecurrenceEdit::i18n_d_Daily()           { return i18n("&Daily"); }
QString RecurrenceEdit::i18n_Weekly()            { return i18n("Weekly"); }
QString RecurrenceEdit::i18n_w_Weekly()          { return i18n("&Weekly"); }
QString RecurrenceEdit::i18n_Monthly()           { return i18n("Monthly"); }
QString RecurrenceEdit::i18n_m_Monthly()         { return i18n("&Monthly"); }
QString RecurrenceEdit::i18n_Yearly()            { return i18n("Yearly"); }
QString RecurrenceEdit::i18n_y_Yearly()          { return i18n("&Yearly"); }


RecurrenceEdit::RecurrenceEdit(bool readOnly, QWidget* parent, const char* name)
	: Q3Frame(parent, name),
	  mRuleButtonType(INVALID_RECUR),
	  mDailyShown(false),
	  mWeeklyShown(false),
	  mMonthlyShown(false),
	  mYearlyShown(false),
	  noEmitTypeChanged(true),
	  mReadOnly(readOnly)
{
	QBoxLayout* layout;
	QVBoxLayout* topLayout = new QVBoxLayout(this, 0, KDialog::spacingHint());

	/* Create the recurrence rule Group box which holds the recurrence period
	 * selection buttons, and the weekly, monthly and yearly recurrence rule
	 * frames which specify options individual to each of these distinct
	 * sections of the recurrence rule. Each frame is made visible by the
	 * selection of its corresponding radio button.
	 */

	recurGroup = new Q3GroupBox(1, Qt::Vertical, i18n("Recurrence Rule"), this, "recurGroup");
	topLayout->addWidget(recurGroup);
	ruleFrame = new Q3Frame(recurGroup, "ruleFrame");
	layout = new QVBoxLayout(ruleFrame, 0);
	layout->addSpacing(KDialog::spacingHint()/2);

	layout = new QHBoxLayout(layout, 0);
	QBoxLayout* lay = new QVBoxLayout(layout, 0);
	QWidget* ruleButtonBox = new QWidget(ruleFrame);
	QVBoxLayout* ruleLayout = new QVBoxLayout(ruleButtonBox, 0);
	lay->addWidget(ruleButtonBox);
	lay->addStretch();    // top-adjust the interval radio buttons
	ruleButtonGroup = new ButtonGroup(ruleButtonBox);
	connect(ruleButtonGroup, SIGNAL(buttonSet(QAbstractButton*)), SLOT(periodClicked(QAbstractButton*)));

	mNoneButton = new RadioButton(i18n_Norecur(), ruleButtonBox);
	mNoneButton->setFixedSize(mNoneButton->sizeHint());
	mNoneButton->setReadOnly(mReadOnly);
	Q3WhatsThis::add(mNoneButton, i18n("Do not repeat the alarm"));
	ruleButtonGroup->addButton(mNoneButton);
	ruleLayout->addWidget(mNoneButton);

	mAtLoginButton = new RadioButton(i18n_l_Atlogin(), ruleButtonBox);
	mAtLoginButton->setFixedSize(mAtLoginButton->sizeHint());
	mAtLoginButton->setReadOnly(mReadOnly);
	Q3WhatsThis::add(mAtLoginButton,
	      i18n("Trigger the alarm at the specified date/time and at every login until then.\n"
	           "Note that it will also be triggered any time the alarm daemon is restarted."));
	ruleButtonGroup->addButton(mAtLoginButton);
	ruleLayout->addWidget(mAtLoginButton);

	mSubDailyButton = new RadioButton(i18n_u_HourlyMinutely(), ruleButtonBox);
	mSubDailyButton->setFixedSize(mSubDailyButton->sizeHint());
	mSubDailyButton->setReadOnly(mReadOnly);
	Q3WhatsThis::add(mSubDailyButton,
	      i18n("Repeat the alarm at hourly/minutely intervals"));
	ruleButtonGroup->addButton(mSubDailyButton);
	ruleLayout->addWidget(mSubDailyButton);

	mDailyButton = new RadioButton(i18n_d_Daily(), ruleButtonBox);
	mDailyButton->setFixedSize(mDailyButton->sizeHint());
	mDailyButton->setReadOnly(mReadOnly);
	Q3WhatsThis::add(mDailyButton,
	      i18n("Repeat the alarm at daily intervals"));
	ruleButtonGroup->addButton(mDailyButton);
	ruleLayout->addWidget(mDailyButton);

	mWeeklyButton = new RadioButton(i18n_w_Weekly(), ruleButtonBox);
	mWeeklyButton->setFixedSize(mWeeklyButton->sizeHint());
	mWeeklyButton->setReadOnly(mReadOnly);
	Q3WhatsThis::add(mWeeklyButton,
	      i18n("Repeat the alarm at weekly intervals"));
	ruleButtonGroup->addButton(mWeeklyButton);
	ruleLayout->addWidget(mWeeklyButton);

	mMonthlyButton = new RadioButton(i18n_m_Monthly(), ruleButtonBox);
	mMonthlyButton->setFixedSize(mMonthlyButton->sizeHint());
	mMonthlyButton->setReadOnly(mReadOnly);
	Q3WhatsThis::add(mMonthlyButton,
	      i18n("Repeat the alarm at monthly intervals"));
	ruleButtonGroup->addButton(mMonthlyButton);
	ruleLayout->addWidget(mMonthlyButton);

	mYearlyButton = new RadioButton(i18n_y_Yearly(), ruleButtonBox);
	mYearlyButton->setFixedSize(mYearlyButton->sizeHint());
	mYearlyButton->setReadOnly(mReadOnly);
	Q3WhatsThis::add(mYearlyButton,
	      i18n("Repeat the alarm at annual intervals"));
	ruleButtonGroup->addButton(mYearlyButton);
	ruleLayout->addWidget(mYearlyButton);

	lay = new QVBoxLayout(layout);

	lay->addStretch();
	layout = new QHBoxLayout(lay);

	layout->addSpacing(KDialog::marginHint());
	Q3Frame* divider = new Q3Frame(ruleFrame);
	divider->setFrameStyle(Q3Frame::VLine | Q3Frame::Sunken);
	layout->addWidget(divider);
	layout->addSpacing(KDialog::marginHint());

	mNoRule       = new NoRule(ruleFrame, "noFrame");
	mSubDailyRule = new SubDailyRule(mReadOnly, ruleFrame, "subdayFrame");
	mDailyRule    = new DailyRule(mReadOnly, ruleFrame, "dayFrame");
	mWeeklyRule   = new WeeklyRule(mReadOnly, ruleFrame, "weekFrame");
	mMonthlyRule  = new MonthlyRule(mReadOnly, ruleFrame, "monthFrame");
	mYearlyRule   = new YearlyRule(mReadOnly, ruleFrame, "yearFrame");

	connect(mSubDailyRule, SIGNAL(frequencyChanged()), this, SIGNAL(frequencyChanged()));
	connect(mDailyRule, SIGNAL(frequencyChanged()), this, SIGNAL(frequencyChanged()));
	connect(mWeeklyRule, SIGNAL(frequencyChanged()), this, SIGNAL(frequencyChanged()));
	connect(mMonthlyRule, SIGNAL(frequencyChanged()), this, SIGNAL(frequencyChanged()));
	connect(mYearlyRule, SIGNAL(frequencyChanged()), this, SIGNAL(frequencyChanged()));

	ruleStack = new Q3WidgetStack(ruleFrame);
	layout->addWidget(ruleStack);
	layout->addStretch(1);
	ruleStack->addWidget(mNoRule, 0);
	ruleStack->addWidget(mSubDailyRule, 1);
	ruleStack->addWidget(mDailyRule, 2);
	ruleStack->addWidget(mWeeklyRule, 3);
	ruleStack->addWidget(mMonthlyRule, 4);
	ruleStack->addWidget(mYearlyRule, 5);
	layout->addSpacing(KDialog::marginHint());

	// Create the recurrence range group which contains the controls
	// which specify how long the recurrence is to last.

	mRangeButtonBox = new Q3GroupBox(i18n("Recurrence End"), this, "mRangeButtonBox");
	topLayout->addWidget(mRangeButtonBox);
	mRangeButtonGroup = new ButtonGroup(mRangeButtonBox);
	connect(mRangeButtonGroup, SIGNAL(buttonSet(int)), SLOT(rangeTypeClicked()));

	QVBoxLayout* vlayout = new QVBoxLayout(mRangeButtonBox, KDialog::marginHint(), KDialog::spacingHint());
	vlayout->addSpacing(fontMetrics().lineSpacing()/2);
	mNoEndDateButton = new RadioButton(i18n("No &end"), mRangeButtonBox);
	mNoEndDateButton->setFixedSize(mNoEndDateButton->sizeHint());
	mNoEndDateButton->setReadOnly(mReadOnly);
	Q3WhatsThis::add(mNoEndDateButton, i18n("Repeat the alarm indefinitely"));
	mRangeButtonGroup->addButton(mNoEndDateButton);
	vlayout->addWidget(mNoEndDateButton, 1, Qt::AlignAuto);
	QSize size = mNoEndDateButton->size();

	layout = new QHBoxLayout(vlayout, KDialog::spacingHint());
	mRepeatCountButton = new RadioButton(i18n("End a&fter:"), mRangeButtonBox);
	mRepeatCountButton->setReadOnly(mReadOnly);
	Q3WhatsThis::add(mRepeatCountButton,
	      i18n("Repeat the alarm for the number of times specified"));
	mRangeButtonGroup->addButton(mRepeatCountButton);
	mRepeatCountEntry = new SpinBox(1, 9999, 1, mRangeButtonBox);
	mRepeatCountEntry->setFixedSize(mRepeatCountEntry->sizeHint());
	mRepeatCountEntry->setSingleShiftStep(10);
	mRepeatCountEntry->setSelectOnStep(false);
	mRepeatCountEntry->setReadOnly(mReadOnly);
	connect(mRepeatCountEntry, SIGNAL(valueChanged(int)), SLOT(repeatCountChanged(int)));
	Q3WhatsThis::add(mRepeatCountEntry,
	      i18n("Enter the total number of times to trigger the alarm"));
	mRepeatCountButton->setFocusWidget(mRepeatCountEntry);
	mRepeatCountLabel = new QLabel(i18n("occurrence(s)"), mRangeButtonBox);
	mRepeatCountLabel->setFixedSize(mRepeatCountLabel->sizeHint());
	layout->addWidget(mRepeatCountButton);
	layout->addSpacing(KDialog::spacingHint());
	layout->addWidget(mRepeatCountEntry);
	layout->addWidget(mRepeatCountLabel);
	layout->addStretch();
	size = size.expandedTo(mRepeatCountButton->sizeHint());

	layout = new QHBoxLayout(vlayout, KDialog::spacingHint());
	mEndDateButton = new RadioButton(i18n("End &by:"), mRangeButtonBox);
	mEndDateButton->setReadOnly(mReadOnly);
	Q3WhatsThis::add(mEndDateButton,
	      i18n("Repeat the alarm until the date/time specified"));
	mRangeButtonGroup->addButton(mEndDateButton);
	mEndDateEdit = new DateEdit(mRangeButtonBox);
	mEndDateEdit->setFixedSize(mEndDateEdit->sizeHint());
	mEndDateEdit->setReadOnly(mReadOnly);
	Q3WhatsThis::add(mEndDateEdit,
	      i18n("Enter the last date to repeat the alarm"));
	mEndDateButton->setFocusWidget(mEndDateEdit);
	mEndTimeEdit = new TimeEdit(mRangeButtonBox);
	mEndTimeEdit->setFixedSize(mEndTimeEdit->sizeHint());
	mEndTimeEdit->setReadOnly(mReadOnly);
	static const QString lastTimeText = i18n("Enter the last time to repeat the alarm.");
	Q3WhatsThis::add(mEndTimeEdit, QString("%1\n\n%2").arg(lastTimeText).arg(TimeSpinBox::shiftWhatsThis()));
	mEndAnyTimeCheckBox = new CheckBox(i18n("Any time"), mRangeButtonBox);
	mEndAnyTimeCheckBox->setFixedSize(mEndAnyTimeCheckBox->sizeHint());
	mEndAnyTimeCheckBox->setReadOnly(mReadOnly);
	connect(mEndAnyTimeCheckBox, SIGNAL(toggled(bool)), SLOT(slotAnyTimeToggled(bool)));
	Q3WhatsThis::add(mEndAnyTimeCheckBox,
	      i18n("Stop repeating the alarm after your first login on or after the specified end date"));
	layout->addWidget(mEndDateButton);
	layout->addSpacing(KDialog::spacingHint());
	layout->addWidget(mEndDateEdit);
	layout->addWidget(mEndTimeEdit);
	layout->addWidget(mEndAnyTimeCheckBox);
	layout->addStretch();
	size = size.expandedTo(mEndDateButton->sizeHint());

	// Line up the widgets to the right of the radio buttons
	mRepeatCountButton->setFixedSize(size);
	mEndDateButton->setFixedSize(size);

	// Create the exceptions group which specifies dates to be excluded
	// from the recurrence.

	mExceptionGroup = new Q3GroupBox(i18n("E&xceptions"), this, "mExceptionGroup");
	topLayout->addWidget(mExceptionGroup);
	topLayout->setStretchFactor(mExceptionGroup, 2);
	vlayout = new QVBoxLayout(mExceptionGroup, KDialog::marginHint(), KDialog::spacingHint());
	vlayout->addSpacing(fontMetrics().lineSpacing()/2);
	layout = new QHBoxLayout(vlayout, KDialog::spacingHint());
	vlayout = new QVBoxLayout(layout);

	mExceptionDateList = new Q3ListBox(mExceptionGroup);
	mExceptionDateList->setSizePolicy(QSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding));
	connect(mExceptionDateList, SIGNAL(selectionChanged()), SLOT(enableExceptionButtons()));
	Q3WhatsThis::add(mExceptionDateList,
	      i18n("The list of exceptions, i.e. dates/times excluded from the recurrence"));
	vlayout->addWidget(mExceptionDateList);

	if (mReadOnly)
	{
		mExceptionDateEdit     = 0;
		mChangeExceptionButton = 0;
		mDeleteExceptionButton = 0;
	}
	else
	{
		vlayout = new QVBoxLayout(layout);
		mExceptionDateEdit = new DateEdit(mExceptionGroup);
		mExceptionDateEdit->setFixedSize(mExceptionDateEdit->sizeHint());
		mExceptionDateEdit->setDate(QDate::currentDate());
		Q3WhatsThis::add(mExceptionDateEdit,
		      i18n("Enter a date to insert in the exceptions list. "
		           "Use in conjunction with the Add or Change button below."));
		vlayout->addWidget(mExceptionDateEdit);

		layout = new QHBoxLayout(vlayout, KDialog::spacingHint());
		QPushButton* button = new QPushButton(i18n("Add"), mExceptionGroup);
		button->setFixedSize(button->sizeHint());
		connect(button, SIGNAL(clicked()), SLOT(addException()));
		Q3WhatsThis::add(button,
		      i18n("Add the date entered above to the exceptions list"));
		layout->addWidget(button);

		mChangeExceptionButton = new QPushButton(i18n("Change"), mExceptionGroup);
		mChangeExceptionButton->setFixedSize(mChangeExceptionButton->sizeHint());
		connect(mChangeExceptionButton, SIGNAL(clicked()), SLOT(changeException()));
		Q3WhatsThis::add(mChangeExceptionButton,
		      i18n("Replace the currently highlighted item in the exceptions list with the date entered above"));
		layout->addWidget(mChangeExceptionButton);

		mDeleteExceptionButton = new QPushButton(i18n("Delete"), mExceptionGroup);
		mDeleteExceptionButton->setFixedSize(mDeleteExceptionButton->sizeHint());
		connect(mDeleteExceptionButton, SIGNAL(clicked()), SLOT(deleteException()));
		Q3WhatsThis::add(mDeleteExceptionButton,
		      i18n("Remove the currently highlighted item from the exceptions list"));
		layout->addWidget(mDeleteExceptionButton);
	}

	noEmitTypeChanged = false;
}

/******************************************************************************
 * Verify the consistency of the entered data.
 * Reply = widget to receive focus on error, or 0 if no error.
 */
QWidget* RecurrenceEdit::checkData(const QDateTime& startDateTime, QString& errorMessage) const
{
	if (mAtLoginButton->isOn())
		return 0;
	const_cast<RecurrenceEdit*>(this)->mCurrStartDateTime = startDateTime;
	if (mEndDateButton->isChecked())
	{
		QWidget* errWidget = 0;
		bool noTime = !mEndTimeEdit->isEnabled();
		QDate endDate = mEndDateEdit->date();
		if (endDate < startDateTime.date())
			errWidget = mEndDateEdit;
		else if (!noTime  &&  QDateTime(endDate, mEndTimeEdit->time()) < startDateTime)
			errWidget = mEndTimeEdit;
		if (errWidget)
		{
			errorMessage = noTime
			             ? i18n("End date is earlier than start date")
			             : i18n("End date/time is earlier than start date/time");
			return errWidget;
		}
	}
	if (!mRule)
		return 0;
	return mRule->validate(errorMessage);
}

/******************************************************************************
 * Called when a recurrence period radio button is clicked.
 */
void RecurrenceEdit::periodClicked(QAbstractButton* button)
{
	RepeatType oldType = mRuleButtonType;
	bool none     = (button == mNoneButton);
	bool atLogin  = (button == mAtLoginButton);
	bool subdaily = (button == mSubDailyButton);
	if (none)
	{
		mRule = 0;
		mRuleButtonType = NO_RECUR;
	}
	else if (atLogin)
	{
		mRule = 0;
		mRuleButtonType = AT_LOGIN;
		mEndDateButton->setChecked(true);
	}
	else if (subdaily)
	{
		mRule = mSubDailyRule;
		mRuleButtonType = SUBDAILY;
	}
	else if (button == mDailyButton)
	{
		mRule = mDailyRule;
		mRuleButtonType = DAILY;
		mDailyShown = true;
	}
	else if (button == mWeeklyButton)
	{
		mRule = mWeeklyRule;
		mRuleButtonType = WEEKLY;
		mWeeklyShown = true;
	}
	else if (button == mMonthlyButton)
	{
		mRule = mMonthlyRule;
		mRuleButtonType = MONTHLY;
		mMonthlyShown = true;
	}
	else if (button == mYearlyButton)
	{
		mRule = mYearlyRule;
		mRuleButtonType = ANNUAL;
		mYearlyShown = true;
	}
	else
		return;

	if (mRuleButtonType != oldType)
	{
		ruleStack->raiseWidget(mRule ? mRule : mNoRule);
		if (oldType == NO_RECUR  ||  none)
			mRangeButtonBox->setEnabled(!none);
		mExceptionGroup->setEnabled(!(none || atLogin));
		mEndAnyTimeCheckBox->setEnabled(atLogin);
		if (!none)
		{
			mNoEndDateButton->setEnabled(!atLogin);
			mRepeatCountButton->setEnabled(!atLogin);
		}
		rangeTypeClicked();
		if (!noEmitTypeChanged)
			emit typeChanged(mRuleButtonType);
	}
}

void RecurrenceEdit::slotAnyTimeToggled(bool on)
{
	QAbstractButton* button = ruleButtonGroup->checkedButton();
	mEndTimeEdit->setEnabled(button == mAtLoginButton && !on
	                     ||  button == mSubDailyButton && mEndDateButton->isChecked());
}

/******************************************************************************
 * Called when a recurrence range type radio button is clicked.
 */
void RecurrenceEdit::rangeTypeClicked()
{
	bool endDate = mEndDateButton->isOn();
	mEndDateEdit->setEnabled(endDate);
	mEndTimeEdit->setEnabled(endDate
	                         &&  (mAtLoginButton->isOn() && !mEndAnyTimeCheckBox->isChecked()
	                              ||  mSubDailyButton->isOn()));
	bool repeatCount = mRepeatCountButton->isOn();
	mRepeatCountEntry->setEnabled(repeatCount);
	mRepeatCountLabel->setEnabled(repeatCount);
}

void RecurrenceEdit::showEvent(QShowEvent*)
{
	if (mRule)
		mRule->setFrequencyFocus();
	else
		ruleButtonGroup->checkedButton()->setFocus();
	emit shown();
}

/******************************************************************************
 * Called when the value of the repeat count field changes, to reset the
 * minimum value to 1 if the value was 0.
 */
void RecurrenceEdit::repeatCountChanged(int value)
{
	if (value > 0  &&  mRepeatCountEntry->minValue() == 0)
		mRepeatCountEntry->setMinValue(1);
}

/******************************************************************************
 * Add the date entered in the exception date edit control to the list of
 * exception dates.
 */
void RecurrenceEdit::addException()
{
	if (!mExceptionDateEdit  ||  !mExceptionDateEdit->isValid())
		return;
	QDate date = mExceptionDateEdit->date();
	DateList::Iterator it;
	int index = 0;
	bool insert = true;
	for (it = mExceptionDates.begin();  it != mExceptionDates.end();  ++index, ++it)
	{
		if (date <= *it)
		{
			insert = (date != *it);
			break;
		}
	}
	if (insert)
	{
		mExceptionDates.insert(it, date);
		mExceptionDateList->insertItem(KGlobal::locale()->formatDate(date), index);
	}
	mExceptionDateList->setCurrentItem(index);
	enableExceptionButtons();
}

/******************************************************************************
 * Change the currently highlighted exception date to that entered in the
 * exception date edit control.
 */
void RecurrenceEdit::changeException()
{
	if (!mExceptionDateEdit  ||  !mExceptionDateEdit->isValid())
		return;
	int index = mExceptionDateList->currentItem();
	if (index >= 0  &&  mExceptionDateList->isSelected(index))
	{
		QDate olddate = mExceptionDates[index];
		QDate newdate = mExceptionDateEdit->date();
		if (newdate != olddate)
		{
			mExceptionDates.remove(mExceptionDates.at(index));
			mExceptionDateList->removeItem(index);
			addException();
		}
	}
}

/******************************************************************************
 * Delete the currently highlighted exception date.
 */
void RecurrenceEdit::deleteException()
{
	int index = mExceptionDateList->currentItem();
	if (index >= 0  &&  mExceptionDateList->isSelected(index))
	{
		mExceptionDates.remove(mExceptionDates.at(index));
		mExceptionDateList->removeItem(index);
		enableExceptionButtons();
	}
}

/******************************************************************************
 * Enable/disable the exception group buttons according to whether any item is
 * selected in the exceptions listbox.
 */
void RecurrenceEdit::enableExceptionButtons()
{
	int index = mExceptionDateList->currentItem();
	bool enable = (index >= 0  &&  mExceptionDateList->isSelected(index));
	if (mDeleteExceptionButton)
		mDeleteExceptionButton->setEnabled(enable);
	if (mChangeExceptionButton)
		mChangeExceptionButton->setEnabled(enable);

	// Prevent the exceptions list box receiving keyboard focus is it's empty
	mExceptionDateList->setFocusPolicy(mExceptionDateList->count() ? Qt::WheelFocus : Qt::NoFocus);
}

/******************************************************************************
 * Notify this instance of a change in the alarm start date.
 */
void RecurrenceEdit::setStartDate(const QDate& start, const QDate& today)
{
	if (!mReadOnly)
	{
		setRuleDefaults(start);
		if (start < today)
		{
			mEndDateEdit->setMinDate(today);
			if (mExceptionDateEdit)
				mExceptionDateEdit->setMinDate(today);
		}
		else
		{
			const QString startString = i18n("Date cannot be earlier than start date", "start date");
			mEndDateEdit->setMinDate(start, startString);
			if (mExceptionDateEdit)
				mExceptionDateEdit->setMinDate(start, startString);
		}
	}
}

/******************************************************************************
 * Specify the default recurrence end date.
 */
void RecurrenceEdit::setDefaultEndDate(const QDate& end)
{
	if (!mEndDateButton->isOn())
		mEndDateEdit->setDate(end);
}

void RecurrenceEdit::setEndDateTime(const DateTime& end)
{
	mEndDateEdit->setDate(end.date());
	mEndTimeEdit->setValue(end.time());
	mEndTimeEdit->setEnabled(!end.isDateOnly());
	mEndAnyTimeCheckBox->setChecked(end.isDateOnly());
}

DateTime RecurrenceEdit::endDateTime() const
{
	if (ruleButtonGroup->checkedButton() == mAtLoginButton  &&  mEndAnyTimeCheckBox->isChecked())
		return DateTime(mEndDateEdit->date());
	return DateTime(mEndDateEdit->date(), mEndTimeEdit->time());
}

/******************************************************************************
 * Set all controls to their default values.
 */
void RecurrenceEdit::setDefaults(const QDateTime& from)
{
	mCurrStartDateTime = from;
	QDate fromDate = from.date();
	mNoEndDateButton->setChecked(true);

	mSubDailyRule->setFrequency(1);
	mDailyRule->setFrequency(1);
	mWeeklyRule->setFrequency(1);
	mMonthlyRule->setFrequency(1);
	mYearlyRule->setFrequency(1);

	setRuleDefaults(fromDate);
	mMonthlyRule->setType(MonthYearRule::DATE);   // date in month
	mYearlyRule->setType(MonthYearRule::DATE);    // date in year

	mEndDateEdit->setDate(fromDate);

	noEmitTypeChanged = true;
	RadioButton* button;
	switch (Preferences::defaultRecurPeriod())
	{
		case AT_LOGIN: button = mAtLoginButton;  break;
		case ANNUAL:   button = mYearlyButton;   break;
		case MONTHLY:  button = mMonthlyButton;  break;
		case WEEKLY:   button = mWeeklyButton;   break;
		case DAILY:    button = mDailyButton;    break;
		case SUBDAILY: button = mSubDailyButton; break;
		case NO_RECUR:
		default:       button = mNoneButton;     break;
	}
	button->setChecked(true);
	noEmitTypeChanged = false;
	rangeTypeClicked();
	enableExceptionButtons();

	saveState();
}

/******************************************************************************
 * Set the controls for weekly, monthly and yearly rules which have not so far
 * been shown, to their default values, depending on the recurrence start date.
 */
void RecurrenceEdit::setRuleDefaults(const QDate& fromDate)
{
	int day       = fromDate.day();
	int dayOfWeek = fromDate.dayOfWeek();
	int month     = fromDate.month();
	if (!mDailyShown)
		mDailyRule->setDays(true);
	if (!mWeeklyShown)
		mWeeklyRule->setDay(dayOfWeek);
	if (!mMonthlyShown)
		mMonthlyRule->setDefaultValues(day, dayOfWeek);
	if (!mYearlyShown)
		mYearlyRule->setDefaultValues(day, dayOfWeek, month);
}

/******************************************************************************
 * Set the state of all controls to reflect the data in the specified event.
 */
void RecurrenceEdit::set(const KAEvent& event)
{
	setDefaults(event.mainDateTime().dateTime());
	if (event.repeatAtLogin())
	{
		mAtLoginButton->setChecked(true);
		mEndDateButton->setChecked(true);
		return;
	}
	mNoneButton->setChecked(true);
	int repeatDuration;
	KARecurrence* recurrence = event.recurrence();
	if (!recurrence)
		return;
	KARecurrence::Type rtype = recurrence->type();
	switch (rtype)
	{
		case KARecurrence::MINUTELY:
			mSubDailyButton->setChecked(true);
			break;

		case KARecurrence::DAILY:
		{
			mDailyButton->setChecked(true);
			QBitArray rDays = recurrence->days();
			bool set = false;
			for (int i = 0;  i < 7 && !set;  ++i)
				set = rDays.testBit(i);
			if (set)
				mDailyRule->setDays(rDays);
			else
				mDailyRule->setDays(true);
			break;
		}
		case KARecurrence::WEEKLY:
		{
			mWeeklyButton->setChecked(true);
			QBitArray rDays = recurrence->days();
			mWeeklyRule->setDays(rDays);
			break;
		}
		case KARecurrence::MONTHLY_POS:    // on nth (Tuesday) of the month
		{
			Q3ValueList<RecurrenceRule::WDayPos> posns = recurrence->monthPositions();
			int i = posns.first().pos();
			if (!i)
			{
				// It's every (Tuesday) of the month. Convert to a weekly recurrence
				// (but ignoring any non-every xxxDay positions).
				mWeeklyButton->setChecked(true);
				mWeeklyRule->setFrequency(recurrence->frequency());
				QBitArray rDays(7);
				for (Q3ValueList<RecurrenceRule::WDayPos>::ConstIterator it = posns.begin();  it != posns.end();  ++it)
				{
					if (!(*it).pos())
						rDays.setBit((*it).day() - 1, 1);
				}
				mWeeklyRule->setDays(rDays);
				break;
			}
			mMonthlyButton->setChecked(true);
			mMonthlyRule->setPosition(i, posns.first().day());
			break;
		}
		case KARecurrence::MONTHLY_DAY:     // on nth day of the month
		{
			mMonthlyButton->setChecked(true);
			Q3ValueList<int> rmd = recurrence->monthDays();
			int day = (rmd.isEmpty()) ? event.mainDate().day() : rmd.first();
			mMonthlyRule->setDate(day);
			break;
		}
		case KARecurrence::ANNUAL_DATE:   // on the nth day of (months...) in the year
		case KARecurrence::ANNUAL_POS:     // on the nth (Tuesday) of (months...) in the year
		{
			if (rtype == KARecurrence::ANNUAL_DATE)
			{
				mYearlyButton->setChecked(true);
				const Q3ValueList<int> rmd = recurrence->monthDays();
				int day = (rmd.isEmpty()) ? event.mainDate().day() : rmd.first();
				mYearlyRule->setDate(day);
				mYearlyRule->setFeb29Type(recurrence->feb29Type());
			}
			else if (rtype == KARecurrence::ANNUAL_POS)
			{
				mYearlyButton->setChecked(true);
				Q3ValueList<RecurrenceRule::WDayPos> posns = recurrence->yearPositions();
				mYearlyRule->setPosition(posns.first().pos(), posns.first().day());
			}
			mYearlyRule->setMonths(recurrence->yearMonths());
			break;
		}
		default:
			return;
	}

	mRule->setFrequency(recurrence->frequency());
	repeatDuration = event.remainingRecurrences();

	// Get range information
	QDateTime endtime = mCurrStartDateTime;
	if (repeatDuration == -1)
		mNoEndDateButton->setChecked(true);
	else if (repeatDuration)
	{
		mRepeatCountButton->setChecked(true);
		if (event.mainExpired())
		{
			mRepeatCountEntry->setMinValue(0);
			repeatDuration = 0;
		}
		mRepeatCountEntry->setValue(repeatDuration);
	}
	else
	{
		mEndDateButton->setChecked(true);
		endtime = recurrence->endDateTime();
		mEndTimeEdit->setValue(endtime.time());
	}
	mEndDateEdit->setDate(endtime.date());

	// Get exception information
	mExceptionDates = event.recurrence()->exDates();
	qHeapSort(mExceptionDates);
	mExceptionDateList->clear();
	for (DateList::ConstIterator it = mExceptionDates.begin();  it != mExceptionDates.end();  ++it)
		mExceptionDateList->insertItem(KGlobal::locale()->formatDate(*it));
	enableExceptionButtons();

	rangeTypeClicked();

	saveState();
}

/******************************************************************************
 * Update the specified KAEvent with the entered recurrence data.
 * If 'adjustStart' is true, the start date/time will be adjusted if necessary
 * to be the first date/time which recurs on or after the original start.
 */
void RecurrenceEdit::updateEvent(KAEvent& event, bool adjustStart)
{
	// Get end date and repeat count, common to all types of recurring events
	QDate  endDate;
	QTime  endTime;
	int    repeatCount;
	if (mNoEndDateButton->isChecked())
		repeatCount = -1;
	else if (mRepeatCountButton->isChecked())
		repeatCount = mRepeatCountEntry->value();
	else
	{
		repeatCount = 0;
		endDate = mEndDateEdit->date();
		endTime = mEndTimeEdit->time();
	}

	// Set up the recurrence according to the type selected
	QAbstractButton* button = ruleButtonGroup->checkedButton();
	event.setRepeatAtLogin(button == mAtLoginButton);
	int frequency = mRule ? mRule->frequency() : 0;
	if (button == mSubDailyButton)
	{
		QDateTime endDateTime(endDate, endTime);
		event.setRecurMinutely(frequency, repeatCount, endDateTime);
	}
	else if (button == mDailyButton)
	{
		event.setRecurDaily(frequency, mDailyRule->days(), repeatCount, endDate);
	}
	else if (button == mWeeklyButton)
	{
		event.setRecurWeekly(frequency, mWeeklyRule->days(), repeatCount, endDate);
	}
	else if (button == mMonthlyButton)
	{
		if (mMonthlyRule->type() == MonthlyRule::POS)
		{
			// It's by position
			KAEvent::MonthPos pos;
			pos.days.fill(false);
			pos.days.setBit(mMonthlyRule->dayOfWeek() - 1);
			pos.weeknum = mMonthlyRule->week();
			Q3ValueList<KAEvent::MonthPos> poses;
			poses.append(pos);
			event.setRecurMonthlyByPos(frequency, poses, repeatCount, endDate);
		}
		else
		{
			// It's by day
			int daynum = mMonthlyRule->date();
			Q3ValueList<int> daynums;
			daynums.append(daynum);
			event.setRecurMonthlyByDate(frequency, daynums, repeatCount, endDate);
		}
	}
	else if (button == mYearlyButton)
	{
		Q3ValueList<int> months = mYearlyRule->months();
		if (mYearlyRule->type() == YearlyRule::POS)
		{
			// It's by position
			KAEvent::MonthPos pos;
			pos.days.fill(false);
			pos.days.setBit(mYearlyRule->dayOfWeek() - 1);
			pos.weeknum = mYearlyRule->week();
			Q3ValueList<KAEvent::MonthPos> poses;
			poses.append(pos);
			event.setRecurAnnualByPos(frequency, poses, months, repeatCount, endDate);
		}
		else
		{
			// It's by date in month
			event.setRecurAnnualByDate(frequency, months, mYearlyRule->date(),
			                           mYearlyRule->feb29Type(), repeatCount, endDate);
		}
	}
	else
	{
		event.setNoRecur();
		return;
	}
	if (adjustStart)
		event.setFirstRecurrence();

	// Set up exceptions
	event.recurrence()->setExDates(mExceptionDates);
	event.setUpdated();
}

/******************************************************************************
 * Save the state of all controls.
 */
void RecurrenceEdit::saveState()
{
	mSavedRuleButton = ruleButtonGroup->checkedButton();
	if (mRule)
		mRule->saveState();
	mSavedRangeButton = mRangeButtonGroup->checkedButton();
	if (mSavedRangeButton == mRepeatCountButton)
		mSavedRepeatCount = mRepeatCountEntry->value();
	else if (mSavedRangeButton == mEndDateButton)
		mSavedEndDateTime.set(QDateTime(mEndDateEdit->date(), mEndTimeEdit->time()), mEndAnyTimeCheckBox->isChecked());
	mSavedExceptionDates = mExceptionDates;
}

/******************************************************************************
 * Check whether any of the controls have changed state since initialisation.
 */
bool RecurrenceEdit::stateChanged() const
{
	if (mSavedRuleButton  != ruleButtonGroup->checkedButton()
	||  mSavedRangeButton != mRangeButtonGroup->checkedButton()
	||  mRule  &&  mRule->stateChanged())
		return true;
	if (mSavedRangeButton == mRepeatCountButton
	&&  mSavedRepeatCount != mRepeatCountEntry->value())
		return true;
	if (mSavedRangeButton == mEndDateButton
	&&  mSavedEndDateTime != DateTime(QDateTime(mEndDateEdit->date(), mEndTimeEdit->time()), mEndAnyTimeCheckBox->isChecked()))
		return true;
	if (mSavedExceptionDates != mExceptionDates)
		return true;
	return false;
}



/*=============================================================================
= Class Rule
= Base class for rule widgets, including recurrence frequency.
=============================================================================*/

Rule::Rule(const QString& freqText, const QString& freqWhatsThis, bool time, bool readOnly, QWidget* parent, const char* name)
	: NoRule(parent, name)
{
	mLayout = new QVBoxLayout(this, 0, KDialog::spacingHint());
	Q3HBox* freqBox = new Q3HBox(this);
	mLayout->addWidget(freqBox);
	Q3HBox* box = new Q3HBox(freqBox);    // this is to control the QWhatsThis text display area
	box->setSpacing(KDialog::spacingHint());

	QLabel* label = new QLabel(i18n("Recur e&very"), box);
	label->setFixedSize(label->sizeHint());
	if (time)
	{
		mIntSpinBox = 0;
		mSpinBox = mTimeSpinBox = new TimeSpinBox(1, 5999, box);
		mTimeSpinBox->setFixedSize(mTimeSpinBox->sizeHint());
		mTimeSpinBox->setReadOnly(readOnly);
	}
	else
	{
		mTimeSpinBox = 0;
		mSpinBox = mIntSpinBox = new SpinBox(1, 999, 1, box);
		mIntSpinBox->setFixedSize(mIntSpinBox->sizeHint());
		mIntSpinBox->setReadOnly(readOnly);
	}
	connect(mSpinBox, SIGNAL(valueChanged(int)), SIGNAL(frequencyChanged()));
	label->setBuddy(mSpinBox);
	label = new QLabel(freqText, box);
	label->setFixedSize(label->sizeHint());
	box->setFixedSize(sizeHint());
	Q3WhatsThis::add(box, freqWhatsThis);

	new QWidget(freqBox);     // left adjust the visible widgets
	freqBox->setFixedHeight(freqBox->sizeHint().height());
	freqBox->setFocusProxy(mSpinBox);
}

int Rule::frequency() const
{
	if (mIntSpinBox)
		return mIntSpinBox->value();
	if (mTimeSpinBox)
		return mTimeSpinBox->value();
	return 0;
}

void Rule::setFrequency(int n)
{
	if (mIntSpinBox)
		mIntSpinBox->setValue(n);
	if (mTimeSpinBox)
		mTimeSpinBox->setValue(n);
}

/******************************************************************************
 * Save the state of all controls.
 */
void Rule::saveState()
{
	mSavedFrequency = frequency();
}

/******************************************************************************
 * Check whether any of the controls have changed state since initialisation.
 */
bool Rule::stateChanged() const
{
	return (mSavedFrequency != frequency());
}


/*=============================================================================
= Class SubDailyRule
= Sub-daily rule widget.
=============================================================================*/

SubDailyRule::SubDailyRule(bool readOnly, QWidget* parent, const char* name)
	: Rule(i18n("hours:minutes"),
	       i18n("Enter the number of hours and minutes between repetitions of the alarm"),
	       true, readOnly, parent, name)
{ }


/*=============================================================================
= Class DayWeekRule
= Daily/weekly rule widget base class.
=============================================================================*/

DayWeekRule::DayWeekRule(const QString& freqText, const QString& freqWhatsThis, const QString& daysWhatsThis,
                         bool readOnly, QWidget* parent, const char* name)
	: Rule(freqText, freqWhatsThis, false, readOnly, parent, name),
	  mSavedDays(7)
{
	QGridLayout* grid = new QGridLayout(layout(), 1, 4, KDialog::spacingHint());
	grid->setRowStretch(0, 1);

	QLabel* label = new QLabel(i18n("On: Tuesday", "O&n:"), this);
	label->setFixedSize(label->sizeHint());
	grid->addWidget(label, 0, 0, Qt::AlignRight | Qt::AlignTop);
	grid->addColSpacing(1, KDialog::spacingHint());

	// List the days of the week starting at the user's start day of the week.
	// Save the first day of the week, just in case it changes while the dialog is open.
	QWidget* box = new QWidget(this);   // this is to control the QWhatsThis text display area
	QGridLayout* dgrid = new QGridLayout(box, 4, 2, 0, KDialog::spacingHint());
	const KCalendarSystem* calendar = KGlobal::locale()->calendar();
	for (int i = 0;  i < 7;  ++i)
	{
		int day = KAlarm::localeDayInWeek_to_weekDay(i);
		mDayBox[i] = new CheckBox(calendar->weekDayName(day), box);
		mDayBox[i]->setFixedSize(mDayBox[i]->sizeHint());
		mDayBox[i]->setReadOnly(readOnly);
		dgrid->addWidget(mDayBox[i], i%4, i/4, Qt::AlignLeft);
	}
	box->setFixedSize(box->sizeHint());
	Q3WhatsThis::add(box, daysWhatsThis);
	grid->addWidget(box, 0, 2, Qt::AlignLeft);
	label->setBuddy(mDayBox[0]);
	grid->setColStretch(3, 1);
}

/******************************************************************************
 * Fetch which days of the week have been ticked.
 */
QBitArray DayWeekRule::days() const
{
	QBitArray ds(7);
	ds.fill(false);
	for (int i = 0;  i < 7;  ++i)
		if (mDayBox[i]->isChecked())
			ds.setBit(KAlarm::localeDayInWeek_to_weekDay(i) - 1, 1);
	return ds;
}

/******************************************************************************
 * Tick/untick every day of the week.
 */
void DayWeekRule::setDays(bool tick)
{
	for (int i = 0;  i < 7;  ++i)
		mDayBox[i]->setChecked(tick);
}

/******************************************************************************
 * Tick/untick each day of the week according to the specified bits.
 */
void DayWeekRule::setDays(QBitArray& days)
{
	for (int i = 0;  i < 7;  ++i)
	{
		bool x = days.testBit(KAlarm::localeDayInWeek_to_weekDay(i) - 1);
		mDayBox[i]->setChecked(x);
	}
}

/******************************************************************************
 * Tick the specified day of the week, and untick all other days.
 */
void DayWeekRule::setDay(int dayOfWeek)
{
	for (int i = 0;  i < 7;  ++i)
		mDayBox[i]->setChecked(false);
	if (dayOfWeek > 0  &&  dayOfWeek <= 7)
		mDayBox[KAlarm::weekDay_to_localeDayInWeek(dayOfWeek)]->setChecked(true);
}

/******************************************************************************
 * Validate: check that at least one day is selected.
 */
QWidget* DayWeekRule::validate(QString& errorMessage)
{
	for (int i = 0;  i < 7;  ++i)
		if (mDayBox[i]->isChecked())
			return 0;
	errorMessage = i18n("No day selected");
	return mDayBox[0];
}

/******************************************************************************
 * Save the state of all controls.
 */
void DayWeekRule::saveState()
{
	Rule::saveState();
	mSavedDays = days();
}

/******************************************************************************
 * Check whether any of the controls have changed state since initialisation.
 */
bool DayWeekRule::stateChanged() const
{
	return (Rule::stateChanged()
	    ||  mSavedDays != days());
}


/*=============================================================================
= Class DailyRule
= Daily rule widget.
=============================================================================*/

DailyRule::DailyRule(bool readOnly, QWidget* parent, const char* name)
	: DayWeekRule(i18n("day(s)"),
	              i18n("Enter the number of days between repetitions of the alarm"),
	              i18n("Select the days of the week on which the alarm is allowed to occur"),
	              readOnly, parent, name)
{ }


/*=============================================================================
= Class WeeklyRule
= Weekly rule widget.
=============================================================================*/

WeeklyRule::WeeklyRule(bool readOnly, QWidget* parent, const char* name)
	: DayWeekRule(i18n("week(s)"),
	              i18n("Enter the number of weeks between repetitions of the alarm"),
	              i18n("Select the days of the week on which to repeat the alarm"),
	              readOnly, parent, name)
{ }


/*=============================================================================
= Class MonthYearRule
= Monthly/yearly rule widget base class.
=============================================================================*/

MonthYearRule::MonthYearRule(const QString& freqText, const QString& freqWhatsThis, bool allowEveryWeek,
                             bool readOnly, QWidget* parent, const char* name)
	: Rule(freqText, freqWhatsThis, false, readOnly, parent, name),
	  mEveryWeek(allowEveryWeek)
{
	mButtonGroup = new ButtonGroup(this);

	// Month day selector
	Q3HBox* box = new Q3HBox(this);
	box->setSpacing(KDialog::spacingHint());
	layout()->addWidget(box);

	mDayButton = new RadioButton(i18n("On day number in the month", "O&n day"), box);
	mDayButton->setFixedSize(mDayButton->sizeHint());
	mDayButton->setReadOnly(readOnly);
	mButtonGroup->addButton(mDayButton);
	Q3WhatsThis::add(mDayButton, i18n("Repeat the alarm on the selected day of the month"));

	mDayCombo = new ComboBox(false, box);
	mDayCombo->setMaxVisibleItems(11);
	for (int i = 0;  i < 31;  ++i)
		mDayCombo->insertItem(QString::number(i + 1));
	mDayCombo->insertItem(i18n("Last day of month", "Last"));
	mDayCombo->setFixedSize(mDayCombo->sizeHint());
	mDayCombo->setReadOnly(readOnly);
	Q3WhatsThis::add(mDayCombo, i18n("Select the day of the month on which to repeat the alarm"));
	mDayButton->setFocusWidget(mDayCombo);
	connect(mDayCombo, SIGNAL(activated(int)), SLOT(slotDaySelected(int)));

	box->setStretchFactor(new QWidget(box), 1);    // left adjust the controls
	box->setFixedHeight(box->sizeHint().height());

	// Month position selector
	box = new Q3HBox(this);
	box->setSpacing(KDialog::spacingHint());
	layout()->addWidget(box);

	mPosButton = new RadioButton(i18n("On the 1st Tuesday", "On t&he"), box);
	mPosButton->setFixedSize(mPosButton->sizeHint());
	mPosButton->setReadOnly(readOnly);
	mButtonGroup->addButton(mPosButton);
	Q3WhatsThis::add(mPosButton,
	      i18n("Repeat the alarm on one day of the week, in the selected week of the month"));

	mWeekCombo = new ComboBox(false, box);
	mWeekCombo->insertItem(i18n("1st"));
	mWeekCombo->insertItem(i18n("2nd"));
	mWeekCombo->insertItem(i18n("3rd"));
	mWeekCombo->insertItem(i18n("4th"));
	mWeekCombo->insertItem(i18n("5th"));
	mWeekCombo->insertItem(i18n("Last Monday in March", "Last"));
	mWeekCombo->insertItem(i18n("2nd Last"));
	mWeekCombo->insertItem(i18n("3rd Last"));
	mWeekCombo->insertItem(i18n("4th Last"));
	mWeekCombo->insertItem(i18n("5th Last"));
	if (mEveryWeek)
	{
		mWeekCombo->insertItem(i18n("Every (Monday...) in month", "Every"));
		mWeekCombo->setMaxVisibleItems(11);
	}
	Q3WhatsThis::add(mWeekCombo, i18n("Select the week of the month in which to repeat the alarm"));
	mWeekCombo->setFixedSize(mWeekCombo->sizeHint());
	mWeekCombo->setReadOnly(readOnly);
	mPosButton->setFocusWidget(mWeekCombo);

	mDayOfWeekCombo = new ComboBox(false, box);
	const KCalendarSystem* calendar = KGlobal::locale()->calendar();
	for (int i = 0;  i < 7;  ++i)
	{
		int day = KAlarm::localeDayInWeek_to_weekDay(i);
		mDayOfWeekCombo->insertItem(calendar->weekDayName(day));
	}
	mDayOfWeekCombo->setReadOnly(readOnly);
	Q3WhatsThis::add(mDayOfWeekCombo, i18n("Select the day of the week on which to repeat the alarm"));

	box->setStretchFactor(new QWidget(box), 1);    // left adjust the controls
	box->setFixedHeight(box->sizeHint().height());
	connect(mButtonGroup, SIGNAL(buttonSet(QAbstractButton*)), SLOT(clicked(QAbstractButton*)));
}

MonthYearRule::DayPosType MonthYearRule::type() const
{
	return (mButtonGroup->checkedButton() == mDayButton) ? DATE : POS;
}

void MonthYearRule::setType(MonthYearRule::DayPosType type)
{
	if (type == DATE)
		mDayButton->setChecked(true);
	else
		mPosButton->setChecked(true);
}

void MonthYearRule::setDefaultValues(int dayOfMonth, int dayOfWeek)
{
	--dayOfMonth;
	mDayCombo->setCurrentItem(dayOfMonth);
	mWeekCombo->setCurrentItem(dayOfMonth / 7);
	mDayOfWeekCombo->setCurrentItem(KAlarm::weekDay_to_localeDayInWeek(dayOfWeek));
}

int MonthYearRule::date() const
{
	int daynum  = mDayCombo->currentItem() + 1;
	return (daynum <= 31) ? daynum : 31 - daynum;
}

int MonthYearRule::week() const
{
	int weeknum = mWeekCombo->currentItem() + 1;
	return (weeknum <= 5) ? weeknum : (weeknum == 11) ? 0 : 5 - weeknum;
}

int MonthYearRule::dayOfWeek() const
{
	return KAlarm::localeDayInWeek_to_weekDay(mDayOfWeekCombo->currentItem());
}

void MonthYearRule::setDate(int dayOfMonth)
{
	mDayButton->setChecked(true);;
	mDayCombo->setCurrentItem(dayOfMonth > 0 ? dayOfMonth - 1 : dayOfMonth < 0 ? 30 - dayOfMonth : 0);   // day 0 shouldn't ever occur
}

void MonthYearRule::setPosition(int week, int dayOfWeek)
{
	mPosButton->setChecked(true);
	mWeekCombo->setCurrentItem((week > 0) ? week - 1 : (week < 0) ? 4 - week : mEveryWeek ? 10 : 0);
	mDayOfWeekCombo->setCurrentItem(KAlarm::weekDay_to_localeDayInWeek(dayOfWeek));
}

void MonthYearRule::enableSelection(DayPosType type)
{
	bool date = (type == DATE);
	mDayCombo->setEnabled(date);
	mWeekCombo->setEnabled(!date);
	mDayOfWeekCombo->setEnabled(!date);
}

void MonthYearRule::clicked(QAbstractButton* button)
{
	enableSelection(button == mDayButton ? DATE : POS);
}

void MonthYearRule::slotDaySelected(int index)
{
	daySelected(index <= 30 ? index + 1 : 30 - index);
}

/******************************************************************************
 * Save the state of all controls.
 */
void MonthYearRule::saveState()
{
	Rule::saveState();
	mSavedType = type();
	if (mSavedType == DATE)
		mSavedDay = date();
	else
	{
		mSavedWeek    = week();
		mSavedWeekDay = dayOfWeek();
	}
}

/******************************************************************************
 * Check whether any of the controls have changed state since initialisation.
 */
bool MonthYearRule::stateChanged() const
{
	if (Rule::stateChanged()
	||  mSavedType != type())
		return true;
	if (mSavedType == DATE)
	{
		if (mSavedDay != date())
			return true;
	}
	else
	{
		if (mSavedWeek    != week()
		||  mSavedWeekDay != dayOfWeek())
			return true;
	}
	return false;
}


/*=============================================================================
= Class MonthlyRule
= Monthly rule widget.
=============================================================================*/

MonthlyRule::MonthlyRule(bool readOnly, QWidget* parent, const char* name)
	: MonthYearRule(i18n("month(s)"),
	       i18n("Enter the number of months between repetitions of the alarm"),
	       false, readOnly, parent, name)
{ }


/*=============================================================================
= Class YearlyRule
= Yearly rule widget.
=============================================================================*/

YearlyRule::YearlyRule(bool readOnly, QWidget* parent, const char* name)
	: MonthYearRule(i18n("year(s)"),
	       i18n("Enter the number of years between repetitions of the alarm"),
	       true, readOnly, parent, name)
{
	// Set up the month selection widgets
	QBoxLayout* hlayout = new QHBoxLayout(layout(), KDialog::spacingHint());
	QLabel* label = new QLabel(i18n("first week of January", "of:"), this);
	label->setFixedSize(label->sizeHint());
	hlayout->addWidget(label, 0, Qt::AlignLeft | Qt::AlignTop);

	// List the months of the year.
	QWidget* w = new QWidget(this);   // this is to control the QWhatsThis text display area
	hlayout->addWidget(w, 1, Qt::AlignLeft);
	QGridLayout* grid = new QGridLayout(w, 4, 3, 0, KDialog::spacingHint());
	const KCalendarSystem* calendar = KGlobal::locale()->calendar();
	for (int i = 0;  i < 12;  ++i)
	{
		mMonthBox[i] = new CheckBox(calendar->monthName(i + 1, 2000), w);
		mMonthBox[i]->setFixedSize(mMonthBox[i]->sizeHint());
		mMonthBox[i]->setReadOnly(readOnly);
		grid->addWidget(mMonthBox[i], i%4, i/4, Qt::AlignLeft);
	}
	connect(mMonthBox[1], SIGNAL(toggled(bool)), SLOT(enableFeb29()));
	w->setFixedHeight(w->sizeHint().height());
	Q3WhatsThis::add(w, i18n("Select the months of the year in which to repeat the alarm"));

	// February 29th handling option
	Q3HBox* f29box = new Q3HBox(this);
	layout()->addWidget(f29box);
	Q3HBox* box = new Q3HBox(f29box);    // this is to control the QWhatsThis text display area
	box->setSpacing(KDialog::spacingHint());
	mFeb29Label = new QLabel(i18n("February 2&9th alarm in non-leap years:"), box);
	mFeb29Label->setFixedSize(mFeb29Label->sizeHint());
	mFeb29Combo = new ComboBox(false, box);
	mFeb29Combo->insertItem(i18n("No date", "None"));
	mFeb29Combo->insertItem(i18n("1st March (short form)", "1 Mar"));
	mFeb29Combo->insertItem(i18n("28th February (short form)", "28 Feb"));
	mFeb29Combo->setFixedSize(mFeb29Combo->sizeHint());
	mFeb29Combo->setReadOnly(readOnly);
	mFeb29Label->setBuddy(mFeb29Combo);
	box->setFixedSize(box->sizeHint());
	Q3WhatsThis::add(box,
	      i18n("Select which date, if any, the February 29th alarm should trigger in non-leap years"));
	new QWidget(f29box);     // left adjust the visible widgets
	f29box->setFixedHeight(f29box->sizeHint().height());
}

void YearlyRule::setDefaultValues(int dayOfMonth, int dayOfWeek, int month)
{
	MonthYearRule::setDefaultValues(dayOfMonth, dayOfWeek);
	--month;
	for (int i = 0;  i < 12;  ++i)
		mMonthBox[i]->setChecked(i == month);
	setFeb29Type(Preferences::defaultFeb29Type());
	daySelected(dayOfMonth);     // enable/disable month checkboxes as appropriate
}

/******************************************************************************
 * Fetch which months have been checked (1 - 12).
 * Reply = true if February has been checked.
 */
Q3ValueList<int> YearlyRule::months() const
{
	Q3ValueList<int> mnths;
	for (int i = 0;  i < 12;  ++i)
		if (mMonthBox[i]->isChecked()  &&  mMonthBox[i]->isEnabled())
			mnths.append(i + 1);
	return mnths;
}

/******************************************************************************
 * Check/uncheck each month of the year according to the specified list.
 */
void YearlyRule::setMonths(const Q3ValueList<int>& mnths)
{
	bool checked[12];
	for (int i = 0;  i < 12;  ++i)
		checked[i] = false;
	for (Q3ValueListConstIterator<int> it = mnths.begin();  it != mnths.end();  ++it)
		checked[(*it) - 1] = true;
	for (int i = 0;  i < 12;  ++i)
		mMonthBox[i]->setChecked(checked[i]);
}

/******************************************************************************
 * Return the date for February 29th alarms in non-leap years.
 */
KARecurrence::Feb29Type YearlyRule::feb29Type() const
{
	if (mFeb29Combo->isEnabled())
	{
		switch (mFeb29Combo->currentItem())
		{
			case 1:   return KARecurrence::FEB29_MAR1;
			case 2:   return KARecurrence::FEB29_FEB28;
			default:  break;
		}
	}
	return KARecurrence::FEB29_FEB29;
}

/******************************************************************************
 * Set the date for February 29th alarms to trigger in non-leap years.
 */
void YearlyRule::setFeb29Type(KARecurrence::Feb29Type type)
{
	int index;
	switch (type)
	{
		default:
		case KARecurrence::FEB29_FEB29:  index = 0;  break;
		case KARecurrence::FEB29_MAR1:   index = 1;  break;
		case KARecurrence::FEB29_FEB28:  index = 2;  break;
	}
	mFeb29Combo->setCurrentItem(index);
}

/******************************************************************************
 * Validate: check that at least one month is selected.
 */
QWidget* YearlyRule::validate(QString& errorMessage)
{
	for (int i = 0;  i < 12;  ++i)
		if (mMonthBox[i]->isChecked()  &&  mMonthBox[i]->isEnabled())
			return 0;
	errorMessage = i18n("No month selected");
	return mMonthBox[0];
}

/******************************************************************************
 * Called when a yearly recurrence type radio button is clicked,
 * to enable/disable month checkboxes as appropriate for the date selected.
 */
void YearlyRule::clicked(QAbstractButton* button)
{
	MonthYearRule::clicked(button);
	daySelected(buttonType(button) == DATE ? date() : 1);
}

/******************************************************************************
 * Called when a day of the month is selected in a yearly recurrence, to
 * disable months for which the day is out of range.
 */
void YearlyRule::daySelected(int day)
{
	mMonthBox[1]->setEnabled(day <= 29);  // February
	bool enable = (day != 31);
	mMonthBox[3]->setEnabled(enable);     // April
	mMonthBox[5]->setEnabled(enable);     // June
	mMonthBox[8]->setEnabled(enable);     // September
	mMonthBox[10]->setEnabled(enable);    // November
	enableFeb29();
}

/******************************************************************************
 * Enable/disable the February 29th combo box depending on whether February
 * 29th is selected.
 */
void YearlyRule::enableFeb29()
{
	bool enable = (type() == DATE  &&  date() == 29  &&  mMonthBox[1]->isChecked()  &&  mMonthBox[1]->isEnabled());
	mFeb29Label->setEnabled(enable);
	mFeb29Combo->setEnabled(enable);
}

/******************************************************************************
 * Save the state of all controls.
 */
void YearlyRule::saveState()
{
	MonthYearRule::saveState();
	mSavedMonths    = months();
	mSavedFeb29Type = feb29Type();
}

/******************************************************************************
 * Check whether any of the controls have changed state since initialisation.
 */
bool YearlyRule::stateChanged() const
{
	return (MonthYearRule::stateChanged()
	    ||  mSavedMonths    != months()
	    ||  mSavedFeb29Type != feb29Type());
}
