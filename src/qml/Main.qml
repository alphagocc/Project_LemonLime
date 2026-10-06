/* SPDX-FileCopyrightText: 2026 Project LemonLime
 * SPDX-License-Identifier: GPL-3.0-or-later */
import QtQuick
import "controls" as Controls
import QtQuick.Controls
import QtQuick.Layouts
import QtQuick.Dialogs
import QtCore
ApplicationWindow {
    id: window
    objectName: "mainWindow"
    width: appController.windowSize.width; height: appController.windowSize.height
    minimumWidth: 725; minimumHeight: 510
    visible: true
    title: appController.hasContest ? qsTranslate("LemonLime", "LemonLime - %1").arg(appController.contestTitle) : "LemonLime"
    font: StyleMetrics.applicationFont
    property int page: 0
    property bool automaticWelcome: true
    property bool allowClose: false
    property string statusText: ""
    property alias settingsWindow: preferences
    property alias welcomeWindow: welcome
    property alias newContestWindow: newContest
    property alias openContestWindow: openContest
    property alias judgingWindow: progress
    function settingsPage() { if (!appController.judging) preferences.open() }
    function navigate(index) { page = index }
    function showMessage(text) { messageText.text = text; message.open() }
    function startup() {
        if (!automaticWelcome) return
        if (appController.splashDuration > 0) splash.show()
        else showWelcome()
    }
    function showWelcome() {
        if (settingsController.compilerNames.length === 0) firstCompiler.open()
        else welcome.open()
    }
    Component.onCompleted: Qt.callLater(startup)
    onClosing: function(event) {
        if (allowClose) return
        event.accepted = false
        if (!appController.judging && appController.prepareExit()) { appController.saveWindowSize(width, height); allowClose = true; Qt.callLater(window.close) }
    }
    Action { id: newAction; text: qsTranslate("LemonLime", "&New Contest"); icon.source: "image://lemon-icons/document-new?palette=" + StyleMetrics.paletteRevision; enabled: !appController.judging; onTriggered: newContest.open() }
    Action { id: openAction; text: qsTranslate("LemonLime", "&Open Existing Contest"); icon.source: "image://lemon-icons/quickopen-file?palette=" + StyleMetrics.paletteRevision; enabled: !appController.judging; onTriggered: openContest.open() }
    Action { id: saveAction; text: qsTranslate("LemonLime", "&Save Current Contest"); icon.source: "image://lemon-icons/document-save?palette=" + StyleMetrics.paletteRevision; enabled: appController.hasContest && !appController.judging; onTriggered: appController.saveContest() }
    Action { id: closeAction; text: qsTranslate("LemonLime", "&Close Current Contest"); icon.source: "image://lemon-icons/document-close?palette=" + StyleMetrics.paletteRevision; enabled: saveAction.enabled; onTriggered: appController.closeContest() }
    Action { id: folderAction; text: qsTranslate("LemonLime", "Open Current Contest &Folder"); icon.source: "image://lemon-icons/go-parent-folder?palette=" + StyleMetrics.paletteRevision; enabled: saveAction.enabled; onTriggered: appController.openFolder() }
    Action { id: renameAction; text: qsTranslate("LemonLime", "C&hange Contest Name"); icon.source: "image://lemon-icons/document-edit?palette=" + StyleMetrics.paletteRevision; enabled: saveAction.enabled; onTriggered: { renameField.text = appController.contestTitle; renameDialog.open() } }
    Action { id: exitAction; text: qsTranslate("LemonLime", "E&xit"); icon.source: "image://lemon-icons/application-exit?palette=" + StyleMetrics.paletteRevision; onTriggered: window.close() }
    Action { id: allAction; text: qsTranslate("LemonLime", "Judge &All"); icon.source: "image://lemon-icons/code-function?palette=" + StyleMetrics.paletteRevision; enabled: saveAction.enabled && window.page === 1 && appController.results.count > 0; onTriggered: appController.judge("all") }
    Action { id: selectedAction; text: qsTranslate("LemonLime", "&Judge Selected"); icon.source: "image://lemon-icons/edit-find?palette=" + StyleMetrics.paletteRevision; enabled: saveAction.enabled && window.page === 1 && appController.results.selectedCount > 0; onTriggered: appController.judge("selected") }
    Action { id: unjudgedAction; text: qsTranslate("LemonLime", "Judge &Unjudged"); icon.source: "image://lemon-icons/edit-find-replace?palette=" + StyleMetrics.paletteRevision; enabled: allAction.enabled; onTriggered: appController.judge("unjudged") }
    Action { id: missingAction; text: qsTranslate("LemonLime", "Judge &No Source"); icon.source: "image://lemon-icons/paint-unknown?palette=" + StyleMetrics.paletteRevision; enabled: allAction.enabled; onTriggered: appController.judge("missing") }
    Action { id: failedAction; text: qsTranslate("LemonLime", "Judge &Compile Failed"); icon.source: "image://lemon-icons/deletecell?palette=" + StyleMetrics.paletteRevision; enabled: allAction.enabled; onTriggered: appController.judge("failed") }
    Action { id: cleanupAction; text: qsTranslate("LemonLime", "Clean Up &Files"); icon.source: "image://lemon-icons/view-sort?palette=" + StyleMetrics.paletteRevision; enabled: saveAction.enabled && window.page === 1; onTriggered: cleanupDialog.open() }
    Action { id: refreshAction; text: qsTranslate("LemonLime", "&Refresh"); icon.source: "image://lemon-icons/view-refresh?palette=" + StyleMetrics.paletteRevision; enabled: saveAction.enabled && window.page === 1; onTriggered: appController.refreshContestants() }
    Action { id: addTasksAction; text: qsTranslate("LemonLime", "Add &Tasks Automatically"); icon.source: "image://lemon-icons/layer-new?palette=" + StyleMetrics.paletteRevision; enabled: saveAction.enabled; onTriggered: { page = 0; tasks.addTasks() } }
    Action { id: exportAction; text: qsTranslate("LemonLime", "&Export Result"); icon.source: "image://lemon-icons/document-export?palette=" + StyleMetrics.paletteRevision; enabled: saveAction.enabled; onTriggered: exportFile.open() }
    Action { id: statisticsAction; text: qsTranslate("LemonLime", "Export &Statistics"); icon.source: "image://lemon-icons/document-export?palette=" + StyleMetrics.paletteRevision; enabled: saveAction.enabled; onTriggered: statistics.exportReport() }
    Action { id: settingsAction; text: qsTranslate("LemonLime", "&Settings"); icon.source: "image://lemon-icons/configure?palette=" + StyleMetrics.paletteRevision; enabled: !appController.judging; onTriggered: window.settingsPage() }
    menuBar: Controls.MenuBar {
        Controls.Menu {
            title: qsTranslate("LemonLime", "&File")
            Controls.MenuItem { action: newAction }
            Controls.MenuItem { action: openAction }
            Controls.MenuItem { action: saveAction }
            Controls.MenuItem { action: closeAction }
            MenuSeparator {}
            Controls.MenuItem { action: folderAction }
            Controls.MenuItem { action: renameAction }
            MenuSeparator {}
            Controls.MenuItem { action: exitAction }
        }
        Controls.Menu {
            title: qsTranslate("LemonLime", "&Control")
            Controls.MenuItem { action: allAction }
            Controls.MenuItem { action: selectedAction }
            Controls.MenuItem { action: unjudgedAction }
            Controls.MenuItem { action: missingAction }
            Controls.MenuItem { action: failedAction }
            MenuSeparator {}
            Controls.MenuItem { action: cleanupAction }
            Controls.MenuItem { action: refreshAction }
            MenuSeparator {}
            Controls.MenuItem { action: addTasksAction }
            Controls.MenuItem { action: exportAction }
            MenuSeparator {}
            Controls.MenuItem { action: statisticsAction }
        }
        Controls.Menu { title: qsTranslate("LemonLime", "&Tools"); Controls.MenuItem { action: settingsAction } }
        Controls.Menu {
            title: qsTranslate("LemonLime", "&Help")
            Controls.Menu {
                title: qsTranslate("LemonLime", "&Guides")
                Controls.MenuItem { text: qsTranslate("LemonLime", "Manual"); icon.source: "image://lemon-icons/acrobat?palette=" + StyleMetrics.paletteRevision; onTriggered: manualFile.open() }
                MenuSeparator {}
                Controls.MenuItem { text: qsTranslate("LemonLime", "More"); icon.source: "image://lemon-icons/globe?palette=" + StyleMetrics.paletteRevision; onTriggered: Qt.openUrlExternally("https://github.com/Project-LemonLime/Project_LemonLime") }
            }
            MenuSeparator {}
            Controls.MenuItem { text: qsTranslate("LemonLime", "&About"); icon.source: "image://lemon-icons/help-about?palette=" + StyleMetrics.paletteRevision; onTriggered: aboutDialog.open() }
        }
    }
    Item {
        anchors.fill: parent; anchors.margins: 11
        visible: appController.hasContest
        readonly property int tabWidth: StyleMetrics.sizeFor("westtab", qsTranslate("LemonLime", "Contestants"), tabFont.font).width
        readonly property rect pageRect: StyleMetrics.tabRect(width, height, tabWidth, navigation.height, true)
        readonly property rect paneRect: StyleMetrics.tabRect(width, height, tabWidth, navigation.height, false)
        Label { id: tabFont; visible: false; font.pointSize: 10; font.bold: true }
        StyleItem { kind: "tabframe"; vertical: true; x: parent.paneRect.x; y: parent.paneRect.y; width: parent.paneRect.width; height: parent.paneRect.height }
        Column {
            id: navigation; width: parent.tabWidth
            Repeater {
                model: [qsTranslate("LemonLime", "Tasks"), qsTranslate("LemonLime", "Contestants"), qsTranslate("LemonLime", "Statistics")]
                delegate: Controls.TabButton {
                    required property int index
                    required property string modelData
                    width: navigation.width; text: modelData; vertical: true; font: tabFont.font
                    checked: window.page === index; first: index === 0; last: index === 2
                    onClicked: { window.page = index; if (index === 2) contestTools.refresh() }
                }
            }
        }
        StackLayout {
            x: parent.pageRect.x; y: parent.pageRect.y; width: parent.pageRect.width; height: parent.pageRect.height
            currentIndex: window.page
            TasksPage { id: tasks; controller: taskController; enabled: !appController.judging }
            ResultsPage { id: results; controller: appController; onCleanupRequested: cleanupDialog.open() }
            StatisticsPage { id: statistics; controller: contestTools }
        }
    }
    footer: Control {
        height: StyleMetrics.textHeight(window.font) + 5
        background: Rectangle { color: window.palette.window }
        Label { anchors.left: parent.left; anchors.leftMargin: 3; anchors.verticalCenter: parent.verticalCenter; text: window.statusText }
        StyleItem { kind: "sizegrip"; width: 16; height: 16; anchors.right: parent.right; anchors.bottom: parent.bottom }
    }
    ContestDialog { id: welcome; objectName: "welcomeDialog"; controller: appController; mode: "welcome"; transientParent: window }
    ContestDialog { id: newContest; objectName: "newContestDialog"; controller: appController; mode: "new"; transientParent: window }
    ContestDialog { id: openContest; objectName: "openContestDialog"; controller: appController; mode: "open"; transientParent: window }
    SettingsDialog { id: preferences; objectName: "settingsDialog"; controller: settingsController; transientParent: window }
    AddCompilerWizard { id: firstCompiler; controller: settingsController; transientParent: window; onAccepted: { settingsController.apply(); welcome.open() } onRejected: { settingsController.cancel(); welcome.open() } }
    JudgingProgressDialog { id: progress; objectName: "judgingProgressDialog"; controller: appController; transientParent: window }
    Controls.Dialog {
        id: renameDialog; width: 360; height: 125; title: qsTranslate("LemonLime", "Change Contest Name"); transientParent: window
        ColumnLayout {
            anchors.fill: parent; anchors.margins: 11; spacing: 6
            Label { text: qsTranslate("LemonLime", "Contest Name") }
            Controls.TextField { id: renameField; Layout.fillWidth: true }
            RowLayout { Layout.fillWidth: true; Item { Layout.fillWidth: true } Controls.Button { text: qsTranslate("QPlatformTheme", "OK"); onClicked: { appController.renameContest(renameField.text); renameDialog.accept() } } Controls.Button { text: qsTranslate("QPlatformTheme", "Cancel"); onClicked: renameDialog.reject() } }
        }
    }
    Controls.Dialog {
        id: message; width: 440; height: 170; title: "LemonLime"; transientParent: window
        ColumnLayout { anchors.fill: parent; anchors.margins: 11; Label { id: messageText; Layout.fillWidth: true; Layout.fillHeight: true; wrapMode: Text.Wrap } RowLayout { Item { Layout.fillWidth: true } Controls.Button { text: qsTranslate("QPlatformTheme", "Close"); onClicked: message.accept() } } }
    }
    Controls.Dialog {
        id: cleanupDialog; width: 440; height: 150; title: qsTranslate("LemonLime", "Clean up Files"); transientParent: window
        ColumnLayout {
            anchors.fill: parent; anchors.margins: 11; spacing: 6
            Label { Layout.fillWidth: true; Layout.fillHeight: true; wrapMode: Text.Wrap; text: qsTranslate("LemonLime", "Are you sure to Clean up Files?") + "\n" + qsTranslate("LemonLime", "Reading guide are recommended.") }
            RowLayout { Item { Layout.fillWidth: true } Controls.Button { text: qsTranslate("QPlatformTheme", "Yes"); onClicked: { cleanupDialog.accept(); backupDialog.open() } } Controls.Button { text: qsTranslate("QPlatformTheme", "No"); onClicked: cleanupDialog.reject() } Controls.Button { text: qsTranslate("QPlatformTheme", "Abort"); onClicked: cleanupDialog.reject() } }
        }
    }
    Controls.Dialog {
        id: backupDialog; width: 400; height: 130; title: qsTranslate("LemonLime", "Clean up Files"); transientParent: window
        ColumnLayout {
            anchors.fill: parent; anchors.margins: 11
            Label { Layout.fillWidth: true; Layout.fillHeight: true; text: qsTranslate("LemonLime", "Making backup files to dir <br> `%1'?").arg(contestTools.backupDirectoryName()); textFormat: Text.RichText }
            RowLayout { Item { Layout.fillWidth: true } Controls.Button { text: qsTranslate("QPlatformTheme", "Yes"); onClicked: { backupDialog.accept(); contestTools.organizeSources(true) } } Controls.Button { text: qsTranslate("QPlatformTheme", "No"); onClicked: { backupDialog.accept(); contestTools.organizeSources(false) } } Controls.Button { text: qsTranslate("QPlatformTheme", "Abort"); onClicked: backupDialog.reject() } }
        }
    }
    Controls.Dialog {
        id: aboutDialog; width: 450; height: 190; title: qsTranslate("LemonLime", "About LemonLime"); transientParent: window
        ColumnLayout { anchors.fill: parent; anchors.margins: 11; Label { Layout.fillWidth: true; Layout.fillHeight: true; wrapMode: Text.Wrap; text: "LemonLime " + appController.version + "\n\nA tiny judging environment for OI contest.\nBased on Lemon and LemonPlus.\nGPL-3.0-or-later" } Controls.Button { Layout.alignment: Qt.AlignRight; text: qsTranslate("QPlatformTheme", "OK"); onClicked: aboutDialog.accept() } }
    }
    FileDialog {
        id: exportFile; title: qsTranslate("ExportUtil", "Export Result"); fileMode: FileDialog.SaveFile
        nameFilters: (qsTranslate("ExportUtil", "HTML Document (*.html *.htm);;CSV (*.csv)") + (contestTools.xlsAvailable ? qsTranslate("ExportUtil", ";;Excel Workbook (*.xls)") : "")).split(";;")
        defaultSuffix: selectedNameFilter.index === 1 ? "csv" : selectedNameFilter.index === 2 ? "xls" : "html"
        onAccepted: {
            const file = selectedFile.toString().toLowerCase()
            contestTools.exportResults(selectedFile, file.endsWith(".htm") ? 1 : file.endsWith(".csv") ? 2 : file.endsWith(".xls") ? 3 : 0)
        }
    }
    FileDialog { id: manualFile; title: qsTranslate("LemonLime", "Manual"); fileMode: FileDialog.SaveFile; nameFilters: ["PDF (*.pdf)"]; defaultSuffix: "pdf"; onAccepted: appController.exportManual(selectedFile) }
    Window {
        id: splash; width: 450; height: 191; flags: Qt.SplashScreen; transientParent: window
        Image { anchors.fill: parent; source: "qrc:/logo/splash2.png"; fillMode: Image.PreserveAspectFit }
        Timer { interval: appController.splashDuration; running: splash.visible; onTriggered: { splash.hide(); window.showWelcome() } }
    }
    Connections {
        target: appController
        function onContestChanged() { window.page = 0 }
        function onNotification(text) { window.statusText = text; statusTimer.restart() }
        function onErrorOccurred(text) { window.showMessage(text) }
        function onMessageRequested(title, text) { message.title = title; window.showMessage(text) }
        function onJudgingChanged() { if (appController.judging) progress.open(); else progress.hide() }
    }
    Timer { id: statusTimer; interval: 1000; onTriggered: window.statusText = "" }
    Connections { target: taskController; function onErrorOccurred(text) { window.showMessage(text) } }
    Connections { target: settingsController; function onErrorChanged() { if (settingsController.error.length) window.showMessage(settingsController.error) } }
    Connections { target: contestTools; function onErrorOccurred(text) { window.showMessage(text) } function onNotification(text) { window.statusText = text; statusTimer.restart() } }
}
