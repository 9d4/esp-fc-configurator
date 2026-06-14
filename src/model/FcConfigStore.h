#pragma once

#include <QHash>
#include <QString>
#include <QStringList>

struct ConfigParam
{
    QString name;
    QStringList values;
    QString rawLine;
    bool dirty = false;
};

class FcConfigStore
{
public:
    int loadFromDump(const QString &text);
    bool contains(const QString &name) const;
    ConfigParam param(const QString &name) const;
    QStringList values(const QString &name) const;
    QString value(const QString &name, const QString &fallback = {}) const;
    int intValue(const QString &name, int fallback = 0) const;
    bool boolValue(const QString &name, bool fallback = false) const;

    void set(const QString &name, const QStringList &values);
    void setClean(const QString &name, const QStringList &values);
    void setValue(const QString &name, const QString &value);
    void setInt(const QString &name, int value);
    void setBool(const QString &name, bool value);

    QList<ConfigParam> allParams() const;
    QList<ConfigParam> dirtyParams() const;
    QStringList toCliSetCommands() const;
    void clearDirty();
    bool isDirty() const;
    int size() const { return m_params.size(); }

    static bool parseSetLine(const QString &line, ConfigParam *param);

private:
    QHash<QString, ConfigParam> m_params;
};
