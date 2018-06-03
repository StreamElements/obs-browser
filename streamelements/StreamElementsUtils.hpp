#pragma once

#include <functional>
#include <QTimer>
#include <QApplication>
#include <QThread>

#include <util/threading.h>

inline static void QtPostTask(void(*func)(void*), void* const data)
{
	/*
	if (QThread::currentThread() == qApp->thread()) {
		func(data);
	}
	else {*/
		QTimer *t = new QTimer();
		t->moveToThread(qApp->thread());
		t->setSingleShot(true);
		QObject::connect(t, &QTimer::timeout, [=]() {
			t->deleteLater();

			func(data);
		});
		QMetaObject::invokeMethod(t, "start", Qt::QueuedConnection, Q_ARG(int, 0));
	/*}*/
}

inline static void QtExecSync(void(*func)(void*), void* const data)
{
	if (QThread::currentThread() == qApp->thread()) {
		func(data);
	}
	else {
		os_event_t* completeEvent;

		os_event_init(&completeEvent, OS_EVENT_TYPE_AUTO);

		QTimer *t = new QTimer();
		t->moveToThread(qApp->thread());
		t->setSingleShot(true);
		QObject::connect(t, &QTimer::timeout, [=]() {
			t->deleteLater();

			func(data);

			os_event_signal(completeEvent);
		});
		QMetaObject::invokeMethod(t, "start", Qt::QueuedConnection, Q_ARG(int, 0));

		QApplication::sendPostedEvents();

		os_event_wait(completeEvent);
		os_event_destroy(completeEvent);
	}
}
