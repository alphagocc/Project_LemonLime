/*
 * SPDX-FileCopyrightText: 2026 Project LemonLime
 * SPDX-License-Identifier: GPL-3.0-or-later
 */
#include "controlstyle.h"
#include <QAbstractSpinBox>
#include <QAbstractTextDocumentLayout>
#include <QApplication>
#include <QCheckBox>
#include <QClipboard>
#include <QComboBox>
#include <QHeaderView>
#include <QIcon>
#include <QKeyEvent>
#include <QLineEdit>
#include <QMenuBar>
#include <QMouseEvent>
#include <QPainter>
#include <QPushButton>
#include <QQmlEngine>
#include <QQuickImageProvider>
#include <QQuickWindow>
#include <QRadioButton>
#include <QSpinBox>
#include <QStandardItemModel>
#include <QStyle>
#include <QStyleOption>
#include <QTabBar>
#include <QTabWidget>
#include <QTextCursor>
#include <QToolButton>
#include <QUrl>

namespace {
	class IconProvider : public QQuickImageProvider {
	  public:
		IconProvider() : QQuickImageProvider(QQuickImageProvider::Image) {}
		QImage requestImage(const QString &id, QSize *size, const QSize &requestedSize) override {
			const QSize target = requestedSize.isValid() ? requestedSize : QSize(16, 16);
			const QImage result =
			    QIcon(":/icon/" + id.section('?', 0, 0) + ".llsvg").pixmap(target, 1.0).toImage();
			if (size)
				*size = result.size();
			return result;
		}
	};
	void initialize(QStyleOption &option, const QStyleOption &base) {
		const int type = option.type, version = option.version;
		option = base;
		option.type = type;
		option.version = version;
	}
	QRect spinRect(const QRect &rect, const QStyleOption &option, const QString &part) {
		auto *style = QApplication::style();
		// Windows 11 places its spin buttons side by side. Keep the original
		// narrow fields by using one icon-wide column with stacked buttons.
		const int frame = style->pixelMetric(QStyle::PM_SpinBoxFrameWidth, &option);
		const int buttonWidth = style->pixelMetric(QStyle::PM_SmallIconSize, &option);
		const QRect inner = rect.adjusted(frame, frame, -frame, -frame);
		QRect result;
		if (part == "up" || part == "down") {
			const int upperHeight = (inner.height() + 1) / 2;
			result =
			    QRect(inner.right() - buttonWidth + 1, part == "up" ? inner.top() : inner.top() + upperHeight,
			          buttonWidth, part == "up" ? upperHeight : inner.height() - upperHeight);
		} else
			result = inner.adjusted(1, 0, -buttonWidth - 1, 0);
		return QStyle::visualRect(option.direction, rect, result);
	}
} // namespace
void registerControlImages(QQmlEngine *engine) { engine->addImageProvider("lemon-icons", new IconProvider); }

StyleMetrics::StyleMetrics(QObject *parent) : QObject(parent) {
	connect(qApp, &QApplication::paletteChanged, this, [this] {
		sizes.clear();
		++revision;
		emit paletteChanged();
	});
}
QFont StyleMetrics::applicationFont() const { return QApplication::font(); }
int StyleMetrics::textWidth(const QString &text, const QFont &font) const {
	return QFontMetrics(font).horizontalAdvance(text);
}
int StyleMetrics::textHeight(const QFont &font) const { return QFontMetrics(font).height(); }
int StyleMetrics::metric(const QString &name) const {
	static const QHash<QString, QStyle::PixelMetric> metrics{
	    {"layoutMargin", QStyle::PM_LayoutLeftMargin}, {"layoutSpacing", QStyle::PM_LayoutHorizontalSpacing},
	    {"frame", QStyle::PM_DefaultFrameWidth},       {"scrollbar", QStyle::PM_ScrollBarExtent},
	    {"indicatorWidth", QStyle::PM_IndicatorWidth}, {"indicatorHeight", QStyle::PM_IndicatorHeight},
	    {"smallIcon", QStyle::PM_SmallIconSize},       {"tabOverlap", QStyle::PM_TabBarTabOverlap},
	    {"spinFrame", QStyle::PM_SpinBoxFrameWidth},   {"menuBarHMargin", QStyle::PM_MenuBarHMargin},
	    {"menuBarVMargin", QStyle::PM_MenuBarVMargin}};
	return QApplication::style()->pixelMetric(metrics.value(name, QStyle::PM_DefaultFrameWidth));
}
QRect StyleMetrics::tabRect(int width, int height, int tabWidth, int tabHeight, bool contents) const {
	QStyleOptionTabWidgetFrame option;
	option.rect = QRect(0, 0, width, height);
	option.shape = QTabBar::RoundedWest;
	option.tabBarSize = QSize(tabWidth, tabHeight);
	option.lineWidth = QApplication::style()->pixelMetric(QStyle::PM_DefaultFrameWidth);
	QTabWidget measurement;
	measurement.setTabPosition(QTabWidget::West);
	return QApplication::style()->subElementRect(
	    contents ? QStyle::SE_TabWidgetTabContents : QStyle::SE_TabWidgetTabPane, &option, &measurement);
}
QRect StyleMetrics::spinRect(int width, int height, const QFont &font, bool mirrored,
                             const QString &part) const {
	QStyleOptionSpinBox option;
	option.rect = QRect(0, 0, width, height);
	option.fontMetrics = QFontMetrics(font);
	option.direction = mirrored ? Qt::RightToLeft : Qt::LeftToRight;
	return ::spinRect(option.rect, option, part);
}
QSize StyleMetrics::sizeFor(const QString &kind, const QString &text, const QFont &font, bool icon) const {
	const QString key = kind + QChar(0) + text + QChar(0) + font.toString() + (icon ? "1" : "0");
	if (sizes.contains(key))
		return sizes.value(key);
	QSize result;
	// Measurement widgets are never shown. Native Windows styles require the
	// correct widget type for their size hints; visible controls remain QML.
	if (kind == "button") {
		QPushButton measure(text);
		measure.setFont(font);
		if (icon) {
			QPixmap glyph(16, 16);
			glyph.fill(Qt::transparent);
			measure.setIcon(QIcon(glyph));
			measure.setIconSize(QSize(16, 16));
		}
		measure.ensurePolished();
		result = measure.sizeHint();
	} else if (kind == "tool") {
		QToolButton measure;
		measure.setFont(font);
		measure.setText(text);
		measure.ensurePolished();
		result = measure.sizeHint();
	} else if (kind == "edit") {
		QLineEdit measure;
		measure.setFont(font);
		measure.ensurePolished();
		result = measure.sizeHint();
	} else if (kind == "combo") {
		QComboBox measure;
		measure.setFont(font);
		measure.addItem(text);
		measure.ensurePolished();
		result = measure.sizeHint();
	} else if (kind == "spin") {
		QSpinBox measure;
		measure.setFont(font);
		measure.setRange(0, 0);
		measure.setSpecialValueText(text);
		measure.ensurePolished();
		result = measure.sizeHint();
#ifdef Q_OS_WIN
		const int frame = QApplication::style()->pixelMetric(QStyle::PM_SpinBoxFrameWidth);
		result.setWidth(qMax(36, QFontMetrics(font).horizontalAdvance(text) +
		                             QApplication::style()->pixelMetric(QStyle::PM_SmallIconSize) +
		                             2 * (frame + 1)));
#endif
	} else if (kind == "check") {
		QCheckBox measure(text);
		measure.setFont(font);
		measure.ensurePolished();
		result = measure.sizeHint();
	} else if (kind == "radio") {
		QRadioButton measure(text);
		measure.setFont(font);
		measure.ensurePolished();
		result = measure.sizeHint();
	} else if (kind == "tab" || kind == "westtab") {
		QTabBar measure;
		measure.setFont(font);
		measure.setShape(kind == "westtab" ? QTabBar::RoundedWest : QTabBar::RoundedNorth);
		measure.setExpanding(false);
		measure.addTab(text);
		measure.ensurePolished();
		measure.resize(measure.sizeHint());
		result = measure.tabRect(0).size();
	} else if (kind == "header") {
		QHeaderView measure(Qt::Horizontal);
		QStandardItemModel model(0, 1);
		model.setHeaderData(0, Qt::Horizontal, text);
		measure.setFont(font);
		measure.setModel(&model);
		measure.ensurePolished();
		measure.resizeSections(QHeaderView::ResizeToContents);
		result = QSize(measure.sectionSize(0), measure.sizeHint().height());
	} else if (kind == "menubaritem") {
		QMenuBar measure;
		measure.setFont(font);
		auto *action = measure.addAction(text);
		measure.ensurePolished();
		measure.resize(measure.sizeHint());
		result = measure.actionGeometry(action).size();
	} else
		result = QFontMetrics(font).size(Qt::TextShowMnemonic, text);
	if (sizes.size() >= 512)
		sizes.clear();
	sizes.insert(key, result);
	return result;
}
StyleItem::StyleItem(QQuickItem *parent) : QQuickPaintedItem(parent), font(QApplication::font()) {
	setAntialiasing(false);
	connect(this, &StyleItem::styleChanged, this, [this] { polish(); });
	connect(this, &QQuickItem::enabledChanged, this, [this] { polish(); });
	connect(qApp, &QApplication::paletteChanged, this, [this] { polish(); });
	connect(this, &QQuickItem::windowChanged, this, [this] { polish(); });
}
void StyleItem::geometryChange(const QRectF &next, const QRectF &previous) {
	QQuickPaintedItem::geometryChange(next, previous);
	polish();
}
void StyleItem::paint(QPainter *painter) { painter->drawImage(QPointF(), rendered); }
void StyleItem::updatePolish() {
	const qreal dpr = window() ? window()->effectiveDevicePixelRatio() : 1;
	const QSize size(qMax(1, qRound(width() * dpr)), qMax(1, qRound(height() * dpr)));
	rendered = QImage(size, QImage::Format_ARGB32_Premultiplied);
	rendered.setDevicePixelRatio(dpr);
	rendered.fill(Qt::transparent);
	QPainter painter(&rendered);
	painter.setFont(font);
	auto *style = QApplication::style();
	QStyleOption base;
	base.rect = QRect(0, 0, qRound(width()), qRound(height()));
	base.palette = QApplication::palette();
	base.fontMetrics = QFontMetrics(font);
	base.state = QStyle::State_Active;
	if (isEnabled())
		base.state |= QStyle::State_Enabled;
	else
		base.palette.setCurrentColorGroup(QPalette::Disabled);
	if (hovered)
		base.state |= QStyle::State_MouseOver;
	if (pressed)
		base.state |= QStyle::State_Sunken;
	else
		base.state |= QStyle::State_Raised;
	if (focused)
		base.state |= QStyle::State_HasFocus | QStyle::State_KeyboardFocusChange;
	base.state |= checked ? QStyle::State_On : QStyle::State_Off;
	QString iconPath = iconSource;
	if (iconPath.startsWith("qrc:/"))
		iconPath.remove(0, 3);
	const QIcon icon(iconPath);
	if (kind == "button" || kind == "check" || kind == "radio") {
		QStyleOptionButton option;
		initialize(option, base);
		option.text = text;
		option.icon = icon;
		option.iconSize = QSize(16, 16);
		if (flat)
			option.features |= QStyleOptionButton::Flat;
		style->drawControl(kind == "button"  ? QStyle::CE_PushButton
		                   : kind == "check" ? QStyle::CE_CheckBox
		                                     : QStyle::CE_RadioButton,
		                   &option, &painter);
	} else if (kind == "tool") {
		QStyleOptionToolButton option;
		initialize(option, base);
		option.text = text;
		option.icon = icon;
		option.font = font;
		option.iconSize = QSize(16, 16);
		option.toolButtonStyle = icon.isNull() ? Qt::ToolButtonTextOnly
		                         : value == 1  ? Qt::ToolButtonIconOnly
		                                       : Qt::ToolButtonTextBesideIcon;
		option.subControls = QStyle::SC_ToolButton;
		option.activeSubControls = QStyle::SC_ToolButton;
		style->drawComplexControl(QStyle::CC_ToolButton, &option, &painter);
	} else if (kind == "edit" || kind == "frame") {
		QStyleOptionFrame option;
		initialize(option, base);
		option.state |= QStyle::State_Sunken;
		option.lineWidth = style->pixelMetric(QStyle::PM_DefaultFrameWidth);
		if (kind == "frame")
			painter.fillRect(base.rect, base.palette.base());
		style->drawPrimitive(kind == "edit" ? QStyle::PE_PanelLineEdit : QStyle::PE_Frame, &option, &painter);
	} else if (kind == "combo") {
		QStyleOptionComboBox option;
		initialize(option, base);
		option.editable = editable;
		option.currentText = text;
		option.subControls = QStyle::SC_All;
		option.activeSubControls = pressed ? QStyle::SC_ComboBoxArrow : QStyle::SC_None;
		style->drawComplexControl(QStyle::CC_ComboBox, &option, &painter);
		if (! editable)
			style->drawControl(QStyle::CE_ComboBoxLabel, &option, &painter);
	} else if (kind == "spin") {
		QStyleOptionFrame option;
		initialize(option, base);
		option.direction = mirrored ? Qt::RightToLeft : Qt::LeftToRight;
		option.state &= ~(QStyle::State_Sunken | QStyle::State_Raised);
		option.state |= QStyle::State_Sunken;
		option.lineWidth = style->pixelMetric(QStyle::PM_DefaultFrameWidth);
		style->drawPrimitive(QStyle::PE_PanelLineEdit, &option, &painter);
		for (int step = 1; step <= 2; ++step) {
			QStyleOption arrow;
			initialize(arrow, base);
			arrow.direction = option.direction;
			arrow.rect = ::spinRect(base.rect, arrow, step == 1 ? "up" : "down");
			arrow.state &= ~(QStyle::State_MouseOver | QStyle::State_Sunken | QStyle::State_Raised);
			if (! isEnabled() || ! (step == 1 ? stepUpEnabled : stepDownEnabled)) {
				arrow.state &= ~QStyle::State_Enabled;
				arrow.palette.setCurrentColorGroup(QPalette::Disabled);
			} else if (value == step) {
				arrow.state |= QStyle::State_MouseOver;
				if (pressed)
					arrow.state |= QStyle::State_Sunken;
				style->drawPrimitive(QStyle::PE_PanelButtonTool, &arrow, &painter);
			}
			arrow.rect.adjust(3, 1, -3, -1);
			style->drawPrimitive(step == 1 ? QStyle::PE_IndicatorArrowUp : QStyle::PE_IndicatorArrowDown,
			                     &arrow, &painter);
		}
	} else if (kind == "group") {
		QStyleOptionGroupBox option;
		initialize(option, base);
		option.text = text;
		option.textAlignment = Qt::AlignLeft;
		option.lineWidth = 1;
		option.subControls = QStyle::SC_GroupBoxFrame | QStyle::SC_GroupBoxLabel;
		if (flat)
			option.subControls |= QStyle::SC_GroupBoxCheckBox;
		style->drawComplexControl(QStyle::CC_GroupBox, &option, &painter);
	} else if (kind == "tab") {
		QStyleOptionTab option;
		initialize(option, base);
		option.text = text;
		option.shape = vertical ? QTabBar::RoundedWest : QTabBar::RoundedNorth;
		option.position = first && last ? QStyleOptionTab::OnlyOneTab
		                  : first       ? QStyleOptionTab::Beginning
		                  : last        ? QStyleOptionTab::End
		                                : QStyleOptionTab::Middle;
		if (checked)
			option.state |= QStyle::State_Selected;
		style->drawControl(QStyle::CE_TabBarTab, &option, &painter);
	} else if (kind == "tabframe") {
		QStyleOptionTabWidgetFrame option;
		initialize(option, base);
		option.shape = vertical ? QTabBar::RoundedWest : QTabBar::RoundedNorth;
		option.lineWidth = style->pixelMetric(QStyle::PM_DefaultFrameWidth);
		style->drawPrimitive(QStyle::PE_FrameTabWidget, &option, &painter);
	} else if (kind == "header") {
		QStyleOptionHeader option;
		initialize(option, base);
		option.text = text;
		option.orientation = vertical ? Qt::Vertical : Qt::Horizontal;
		option.textAlignment = Qt::Alignment(alignment);
		option.position = first  ? QStyleOptionHeader::Beginning
		                  : last ? QStyleOptionHeader::End
		                         : QStyleOptionHeader::Middle;
		if (checked)
			option.sortIndicator = value == 0 ? QStyleOptionHeader::SortDown : QStyleOptionHeader::SortUp;
		style->drawControl(QStyle::CE_Header, &option, &painter);
	} else if (kind == "progress") {
		QStyleOptionProgressBar option;
		initialize(option, base);
		option.state |= QStyle::State_Horizontal;
		option.minimum = 0;
		option.maximum = maximum;
		option.progress = value;
		option.textVisible = true;
		option.text = text;
		option.textAlignment = Qt::AlignCenter;
		style->drawControl(QStyle::CE_ProgressBar, &option, &painter);
	} else if (kind == "menuitem" || kind == "menubaritem") {
		QStyleOptionMenuItem option;
		initialize(option, base);
		option.text = text;
		option.font = font;
		option.icon = icon;
		option.menuItemType = flat ? QStyleOptionMenuItem::Separator : QStyleOptionMenuItem::Normal;
		option.checkType = QStyleOptionMenuItem::NotCheckable;
		option.maxIconWidth = 20;
		if (hovered || pressed)
			option.state |= QStyle::State_Selected;
		style->drawControl(kind == "menuitem" ? QStyle::CE_MenuItem : QStyle::CE_MenuBarItem, &option,
		                   &painter);
	} else if (kind == "branch") {
		base.state |= QStyle::State_Children | QStyle::State_Item;
		if (checked)
			base.state |= QStyle::State_Open;
		style->drawPrimitive(QStyle::PE_IndicatorBranch, &base, &painter);
	} else if (kind == "sizegrip") {
		style->drawControl(QStyle::CE_SizeGrip, &base, &painter);
	}
	update();
}

TextDocumentItem::TextDocumentItem(QQuickItem *parent)
    : QQuickPaintedItem(parent), font(QApplication::font()) {
	setAcceptedMouseButtons(Qt::LeftButton);
	setFlag(ItemIsFocusScope);
	document.setDocumentMargin(4);
	connect(this, &TextDocumentItem::documentChanged, this, [this] {
		document.setDefaultFont(font);
		document.setHtml(html);
		document.setTextWidth(width());
		selectionStart = selectionEnd = 0;
		emit documentSizeChanged();
		polish();
	});
	connect(this, &TextDocumentItem::offsetChanged, this, [this] { polish(); });
	connect(qApp, &QApplication::paletteChanged, this, [this] { polish(); });
}
qreal TextDocumentItem::documentWidth() const { return document.size().width(); }
qreal TextDocumentItem::documentHeight() const { return document.size().height(); }
void TextDocumentItem::geometryChange(const QRectF &next, const QRectF &previous) {
	QQuickPaintedItem::geometryChange(next, previous);
	if (next.width() != previous.width()) {
		document.setTextWidth(next.width());
		emit documentSizeChanged();
	}
	polish();
}
void TextDocumentItem::paint(QPainter *painter) { painter->drawImage(QPointF(), rendered); }
void TextDocumentItem::updatePolish() {
	const qreal dpr = window() ? window()->effectiveDevicePixelRatio() : 1;
	rendered = QImage(QSize(qMax(1, qRound(width() * dpr)), qMax(1, qRound(height() * dpr))),
	                  QImage::Format_ARGB32_Premultiplied);
	rendered.setDevicePixelRatio(dpr);
	rendered.fill(QApplication::palette().color(QPalette::Base));
	QPainter painter(&rendered);
	painter.translate(-offsetX, -offsetY);
	QAbstractTextDocumentLayout::PaintContext context;
	context.palette = QApplication::palette();
	context.clip = QRectF(offsetX, offsetY, width(), height());
	if (selectionStart != selectionEnd) {
		QAbstractTextDocumentLayout::Selection selection;
		selection.cursor = QTextCursor(&document);
		selection.cursor.setPosition(qMin(selectionStart, selectionEnd));
		selection.cursor.setPosition(qMax(selectionStart, selectionEnd), QTextCursor::KeepAnchor);
		selection.format.setBackground(context.palette.highlight());
		selection.format.setForeground(context.palette.highlightedText());
		context.selections.append(selection);
	}
	document.documentLayout()->draw(&painter, context);
	update();
}
void TextDocumentItem::mousePressEvent(QMouseEvent *event) {
	const auto point = event->position() + QPointF(offsetX, offsetY);
	selectionStart = selectionEnd = qMax(0, document.documentLayout()->hitTest(point, Qt::FuzzyHit));
	pressedLink = document.documentLayout()->anchorAt(point);
	forceActiveFocus();
	setKeepMouseGrab(true);
	polish();
	event->accept();
}
void TextDocumentItem::mouseMoveEvent(QMouseEvent *event) {
	selectionEnd = qMax(
	    0, document.documentLayout()->hitTest(event->position() + QPointF(offsetX, offsetY), Qt::FuzzyHit));
	polish();
	event->accept();
}
void TextDocumentItem::mouseReleaseEvent(QMouseEvent *event) {
	setKeepMouseGrab(false);
	const auto link = document.documentLayout()->anchorAt(event->position() + QPointF(offsetX, offsetY));
	if (! pressedLink.isEmpty() && link == pressedLink && selectionStart == selectionEnd)
		emit linkActivated(link);
	event->accept();
}
void TextDocumentItem::keyPressEvent(QKeyEvent *event) {
	if (event->matches(QKeySequence::Copy)) {
		QTextCursor cursor(&document);
		cursor.setPosition(qMin(selectionStart, selectionEnd));
		cursor.setPosition(qMax(selectionStart, selectionEnd), QTextCursor::KeepAnchor);
		QGuiApplication::clipboard()->setText(cursor.selectedText());
		event->accept();
	} else if (event->matches(QKeySequence::SelectAll)) {
		selectionStart = 0;
		selectionEnd = document.characterCount() - 1;
		polish();
		event->accept();
	} else
		QQuickPaintedItem::keyPressEvent(event);
}
