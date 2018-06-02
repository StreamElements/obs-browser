#pragma once

#include <functional>
#include <QTimer>
#include <QApplication>
#include <QThread>

inline static void QtPostTask(void (*func)()) {
	if (QThread::currentThread() == qApp->thread()) {
		func();
	}
	else {
		QTimer *t = new QTimer();
		t->moveToThread(qApp->thread());
		t->setSingleShot(true);
		QObject::connect(t, &QTimer::timeout, [=]() {
			func();
			t->deleteLater();
		});
		QMetaObject::invokeMethod(t, "start", Qt::QueuedConnection, Q_ARG(int, 0));
	}
}

inline static void QtPostTask(void(*func)(void*), void* const data) {
	if (QThread::currentThread() == qApp->thread()) {
		func(data);
	}
	else {
		QTimer *t = new QTimer();
		t->moveToThread(qApp->thread());
		t->setSingleShot(true);
		QObject::connect(t, &QTimer::timeout, [=]() {
			func(data);
			t->deleteLater();
		});
		QMetaObject::invokeMethod(t, "start", Qt::QueuedConnection, Q_ARG(int, 0));
	}
}
