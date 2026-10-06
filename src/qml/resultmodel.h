/*
 * SPDX-FileCopyrightText: 2026 Project LemonLime
 * SPDX-License-Identifier: GPL-3.0-or-later
 */
#pragma once

#include <QAbstractTableModel>
#include <QFont>
#include <QMap>
#include <QSet>

class Contest;
class Contestant;
class Settings;

class ResultModel : public QAbstractTableModel {
	Q_OBJECT
	Q_PROPERTY(int count READ rowCount NOTIFY refreshed)
	Q_PROPERTY(int selectedCount READ selectedCount NOTIFY selectionChanged)
  public:
	enum Role { BackgroundRole = Qt::UserRole + 1, ForegroundRole, SelectedRole, ContestantRole, TaskRole };
	explicit ResultModel(Settings *settings, QObject *parent = nullptr);
	void setContest(Contest *contest);
	void refresh();
	void setBusy(bool busy);
	int rowCount(const QModelIndex &parent = {}) const override;
	int columnCount(const QModelIndex &parent = {}) const override;
	QVariant data(const QModelIndex &index, int role) const override;
	QVariant headerData(int section, Qt::Orientation orientation, int role) const override;
	QHash<int, QByteArray> roleNames() const override;
	Q_INVOKABLE void sort(int column, Qt::SortOrder order = Qt::AscendingOrder) override;
	Q_INVOKABLE void selectCell(int row, int column, bool extend);
	Q_INVOKABLE void selectRange(int firstRow, int firstColumn, int lastRow, int lastColumn, bool extend);
	Q_INVOKABLE void selectAll();
	Q_INVOKABLE void clearSelection();
	Q_INVOKABLE QString nameAt(int row) const;
	Q_INVOKABLE int columnWidth(int column, const QFont &font) const;
	int selectedCount() const;
	QMap<QString, QSet<int>> selection() const;
  signals:
	void refreshed();
	void selectionChanged();

  private:
	QVariant cellValue(const Contestant *contestant, int column) const;
	QVariant cellData(const QModelIndex &index, int role) const;
	QVector<QVector<QHash<int, QVariant>>> cachedRows;
	bool busy = false;
	Settings *settings;
	Contest *contest = nullptr;
	QList<Contestant *> rows;
	QMap<QString, int> ranks;
	QMap<QString, QSet<int>> selected;
	int sortColumn = 0;
	Qt::SortOrder sortOrder = Qt::AscendingOrder;
};
