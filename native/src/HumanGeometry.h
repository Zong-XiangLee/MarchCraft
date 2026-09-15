#pragma once

#include <QtQuick3D/qquick3dgeometry.h>
#include <QtQml/qqmlregistration.h>

class HumanGeometry : public QQuick3DGeometry
{
    Q_OBJECT
    QML_ELEMENT
    Q_PROPERTY(int detailLevel READ detailLevel WRITE setDetailLevel NOTIFY detailLevelChanged)
    Q_PROPERTY(bool valid READ valid NOTIFY detailLevelChanged)
    Q_PROPERTY(QString errorString READ errorString NOTIFY detailLevelChanged)

public:
    explicit HumanGeometry(QQuick3DObject *parent = nullptr);

    bool valid() const { return m_valid; }
    QString errorString() const { return m_errorString; }
    int detailLevel() const { return m_detailLevel; }
    void setDetailLevel(int value);

signals:
    void detailLevelChanged();

private:
    void applyDetailLevel();

    int m_detailLevel = 0;
    bool m_valid = false;
    QString m_errorString;
};
