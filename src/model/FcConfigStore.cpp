#include "model/FcConfigStore.h"

#include <QRegularExpression>

#include <algorithm>

int FcConfigStore::loadFromDump(const QString &text)
{
    m_params.clear();
    int count = 0;
    const QStringList lines = text.split('\n');
    for (const QString &line : lines) {
        ConfigParam param;
        if (!parseSetLine(line, &param)) {
            continue;
        }
        m_params.insert(param.name, param);
        ++count;
    }
    return count;
}

bool FcConfigStore::contains(const QString &name) const
{
    return m_params.contains(name);
}

ConfigParam FcConfigStore::param(const QString &name) const
{
    return m_params.value(name);
}

QStringList FcConfigStore::values(const QString &name) const
{
    return m_params.value(name).values;
}

QString FcConfigStore::value(const QString &name, const QString &fallback) const
{
    const QStringList current = values(name);
    return current.isEmpty() ? fallback : current.first();
}

int FcConfigStore::intValue(const QString &name, int fallback) const
{
    bool ok = false;
    const int parsed = value(name).toInt(&ok);
    return ok ? parsed : fallback;
}

bool FcConfigStore::boolValue(const QString &name, bool fallback) const
{
    bool ok = false;
    const int parsed = value(name).toInt(&ok);
    return ok ? parsed != 0 : fallback;
}

void FcConfigStore::set(const QString &name, const QStringList &values)
{
    ConfigParam param = m_params.value(name);
    param.name = name;
    param.values = values;
    param.rawLine = QStringLiteral("set %1 %2").arg(name, values.join(QStringLiteral(" "))).trimmed();
    param.dirty = true;
    m_params.insert(name, param);
}

void FcConfigStore::setClean(const QString &name, const QStringList &values)
{
    ConfigParam param = m_params.value(name);
    param.name = name;
    param.values = values;
    param.rawLine = QStringLiteral("set %1 %2").arg(name, values.join(QStringLiteral(" "))).trimmed();
    param.dirty = false;
    m_params.insert(name, param);
}

void FcConfigStore::setValue(const QString &name, const QString &value)
{
    set(name, {value});
}

void FcConfigStore::setInt(const QString &name, int value)
{
    setValue(name, QString::number(value));
}

void FcConfigStore::setBool(const QString &name, bool value)
{
    setInt(name, value ? 1 : 0);
}

QList<ConfigParam> FcConfigStore::allParams() const
{
    QList<ConfigParam> params = m_params.values();
    std::sort(params.begin(), params.end(), [](const ConfigParam &a, const ConfigParam &b) {
        return a.name < b.name;
    });
    return params;
}

QList<ConfigParam> FcConfigStore::dirtyParams() const
{
    QList<ConfigParam> params;
    for (const ConfigParam &param : m_params) {
        if (param.dirty) {
            params.append(param);
        }
    }
    std::sort(params.begin(), params.end(), [](const ConfigParam &a, const ConfigParam &b) {
        return a.name < b.name;
    });
    return params;
}

QStringList FcConfigStore::toCliSetCommands() const
{
    QStringList commands;
    for (const ConfigParam &param : dirtyParams()) {
        commands << QStringLiteral("set %1 %2").arg(param.name, param.values.join(QStringLiteral(" "))).trimmed();
    }
    return commands;
}

void FcConfigStore::clearDirty()
{
    for (auto it = m_params.begin(); it != m_params.end(); ++it) {
        it->dirty = false;
    }
}

bool FcConfigStore::isDirty() const
{
    for (const ConfigParam &param : m_params) {
        if (param.dirty) {
            return true;
        }
    }
    return false;
}

bool FcConfigStore::parseSetLine(const QString &line, ConfigParam *param)
{
    const QString trimmed = line.trimmed();
    if (!trimmed.startsWith(QStringLiteral("set "))) {
        return false;
    }
    const QStringList tokens = trimmed.split(QRegularExpression(QStringLiteral("\\s+")), Qt::SkipEmptyParts);
    if (tokens.size() < 2 || tokens.first() != QStringLiteral("set")) {
        return false;
    }
    if (param) {
        param->name = tokens.at(1);
        param->values = tokens.mid(2);
        param->rawLine = trimmed;
        param->dirty = false;
    }
    return true;
}
