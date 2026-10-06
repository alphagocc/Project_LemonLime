/*
 * SPDX-FileCopyrightText: 2026 Project LemonLime
 * SPDX-License-Identifier: GPL-3.0-or-later
 */
#include "resultmodel.h"
#include "base/settings.h"
#include "controlstyle.h"
#include "core/contest.h"
#include "core/contestant.h"
#include "core/task.h"
#include <QFontMetrics>
#include <QGuiApplication>
#include <QPalette>
#include <algorithm>

ResultModel::ResultModel(Settings *settings, QObject *parent)
    : QAbstractTableModel(parent), settings(settings) {}

void ResultModel::setContest(Contest *value) {
	contest = value;
	selected.clear();
	refresh();
}

void ResultModel::refresh() {
	if (busy)
		return;
	beginResetModel();
	rows = contest ? contest->getContestantList() : QList<Contestant *>();
	sortColumn = std::clamp(sortColumn, 0, qMax(0, columnCount() - 1));
	ranks.clear();
	std::sort(rows.begin(), rows.end(), [](auto a, auto b) {
		if (a->getTotalScore() != b->getTotalScore())
			return a->getTotalScore() > b->getTotalScore();
		return a->getContestantName() < b->getContestantName();
	});
	int rank = 0;
	for (int i = 0; i < rows.size(); ++i) {
		if (i == 0 || rows[i]->getTotalScore() != rows[i - 1]->getTotalScore())
			rank = i + 1;
		if (rows[i]->getTotalScore() >= 0)
			ranks.insert(rows[i]->getContestantName(), rank);
	}
	for (auto it = selected.begin(); it != selected.end();) {
		if (! contest || ! contest->getContestant(it.key()))
			it = selected.erase(it);
		else {
			for (auto cell = it->begin(); cell != it->end();) {
				if (*cell >= contest->getTaskList().size())
					cell = it->erase(cell);
				else
					++cell;
			}
			if (it->isEmpty())
				it = selected.erase(it);
			else
				++it;
		}
	}
	std::stable_sort(rows.begin(), rows.end(), [this](auto a, auto b) {
		auto av = cellValue(a, sortColumn), bv = cellValue(b, sortColumn);
		int comparison = sortColumn == 1 || sortColumn == columnCount() - 1
		                     ? av.toString().localeAwareCompare(bv.toString())
		                     : (av.toInt() < bv.toInt()   ? -1
		                        : av.toInt() > bv.toInt() ? 1
		                                                  : 0);
		if (comparison == 0)
			return a->getContestantName() < b->getContestantName();
		return sortOrder == Qt::AscendingOrder ? comparison < 0 : comparison > 0;
	});
	cachedRows.clear();
	for (int row = 0; row < rowCount(); ++row) {
		QVector<QHash<int, QVariant>> cells;
		for (int col = 0; col < columnCount(); ++col) {
			QHash<int, QVariant> cell;
			for (int role : {int(Qt::DisplayRole), int(Qt::ToolTipRole), int(BackgroundRole),
			                 int(ForegroundRole), int(ContestantRole), int(TaskRole)})
				cell.insert(role, cellData(index(row, col), role));
			cells.append(cell);
		}
		cachedRows.append(cells);
	}
	endResetModel();
	emit refreshed();
	emit selectionChanged();
}

int ResultModel::rowCount(const QModelIndex &parent) const { return parent.isValid() ? 0 : rows.size(); }
int ResultModel::columnCount(const QModelIndex &parent) const {
	return parent.isValid() || ! contest ? 0 : contest->getTaskList().size() + 5;
}

QVariant ResultModel::cellValue(const Contestant *c, int col) const {
	if (! contest)
		return {};
	if (col == 0)
		return ranks.value(c->getContestantName(), 0);
	if (col == 1)
		return c->getContestantName();
	if (col == 2)
		return c->getTotalScore();
	if (col < columnCount() - 2)
		return c->getTaskScore(col - 3);
	if (col == columnCount() - 2)
		return c->getTotalUsedTime();
	return c->getJudingTime().toString("yyyy-MM-dd HH:mm:ss");
}

QVariant ResultModel::data(const QModelIndex &idx, int role) const {
	if (! idx.isValid() || idx.row() >= cachedRows.size() || idx.column() >= cachedRows[idx.row()].size())
		return {};
	const auto &cell = cachedRows[idx.row()][idx.column()];
	if (role == SelectedRole) {
		const auto cells = selected.value(cell.value(ContestantRole).toString());
		return cells.contains(-1) || cells.contains(cell.value(TaskRole).toInt());
	}
	return cell.value(role);
}

QVariant ResultModel::cellData(const QModelIndex &idx, int role) const {
	if (! idx.isValid() || idx.row() >= rows.size() || idx.column() >= columnCount())
		return {};
	const auto *c = rows[idx.row()];
	int col = idx.column(), task = col >= 3 && col < columnCount() - 2 ? col - 3 : -1;
	if (role == ContestantRole)
		return c->getContestantName();
	if (role == TaskRole)
		return task;
	if (role == SelectedRole) {
		auto cells = selected.value(c->getContestantName());
		return cells.contains(-1) || (task >= 0 && cells.contains(task));
	}
	if (role == Qt::DisplayRole || role == Qt::ToolTipRole) {
		auto value = cellValue(c, col);
		if (col == columnCount() - 2 && value.toInt() >= 0)
			return value.toDouble() / 1000;
		if (col != 1 && col != columnCount() - 1 && (value.toInt() < 0 || (col == 0 && value.toInt() == 0)))
			return QCoreApplication::translate("ResultViewer", "Invalid");
		return value;
	}
	const auto palette = QGuiApplication::palette();
	QColor background = palette.color(QPalette::Base);
	ColorTheme colors = settings->getCurrentColorTheme();
	if (palette.color(QPalette::WindowText).lightness() > palette.color(QPalette::Window).lightness())
		colors.invertLightness();
	if (col == 2 && c->getTotalScore() >= 0 && contest->getTotalScore() > 0)
		background = colors.getColorGrand(c->getTotalScore(), contest->getTotalScore());
	if (task >= 0 && c->getCheckJudged(task)) {
		if (c->getCompileState(task) != CompileSuccessfully &&
		    contest->getTask(task)->getTaskType() != Task::AnswersOnly)
			background =
			    c->getCompileState(task) == NoValidSourceFile ? colors.getColorNf() : colors.getColorCe();
		else if (contest->getTask(task)->getTotalScore() > 0)
			background = colors.getColorPer(c->getTaskScore(task), contest->getTask(task)->getTotalScore());
	}
	if (role == BackgroundRole)
		return background;
	if (role == ForegroundRole)
		return palette.color(QPalette::Text);
	return {};
}

QVariant ResultModel::headerData(int section, Qt::Orientation orientation, int role) const {
	if (orientation != Qt::Horizontal || role != Qt::DisplayRole || ! contest)
		return {};
	if (section == 0)
		return QCoreApplication::translate("ResultViewer", "Rank");
	if (section == 1)
		return QCoreApplication::translate("ResultViewer", "Name");
	if (section == 2)
		return QCoreApplication::translate("ResultViewer", "Total Score");
	if (section < columnCount() - 2)
		return contest->getTask(section - 3)->getProblemTitle();
	if (section == columnCount() - 2)
		return QCoreApplication::translate("ResultViewer", "Total Used Time (s)");
	return QCoreApplication::translate("ResultViewer", "Judging Time");
}

QHash<int, QByteArray> ResultModel::roleNames() const {
	return {{Qt::DisplayRole, "display"},
	        {Qt::ToolTipRole, "tooltip"},
	        {BackgroundRole, "cellBackground"},
	        {ForegroundRole, "cellForeground"},
	        {SelectedRole, "cellSelected"},
	        {ContestantRole, "contestantName"},
	        {TaskRole, "taskIndex"}};
}

void ResultModel::sort(int column, Qt::SortOrder order) {
	if (busy || column < 0 || column >= columnCount())
		return;
	sortColumn = column;
	sortOrder = order;
	refresh();
}

void ResultModel::selectCell(int row, int column, bool extend) {
	if (row < 0 || row >= rows.size() || column < 0 || column >= columnCount())
		return;
	if (! extend)
		selected.clear();
	int task = column >= 3 && column < columnCount() - 2 ? column - 3 : -1;
	auto &cells = selected[rows[row]->getContestantName()];
	if (extend && cells.contains(task))
		cells.remove(task);
	else
		cells.insert(task);
	if (cells.isEmpty())
		selected.remove(rows[row]->getContestantName());
	emit dataChanged(index(0, 0), index(rowCount() - 1, columnCount() - 1), {SelectedRole});
	emit selectionChanged();
}
void ResultModel::selectAll() {
	for (auto *row : rows)
		selected[row->getContestantName()] = {-1};
	if (! rows.isEmpty())
		emit dataChanged(index(0, 0), index(rowCount() - 1, columnCount() - 1), {SelectedRole});
	emit selectionChanged();
}
void ResultModel::selectRange(int firstRow, int firstColumn, int lastRow, int lastColumn, bool extend) {
	if (rows.isEmpty() || ! contest)
		return;
	if (! extend)
		selected.clear();
	int top = std::clamp(qMin(firstRow, lastRow), 0, rowCount() - 1);
	int bottom = std::clamp(qMax(firstRow, lastRow), 0, rowCount() - 1);
	int left = std::clamp(qMin(firstColumn, lastColumn), 0, columnCount() - 1);
	int right = std::clamp(qMax(firstColumn, lastColumn), 0, columnCount() - 1);
	for (int row = top; row <= bottom; ++row)
		for (int col = left; col <= right; ++col)
			selected[rows[row]->getContestantName()].insert(col >= 3 && col < columnCount() - 2 ? col - 3
			                                                                                    : -1);
	emit dataChanged(index(0, 0), index(rowCount() - 1, columnCount() - 1), {SelectedRole});
	emit selectionChanged();
}
void ResultModel::clearSelection() {
	selected.clear();
	if (! rows.isEmpty())
		emit dataChanged(index(0, 0), index(rowCount() - 1, columnCount() - 1), {SelectedRole});
	emit selectionChanged();
}
QString ResultModel::nameAt(int row) const {
	return row >= 0 && row < rows.size() ? rows[row]->getContestantName() : QString();
}
int ResultModel::selectedCount() const { return selected.size(); }
QMap<QString, QSet<int>> ResultModel::selection() const { return selected; }
void ResultModel::setBusy(bool value) { busy = value; }
int ResultModel::columnWidth(int column, const QFont &font) const {
	const QFontMetrics metrics(font);
	StyleMetrics style;
	int width =
	    qMax(75, style.sizeFor("header", headerData(column, Qt::Horizontal, Qt::DisplayRole).toString(), font)
	                 .width());
	for (int row = 0; row < rowCount(); ++row)
		width =
		    qMax(width, metrics.horizontalAdvance(data(index(row, column), Qt::DisplayRole).toString()) + 8);
	return width;
}
