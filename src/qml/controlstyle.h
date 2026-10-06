/*
 * SPDX-FileCopyrightText: 2026 Project LemonLime
 * SPDX-License-Identifier: GPL-3.0-or-later
 */
#pragma once

#include <QFont>
#include <QImage>
#include <QObject>
#include <QQuickPaintedItem>
#include <QTextDocument>
#include <QtQml/qqmlregistration.h>
class QQmlEngine;
void registerControlImages(QQmlEngine *engine);

class StyleMetrics : public QObject {
	Q_OBJECT
	QML_ELEMENT
	QML_SINGLETON
	Q_PROPERTY(QFont applicationFont READ applicationFont CONSTANT)
	Q_PROPERTY(int paletteRevision READ paletteRevision NOTIFY paletteChanged)
  public:
	explicit StyleMetrics(QObject *parent = nullptr);
	QFont applicationFont() const;
	int paletteRevision() const { return revision; }
	Q_INVOKABLE QSize sizeFor(const QString &kind, const QString &text, const QFont &font,
	                          bool icon = false) const;
	Q_INVOKABLE int textWidth(const QString &text, const QFont &font) const;
	Q_INVOKABLE int textHeight(const QFont &font) const;
	Q_INVOKABLE int metric(const QString &name) const;
	Q_INVOKABLE QRect tabRect(int width, int height, int tabWidth, int tabHeight, bool contents) const;
	Q_INVOKABLE QRect spinRect(int width, int height, const QFont &font, bool mirrored,
	                           const QString &part) const;
  signals:
	void paletteChanged();

  private:
	int revision = 0;
	mutable QHash<QString, QSize> sizes;
};

class StyleItem : public QQuickPaintedItem {
	Q_OBJECT
	QML_ELEMENT
	Q_PROPERTY(QString kind MEMBER kind NOTIFY styleChanged)
	Q_PROPERTY(QString text MEMBER text NOTIFY styleChanged)
	Q_PROPERTY(QString iconSource MEMBER iconSource NOTIFY styleChanged)
	Q_PROPERTY(QFont font MEMBER font NOTIFY styleChanged)
	Q_PROPERTY(bool hovered MEMBER hovered NOTIFY styleChanged)
	Q_PROPERTY(bool pressed MEMBER pressed NOTIFY styleChanged)
	Q_PROPERTY(bool checked MEMBER checked NOTIFY styleChanged)
	Q_PROPERTY(bool focused MEMBER focused NOTIFY styleChanged)
	Q_PROPERTY(bool flat MEMBER flat NOTIFY styleChanged)
	Q_PROPERTY(bool vertical MEMBER vertical NOTIFY styleChanged)
	Q_PROPERTY(bool first MEMBER first NOTIFY styleChanged)
	Q_PROPERTY(bool last MEMBER last NOTIFY styleChanged)
	Q_PROPERTY(bool editable MEMBER editable NOTIFY styleChanged)
	Q_PROPERTY(bool mirrored MEMBER mirrored NOTIFY styleChanged)
	Q_PROPERTY(bool stepUpEnabled MEMBER stepUpEnabled NOTIFY styleChanged)
	Q_PROPERTY(bool stepDownEnabled MEMBER stepDownEnabled NOTIFY styleChanged)
	Q_PROPERTY(int alignment MEMBER alignment NOTIFY styleChanged)
	Q_PROPERTY(int value MEMBER value NOTIFY styleChanged)
	Q_PROPERTY(int maximum MEMBER maximum NOTIFY styleChanged)
  public:
	explicit StyleItem(QQuickItem *parent = nullptr);
	void paint(QPainter *painter) override;
  signals:
	void styleChanged();

  protected:
	void updatePolish() override;
	void geometryChange(const QRectF &newGeometry, const QRectF &oldGeometry) override;

  private:
	QString kind;
	QString text;
	QString iconSource;
	QFont font;
	bool hovered = false;
	bool pressed = false;
	bool checked = false;
	bool focused = false;
	bool flat = false;
	bool vertical = false;
	bool first = false;
	bool last = false;
	bool editable = false;
	bool mirrored = false;
	bool stepUpEnabled = true;
	bool stepDownEnabled = true;
	int alignment = Qt::AlignCenter;
	int value = 0;
	int maximum = 100;
	QImage rendered;
};

class TextDocumentItem : public QQuickPaintedItem {
	Q_OBJECT
	QML_ELEMENT
	Q_PROPERTY(QString html MEMBER html NOTIFY documentChanged)
	Q_PROPERTY(QFont font MEMBER font NOTIFY documentChanged)
	Q_PROPERTY(qreal offsetX MEMBER offsetX NOTIFY offsetChanged)
	Q_PROPERTY(qreal offsetY MEMBER offsetY NOTIFY offsetChanged)
	Q_PROPERTY(qreal documentWidth READ documentWidth NOTIFY documentSizeChanged)
	Q_PROPERTY(qreal documentHeight READ documentHeight NOTIFY documentSizeChanged)
  public:
	explicit TextDocumentItem(QQuickItem *parent = nullptr);
	void paint(QPainter *painter) override;
	qreal documentWidth() const;
	qreal documentHeight() const;
  signals:
	void documentChanged();
	void offsetChanged();
	void documentSizeChanged();
	void linkActivated(const QString &link);

  protected:
	void updatePolish() override;
	void geometryChange(const QRectF &next, const QRectF &previous) override;
	void mousePressEvent(QMouseEvent *event) override;
	void mouseMoveEvent(QMouseEvent *event) override;
	void mouseReleaseEvent(QMouseEvent *event) override;
	void keyPressEvent(QKeyEvent *event) override;

  private:
	QString html;
	QFont font;
	qreal offsetX = 0, offsetY = 0;
	QTextDocument document;
	QImage rendered;
	int selectionStart = 0, selectionEnd = 0;
	QString pressedLink;
};
