#ifndef QMLSETUP_H
#define QMLSETUP_H

class QQmlEngine;

// Registers the QML types and context objects the UI expects. Shared by the
// application and the UI load test.
void setupQml(QQmlEngine &engine);

#endif // QMLSETUP_H
