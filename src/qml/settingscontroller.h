/*
 * SPDX-FileCopyrightText: 2026 Project LemonLime
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 *
 */

#pragma once

#include "base/settings.h"
#include <QObject>
#include <QUrl>
#include <QVariantMap>
#include <memory>

class SettingsController : public QObject {
	Q_OBJECT
	Q_PROPERTY(QVariantMap general READ general NOTIFY generalChanged)
	Q_PROPERTY(QVariantMap limits READ limits CONSTANT)
	Q_PROPERTY(QStringList languages READ languages CONSTANT)
	Q_PROPERTY(QStringList compilerNames READ compilerNames NOTIFY compilerNamesChanged)
	Q_PROPERTY(int compilerIndex READ compilerIndex WRITE setCompilerIndex NOTIFY compilerChanged)
	Q_PROPERTY(QVariantMap compiler READ compiler NOTIFY compilerChanged)
	Q_PROPERTY(QStringList themeNames READ themeNames NOTIFY themeNamesChanged)
	Q_PROPERTY(int themeIndex READ themeIndex WRITE setThemeIndex NOTIFY themeChanged)
	Q_PROPERTY(QVariantMap theme READ theme NOTIFY themeChanged)
	Q_PROPERTY(QVariantList themePreview READ themePreview NOTIFY themeChanged)
	Q_PROPERTY(bool windows READ windows CONSTANT)
	Q_PROPERTY(bool dirty READ dirty NOTIFY dirtyChanged)
	Q_PROPERTY(QString error READ error NOTIFY errorChanged)
	Q_PROPERTY(int validationPage READ validationPage NOTIFY errorChanged)

  public:
	explicit SettingsController(Settings *settings, QObject *parent = nullptr);
	~SettingsController() override;
	Settings *settings() const;
	QVariantMap general() const;
	QVariantMap limits() const;
	QStringList languages() const;
	QStringList compilerNames() const;
	int compilerIndex() const;
	void setCompilerIndex(int index);
	QVariantMap compiler() const;
	QStringList themeNames() const;
	int themeIndex() const;
	void setThemeIndex(int index);
	QVariantMap theme() const;
	QVariantList themePreview() const;
	bool windows() const;
	bool dirty() const;
	QString error() const;
	int validationPage() const;

	Q_INVOKABLE void startEditing();
	Q_INVOKABLE bool apply();
	Q_INVOKABLE void cancel();
	Q_INVOKABLE void setGeneral(const QString &key, const QVariant &value);
	Q_INVOKABLE void setCompilerValue(const QString &key, const QVariant &value);
	Q_INVOKABLE void addCompiler(const QString &preset = QString());
	Q_INVOKABLE QVariantMap compilerPreset(const QString &preset = QString()) const;
	Q_INVOKABLE bool addCompilerDefinitions(const QVariantList &definitions);
	Q_INVOKABLE bool replaceCompiler(const QVariantMap &definition);
	Q_INVOKABLE bool replaceTheme(const QVariantMap &definition);
	Q_INVOKABLE void duplicateCompiler();
	Q_INVOKABLE void deleteCompiler();
	Q_INVOKABLE void moveCompiler(int offset);
	Q_INVOKABLE void addConfiguration();
	Q_INVOKABLE void deleteConfiguration(int index);
	Q_INVOKABLE void setConfiguration(int index, const QString &key, const QVariant &value);
	Q_INVOKABLE bool setEnvironmentVariable(const QString &oldName, const QString &name,
	                                        const QString &value);
	Q_INVOKABLE void removeEnvironmentVariable(const QString &name);
	Q_INVOKABLE bool importCompilers(const QUrl &url);
	Q_INVOKABLE bool exportCompilers(const QUrl &url);
	Q_INVOKABLE QString localFile(const QUrl &url) const;
	Q_INVOKABLE QString executableDirectory(const QString &file) const;
	Q_INVOKABLE void addTheme();
	Q_INVOKABLE void deleteTheme();
	Q_INVOKABLE void setThemeValue(const QString &key, const QVariant &value);

  signals:
	void generalChanged();
	void compilerNamesChanged();
	void compilerChanged();
	void themeNamesChanged();
	void themeChanged();
	void dirtyChanged();
	void errorChanged();
	void settingsApplied();
	void editingCancelled();

  private:
	void markDirty();
	bool fail(const QString &message);
	bool validateCompiler(const QVariantMap &value);
	bool validateTheme(const QVariantMap &value);
	Settings *originalSettings;
	Settings editSettings;
	QVariantMap generalDraft;
	QVariantList compilerDrafts;
	QVariantList themeDrafts;
	int selectedCompiler = -1;
	int selectedTheme = -1;
	bool modified = false;
	QString errorMessage;
	int errorPage = 0;
};
