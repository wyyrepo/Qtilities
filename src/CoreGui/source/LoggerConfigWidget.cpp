/****************************************************************************
**
** Copyright (c) 2009-2010, Jaco Naude
**
** This file is part of Qtilities which is released under the following
** licensing options.
**
** Option 1: Open Source
** Under this license Qtilities is free software: you can
** redistribute it and/or modify it under the terms of the GNU General
** Public License as published by the Free Software Foundation, either
** version 3 of the License, or (at your option) any later version.
**
** Qtilities is distributed in the hope that it will be useful,
** but WITHOUT ANY WARRANTY; without even the implied warranty of
** MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
** GNU General Public License for more details.
**
** You should have received a copy of the GNU General Public License
** along with Qtilities. If not, see http://www.gnu.org/licenses/.
**
** Option 2: Commercial
** Alternatively, this library is also released under a commercial license
** that allows the development of closed source proprietary applications
** without restrictions on licensing. For more information on this option,
** please see the project website's licensing page:
** http://www.qtilities.org/licensing.html
**
** If you are unsure which license is appropriate for your use, please
** contact support@qtilities.org.
**
****************************************************************************/

#include "LoggerConfigWidget.h"
#include "ui_LoggerConfigWidget.h"
#include "Logger.h"
#include "LoggingConstants.h"
#include "LoggerEnginesTableModel.h"
#include "QtilitiesCoreGuiConstants.h"

#include <AbstractLoggerEngine.h>
#include <LoggingConstants.h>

#include <QString>
#include <QInputDialog>
#include <QHBoxLayout>
#include <QTableWidgetItem>
#include <QFileDialog>

using namespace Qtilities::CoreGui::Constants;
using namespace Qtilities::Logging;
using namespace Qtilities::Logging::Constants;

struct Qtilities::CoreGui::LoggerConfigWidgetData {
    LoggerConfigWidgetData() : active_engine(0) {}

    LoggerEnginesTableModel logger_engine_model;
    AbstractLoggerEngine* active_engine;
};

Qtilities::CoreGui::LoggerConfigWidget::LoggerConfigWidget(bool applyButtonVisisble, QWidget *parent) :
    QWidget(parent),
    ui(new Ui::LoggerConfigWidget)
{
    ui->setupUi(this); 
    ui->btnApply->setVisible(applyButtonVisisble);
    setObjectName(tr("Logger Config Widget"));

    d = new LoggerConfigWidgetData;
    connect(ui->tableViewLoggerEngines,SIGNAL(clicked(QModelIndex)),SLOT(handle_LoggerEngineTableClicked(QModelIndex)));
    connect(ui->btnAddLoggerEngine,SIGNAL(clicked(bool)),SLOT(handle_NewLoggerEngineRequest()));
    connect(ui->btnRemoveLoggerEngine,SIGNAL(clicked(bool)),SLOT(handle_RemoveLoggerEngineRequest()));
    connect(ui->checkBoxToggleAll,SIGNAL(clicked(bool)),SLOT(handle_CheckBoxToggleAllClicked(bool)));
    connect(ui->listWidgetFormattingEngines,SIGNAL(currentRowChanged(int)),SLOT(handle_FormattingEnginesCurrentRowChanged(int)));
    connect(ui->comboBoxLoggerFormattingEngine,SIGNAL(currentIndexChanged(int)),SLOT(handle_ComboBoxLoggerFormattingEngineCurrentIndexChange(int)));
    connect(ui->btnLoadConfig,SIGNAL(clicked()),SLOT(handle_BtnLoadConfigClicked()));
    connect(ui->btnSaveConfig,SIGNAL(clicked()),SLOT(handle_BtnSaveConfigClicked()));
    connect(ui->btnApply,SIGNAL(clicked()),SLOT(handle_BtnApplyClicked()));

    // Add log levels:
    QStringList list = Log->allLogLevelStrings();
    list.pop_back();
    ui->comboGlobalLogLevel->addItems(list);

    // Add logger engines:
    ui->tableViewLoggerEngines->setModel(&d->logger_engine_model);
    ui->tableViewLoggerEngines->verticalHeader()->setVisible(false);
    for (int i = 0; i < Log->attachedLoggerEngineCount(); i++)
        ui->tableViewLoggerEngines->setRowHeight(i,17);
    ui->tableViewLoggerEngines->horizontalHeader()->setStretchLastSection(true);
    if (Log->attachedLoggerEngineCount() >= 1) {
        ui->tableViewLoggerEngines->setCurrentIndex(d->logger_engine_model.index(0,0));
        d->active_engine = Log->loggerEngineReferenceAt(0);
        refreshLoggerEngineInformation();
    }

    // Add formatting engines:
    ui->listWidgetFormattingEngines->addItems(Log->attachedFormattingEngineNames());
    ui->comboBoxLoggerFormattingEngine->addItems(Log->attachedFormattingEngineNames());

    // Read the logging settings
    readSettings();

    connect(ui->comboGlobalLogLevel,SIGNAL(currentIndexChanged(QString)),SLOT(handle_ComboBoxGlobalLogLevelCurrentIndexChange(QString)));
    connect(ui->checkBoxRememberSession,SIGNAL(clicked(bool)),SLOT(handle_CheckBoxRememberSessionConfigClicked(bool)));
}

Qtilities::CoreGui::LoggerConfigWidget::~LoggerConfigWidget()
{
    delete ui;
    delete d;
}

QIcon Qtilities::CoreGui::LoggerConfigWidget::configPageIcon() const {
    return QIcon();//Constants::ICON_SHORTCUTS_22x22);
}

QWidget* Qtilities::CoreGui::LoggerConfigWidget::configPageWidget() {
    return this;
}

QStringList Qtilities::CoreGui::LoggerConfigWidget::configPageTitle() const {
    QStringList text;
    text << tr("General") << tr("Logging");
    return text;
}

void Qtilities::CoreGui::LoggerConfigWidget::configPageApply() {
    handle_BtnApplyClicked();
}

void Qtilities::CoreGui::LoggerConfigWidget::setApplyButtonVisible(bool visible) {
    ui->btnApply->setVisible(visible);
}

void Qtilities::CoreGui::LoggerConfigWidget::handle_NewLoggerEngineRequest() {
    bool ok;
    QString new_item_selection = QInputDialog::getItem(this, tr("What type of logger engine would you like to add?"),tr("Available Logger Engines:"), Log->availableLoggerEngines(), 0, false, &ok);
    QString engine_name = QInputDialog::getText(this, tr("Name of new engine:"),tr("Engine Name:"), QLineEdit::Normal, "new_logger_engine", &ok);

    if (ok && !new_item_selection.isEmpty() && !engine_name.isEmpty()) {
        // Handle new widget
        if (new_item_selection == "File") {
            // Prompt the correct file extensions and select the formatting engine according to the user's selection.
            QString file_ext = "";
            for (int i = 0; i < Log->availableFormattingEngines().count(); i++) {
                AbstractFormattingEngine* engine = Log->formattingEngineReference(Log->availableFormattingEngines().at(i));
                if (engine) {
                    if ((!engine->fileExtension().isEmpty()) && (!engine->name().isEmpty()))
                        file_ext.append(QString("%1 (*.%2);;").arg(engine->name()).arg(engine->fileExtension()));
                }
            }

            QString fileName = QFileDialog::getSaveFileName(this,tr("Select Output File"),QApplication::applicationDirPath(),file_ext);
            if (!fileName.isEmpty()) {
                Log->newFileEngine(engine_name,fileName,QString());
            }
        }
    }
}

void Qtilities::CoreGui::LoggerConfigWidget::handle_RemoveLoggerEngineRequest() {
    if (Log->detachLoggerEngine(d->active_engine)) {
        if (Log->attachedLoggerEngineCount() >= 1) {
            ui->tableViewLoggerEngines->setCurrentIndex(d->logger_engine_model.index(0,0));
            d->active_engine = Log->loggerEngineReferenceAt(0);
            refreshLoggerEngineInformation();
        }
    }
}

void Qtilities::CoreGui::LoggerConfigWidget::handle_LoggerEngineTableClicked(const QModelIndex& index) {
    // Get the engine at the position:
    if (index.row() < 0 or index.row() >= Log->attachedLoggerEngineCount()) {
        d->active_engine = 0;
    } else {
        d->active_engine = Log->loggerEngineReferenceAt(index.row());
    }

    refreshLoggerEngineInformation();
}

void Qtilities::CoreGui::LoggerConfigWidget::handle_FormattingEnginesCurrentRowChanged(int currentRow) {
    AbstractFormattingEngine* engine = Log->formattingEngineReferenceAt(currentRow);
    if (engine) {
        // We prepare a formatted message to show how the message will look:
        QString preview_string;
        preview_string.append(engine->initializeString());
        if (!engine->initializeString().isEmpty())
            preview_string.append(engine->endOfLineChar());
        QList<QVariant> message;
        message.push_front(QVariant(tr("Information Message Example")));
        preview_string.append(engine->formatMessage(Logger::Info,message));
        preview_string.append(engine->endOfLineChar());
        message.clear();
        message.push_front(QVariant(tr("Warning Message Example")));
        preview_string.append(engine->formatMessage(Logger::Warning,message));
        preview_string.append(engine->endOfLineChar());
        message.clear();
        message.push_front(QVariant(tr("Error Message Example")));
        preview_string.append(engine->formatMessage(Logger::Error,message));
        preview_string.append(engine->endOfLineChar());
        message.clear();
        message.push_front(QVariant(tr("Fatal Message Example")));
        preview_string.append(engine->formatMessage(Logger::Fatal,message));
        preview_string.append(engine->endOfLineChar());
        message.clear();
        message.push_front(QVariant(tr("Debug Message Example")));
        preview_string.append(engine->formatMessage(Logger::Debug,message));
        preview_string.append(engine->endOfLineChar());
        message.clear();
        message.push_front(QVariant(tr("Trace Message Example")));
        preview_string.append(engine->formatMessage(Logger::Trace,message));
        preview_string.append(engine->endOfLineChar());
        if (!engine->finalizeString().isEmpty())
            preview_string.append(engine->endOfLineChar());
        preview_string.append(engine->finalizeString());
        ui->txtFormattingEnginePreview->setText(preview_string);
    } else {
        ui->txtFormattingEnginePreview->clear();
    }
}

void Qtilities::CoreGui::LoggerConfigWidget::handle_ComboBoxLoggerFormattingEngineCurrentIndexChange(int index) {
    // We need to get the selected formatting engine and change the active logger engine.
    AbstractFormattingEngine* new_formatting_engine = Log->formattingEngineReferenceAt(index);
    if (new_formatting_engine && d->active_engine) {
        d->active_engine->installFormattingEngine(new_formatting_engine);
    }
}

void Qtilities::CoreGui::LoggerConfigWidget::handle_CheckBoxToggleAllClicked(bool checked) {
    if (checked) {
        Log->enableAllLoggerEngines();
    } else {
        Log->disableAllLoggerEngines();
    }
    d->logger_engine_model.requestRefresh();
}

void Qtilities::CoreGui::LoggerConfigWidget::handle_CheckBoxRememberSessionConfigClicked(bool checked) {
    Log->setRememberSessionConfig(checked);
}

void Qtilities::CoreGui::LoggerConfigWidget::handle_ComboBoxGlobalLogLevelCurrentIndexChange(const QString& text) {
    Log->setGlobalLogLevel(Log->stringToLogLevel(text));
}

void Qtilities::CoreGui::LoggerConfigWidget::handle_BtnSaveConfigClicked() {
    QString filter = tr("Log Configurations (*") + FILE_EXT_LOGGER_CONFIG + ")";
    QString session_log_path = QApplication::applicationDirPath();
    QString output_file = QFileDialog::getSaveFileName(0, tr("Save log configuration to:"), session_log_path, filter);
    if (output_file.isEmpty())
        return;
    else {
        Log->saveSessionConfig(output_file);
    }
}

void Qtilities::CoreGui::LoggerConfigWidget::handle_BtnLoadConfigClicked() {
    QString filter = tr("Log Configurations (*") + FILE_EXT_LOGGER_CONFIG + ")";
    QString session_log_path = QApplication::applicationDirPath();
    QString input_file = QFileDialog::getOpenFileName(0, tr("Select log configuration to load:"), session_log_path, filter);
    if (input_file.isEmpty())
        return;
    else {
        Log->loadSessionConfig(input_file);
    }
}

void Qtilities::CoreGui::LoggerConfigWidget::handle_BtnApplyClicked() {
    writeSettings();
    Log->saveSessionConfig();
}

void Qtilities::CoreGui::LoggerConfigWidget::writeSettings() {
    // Store settings using QSettings only if it was initialized
    QSettings settings;
    settings.beginGroup("Session Log");
    settings.beginGroup("General");
    settings.setValue("global_log_level", QVariant(Log->globalLogLevel()));
    settings.setValue("remember_session_config", QVariant(Log->rememberSessionConfig()));
    settings.endGroup();
    settings.endGroup();
}

void Qtilities::CoreGui::LoggerConfigWidget::readSettings() {
    if (QCoreApplication::organizationName().isEmpty() || QCoreApplication::organizationDomain().isEmpty() || QCoreApplication::applicationName().isEmpty())
        qDebug() << tr("The logger may not be able to restore paramaters from previous sessions since the correct details in QCoreApplication have not been set.");

    // Load logging paramaters using QSettings()
    QSettings settings;
    settings.beginGroup("Session Log");
    settings.beginGroup("General");
    QVariant log_level =  settings.value("global_log_level", Logger::Fatal);
    Logger::MessageType global_type = (Logger::MessageType) log_level.toInt();
    ui->comboGlobalLogLevel->setCurrentIndex(ui->comboGlobalLogLevel->findText(Log->logLevelToString(global_type)));
    if (settings.value("remember_session_config", true).toBool())
        ui->checkBoxRememberSession->setChecked(true);
    else
        ui->checkBoxRememberSession->setChecked(false);

    settings.endGroup();
    settings.endGroup();
}

void Qtilities::CoreGui::LoggerConfigWidget::refreshLoggerEngineInformation() {
    if (!d->active_engine) {
        ui->txtLoggerEngineStatus->setPlainText(QString());
        ui->txtLoggerEngineDescription->setPlainText(QString());
        ui->comboBoxLoggerFormattingEngine->setEnabled(false);
        ui->btnRemoveLoggerEngine->setEnabled(false);
        return;
    }

    // Status:
    ui->txtLoggerEngineStatus->setPlainText(d->active_engine->status());

    // Description:
    ui->txtLoggerEngineDescription->setPlainText(d->active_engine->description());

    // Formatting Engine:
    ui->comboBoxLoggerFormattingEngine->setCurrentIndex(ui->comboBoxLoggerFormattingEngine->findText(d->active_engine->formattingEngineName()));
    if (d->active_engine->isFormattingEngineConstant())
        ui->comboBoxLoggerFormattingEngine->setEnabled(false);
    else
        ui->comboBoxLoggerFormattingEngine->setEnabled(true);

    // Remove Button:
    if (d->active_engine->removable())
        ui->btnRemoveLoggerEngine->setEnabled(true);
    else
        ui->btnRemoveLoggerEngine->setEnabled(false);

    // Make all rows the same height:
    for (int i = 0; i < Log->attachedLoggerEngineCount(); i++)
        ui->tableViewLoggerEngines->setRowHeight(i,17);
}

void Qtilities::CoreGui::LoggerConfigWidget::changeEvent(QEvent *e)
{
    QWidget::changeEvent(e);
    switch (e->type()) {
    case QEvent::LanguageChange:
        ui->retranslateUi(this);
        break;
    default:
        break;
    }
}