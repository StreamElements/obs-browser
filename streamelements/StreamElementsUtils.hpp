#pragma once

#include <functional>
#include <QTimer>
#include <QApplication>

inline static void QtPostTask(void (*func)()) {
	QTimer *t = new QTimer();
	t->moveToThread(qApp->thread());
	t->setSingleShot(true);
	QObject::connect(t, &QTimer::timeout, [=]() {
		func();
		t->deleteLater();
	});
	QMetaObject::invokeMethod(t, "start", Qt::QueuedConnection, Q_ARG(int, 0));
}
