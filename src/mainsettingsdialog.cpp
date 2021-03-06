#include <QSettings>
#include <QDir>
#include <QFileDialog>
#include <QLocale>
#include <QTranslator>
#include <QListIterator>
#include <QStringList>
#include <QTableWidgetItem>
#include <QMapIterator>
#include <QVariant>
#include <QDebug>
#include <QMessageBox>

#include "mainsettingsdialog.h"
#include "ui_mainsettingsdialog.h"

#include "addeditautoprofiledialog.h"
#include "editalldefaultautoprofiledialog.h"
#include "common.h"

MainSettingsDialog::MainSettingsDialog(QSettings *settings, QList<InputDevice *> *devices, QWidget *parent) :
    QDialog(parent, Qt::Dialog),
    ui(new Ui::MainSettingsDialog)
{
    ui->setupUi(this);
    setAttribute(Qt::WA_DeleteOnClose);

    this->settings = settings;
    this->allDefaultProfile = 0;
    this->connectedDevices = devices;

#ifdef USE_SDL_2
    fillControllerMappingsTable();
#endif

    QString defaultProfileDir = settings->value("DefaultProfileDir", "").toString();
    int numberRecentProfiles = settings->value("NumberRecentProfiles", 5).toInt();
    bool closeToTray = settings->value("CloseToTray", false).toBool();

    if (!defaultProfileDir.isEmpty() && QDir(defaultProfileDir).exists())
    {
        ui->profileDefaultDirLineEdit->setText(defaultProfileDir);
    }

    ui->numberRecentProfileSpinBox->setValue(numberRecentProfiles);

    if (closeToTray)
    {
        ui->closeToTrayCheckBox->setChecked(true);
    }

    findLocaleItem();

#ifdef USE_SDL_2
    populateAutoProfiles();
    fillAllAutoProfilesTable();
    fillGUIDComboBox();
#else
    delete ui->categoriesListWidget->item(3);
    delete ui->categoriesListWidget->item(1);
    ui->stackedWidget->removeWidget(ui->controllerMappingsPage);
    ui->stackedWidget->removeWidget(ui->page_2);
#endif

    delete ui->categoriesListWidget->item(2);
    ui->stackedWidget->removeWidget(ui->page);

    QString autoProfileActive = settings->value("AutoProfiles/AutoProfilesActive", "").toString();
    if (!autoProfileActive.isEmpty() && autoProfileActive == "1")
    {
        ui->activeCheckBox->setChecked(true);
        ui->autoProfileTableWidget->setEnabled(true);
    }

    connect(ui->categoriesListWidget, SIGNAL(currentRowChanged(int)), ui->stackedWidget, SLOT(setCurrentIndex(int)));
    connect(ui->controllerMappingsTableWidget, SIGNAL(itemChanged(QTableWidgetItem*)), this, SLOT(mappingsTableItemChanged(QTableWidgetItem*)));
    connect(ui->mappingDeletePushButton, SIGNAL(clicked()), this, SLOT(deleteMappingRow()));
    connect(ui->mappngInsertPushButton, SIGNAL(clicked()), this, SLOT(insertMappingRow()));
    //connect(ui->buttonBox, SIGNAL(accepted()), this, SLOT(syncMappingSettings()));
    connect(this, SIGNAL(accepted()), this, SLOT(saveNewSettings()));
    connect(ui->profileOpenDirPushButton, SIGNAL(clicked()), this, SLOT(selectDefaultProfileDir()));
    connect(ui->activeCheckBox, SIGNAL(toggled(bool)), ui->autoProfileTableWidget, SLOT(setEnabled(bool)));
    //connect(ui->activeCheckBox, SIGNAL(toggled(bool)), ui->devicesComboBox, SLOT(setEnabled(bool)));
    connect(ui->devicesComboBox, SIGNAL(activated(int)), this, SLOT(changeDeviceForProfileTable(int)));
    connect(ui->autoProfileTableWidget, SIGNAL(itemChanged(QTableWidgetItem*)), this, SLOT(processAutoProfileActiveClick(QTableWidgetItem*)));
    connect(ui->autoProfileAddPushButton, SIGNAL(clicked()), this, SLOT(openAddAutoProfileDialog()));
    connect(ui->autoProfileDeletePushButton, SIGNAL(clicked()), this, SLOT(openDeleteAutoProfileConfirmDialog()));
    connect(ui->autoProfileEditPushButton, SIGNAL(clicked()), this, SLOT(openEditAutoProfileDialog()));
    connect(ui->autoProfileTableWidget, SIGNAL(itemSelectionChanged()), this, SLOT(changeAutoProfileButtonsState()));
}

MainSettingsDialog::~MainSettingsDialog()
{
    delete ui;
    if (connectedDevices)
    {
        delete connectedDevices;
        connectedDevices = 0;
    }
}

void MainSettingsDialog::fillControllerMappingsTable()
{
    /*QList<QVariant> tempvariant = bindingValues(bind);
    QTableWidgetItem* item = new QTableWidgetItem();
    ui->buttonMappingTableWidget->setItem(associatedRow, 0, item);
    item->setText(temptext);
    item->setData(Qt::UserRole, tempvariant);
    */

#if (QT_VERSION >= QT_VERSION_CHECK(5, 0, 0))
    ui->controllerMappingsTableWidget->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
#else
    ui->controllerMappingsTableWidget->horizontalHeader()->setResizeMode(QHeaderView::Stretch);
#endif

    QHash<QString, QList<QVariant> > tempHash;

    settings->beginGroup("Mappings");

    QStringList mappings = settings->allKeys();
    QStringListIterator iter(mappings);
    while (iter.hasNext())
    {
        QString tempkey = iter.next();
        QString tempGUID;

        if (tempkey.contains("Disable"))
        {
            bool disableGameController = settings->value(tempkey, false).toBool();
            tempGUID = tempkey.remove("Disable");
            insertTempControllerMapping(tempHash, tempGUID);
            if (tempHash.contains(tempGUID))
            {
                QList<QVariant> templist = tempHash.value(tempGUID);
                templist.replace(2, QVariant(disableGameController));
                tempHash.insert(tempGUID, templist); // Overwrite original list
            }
        }
        else
        {
            QString mappingString = settings->value(tempkey, QString()).toString();
            if (!mappingString.isEmpty())
            {
                tempGUID = tempkey;
                insertTempControllerMapping(tempHash, tempGUID);
                if (tempHash.contains(tempGUID))
                {
                    QList<QVariant> templist = tempHash.value(tempGUID);
                    templist.replace(1, mappingString);
                    tempHash.insert(tempGUID, templist); // Overwrite original list
                }
            }
        }
    }

    settings->endGroup();

    QHashIterator<QString, QList<QVariant> > iter2(tempHash);
    int i = 0;
    while (iter2.hasNext())
    {
        ui->controllerMappingsTableWidget->insertRow(i);

        QList<QVariant> templist = iter2.next().value();
        QTableWidgetItem* item = new QTableWidgetItem(templist.at(0).toString());
        item->setFlags(item->flags() & ~Qt::ItemIsEditable);
        item->setData(Qt::UserRole, iter2.key());
        item->setToolTip(templist.at(0).toString());
        ui->controllerMappingsTableWidget->setItem(i, 0, item);

        item = new QTableWidgetItem(templist.at(1).toString());
        item->setFlags(item->flags() & ~Qt::ItemIsEditable);
        item->setData(Qt::UserRole, iter2.key());
        //item->setToolTip(templist.at(1).toString());
        ui->controllerMappingsTableWidget->setItem(i, 1, item);

        bool disableController = templist.at(2).toBool();
        item = new QTableWidgetItem();
        item->setCheckState(disableController ? Qt::Checked : Qt::Unchecked);
        item->setData(Qt::UserRole, iter2.key());
        ui->controllerMappingsTableWidget->setItem(i, 2, item);

        i++;
    }
}

void MainSettingsDialog::insertTempControllerMapping(QHash<QString, QList<QVariant> > &hash, QString newGUID)
{
    if (!newGUID.isEmpty() && !hash.contains(newGUID))
    {
        QList<QVariant> templist;
        templist.append(QVariant(newGUID));
        templist.append(QVariant(""));
        templist.append(QVariant(false));

        hash.insert(newGUID, templist);
    }
}

void MainSettingsDialog::mappingsTableItemChanged(QTableWidgetItem *item)
{
    int column = item->column();
    int row = item->row();

    if (column == 0 && !item->text().isEmpty())
    {
        QTableWidgetItem *disableitem = ui->controllerMappingsTableWidget->item(row, column);
        if (disableitem)
        {
            disableitem->setData(Qt::UserRole, item->text());
        }

        item->setData(Qt::UserRole, item->text());
    }
}

void MainSettingsDialog::insertMappingRow()
{
    int insertRowIndex = ui->controllerMappingsTableWidget->rowCount();
    ui->controllerMappingsTableWidget->insertRow(insertRowIndex);

    QTableWidgetItem* item = new QTableWidgetItem();
    //item->setFlags(item->flags() & ~Qt::ItemIsEditable);
    //item->setData(Qt::UserRole, iter2.key());
    ui->controllerMappingsTableWidget->setItem(insertRowIndex, 0, item);

    item = new QTableWidgetItem();
    item->setFlags(item->flags() & ~Qt::ItemIsEditable);
    //item->setData(Qt::UserRole, iter2.key());
    ui->controllerMappingsTableWidget->setItem(insertRowIndex, 1, item);

    item = new QTableWidgetItem();
    item->setCheckState(Qt::Unchecked);
    ui->controllerMappingsTableWidget->setItem(insertRowIndex, 2, item);
}

void MainSettingsDialog::deleteMappingRow()
{
    int row = ui->controllerMappingsTableWidget->currentRow();

    if (row >= 0)
    {
        ui->controllerMappingsTableWidget->removeRow(row);
    }
}

void MainSettingsDialog::syncMappingSettings()
{
    settings->beginGroup("Mappings");
    settings->remove("");

    for (int i=0; i < ui->controllerMappingsTableWidget->rowCount(); i++)
    {
        QTableWidgetItem *itemGUID = ui->controllerMappingsTableWidget->item(i, 0);
        QTableWidgetItem *itemMapping = ui->controllerMappingsTableWidget->item(i, 1);
        QTableWidgetItem *itemDisable = ui->controllerMappingsTableWidget->item(i, 2);

        if (itemGUID && !itemGUID->text().isEmpty() && itemDisable)
        {
            bool disableController = itemDisable->checkState() == Qt::Checked ? true : false;
            if (itemMapping && !itemMapping->text().isEmpty())
            {
                settings->setValue(itemGUID->text(), itemMapping->text());
            }

            settings->setValue(QString("%1Disable").arg(itemGUID->text()), disableController);
        }
    }

    settings->endGroup();
}

void MainSettingsDialog::saveNewSettings()
{
    //QSettings settings(PadderCommon::configFilePath, QSettings::IniFormat);
#ifdef USE_SDL_2
    syncMappingSettings();
#endif

    QString oldProfileDir = settings->value("DefaultProfileDir", "").toString();
    QString possibleProfileDir = ui->profileDefaultDirLineEdit->text();
    bool closeToTray = ui->closeToTrayCheckBox->isChecked();

    if (oldProfileDir != possibleProfileDir)
    {
        if (QFileInfo(possibleProfileDir).exists())
        {
            settings->setValue("DefaultProfileDir", possibleProfileDir);
        }
        else if (possibleProfileDir.isEmpty())
        {
            settings->remove("DefaultProfileDir");
        }
    }

    int numRecentProfiles = ui->numberRecentProfileSpinBox->value();
    settings->setValue("NumberRecentProfiles", numRecentProfiles);

    if (closeToTray)
    {
        settings->setValue("CloseToTray", closeToTray ? "1" : "0");
    }
    else
    {
        settings->remove("CloseToTray");
    }
    //checkLocaleChange();
#ifdef USE_SDL_2
    saveAutoProfileSettings();
#endif

    settings->sync();
}

void MainSettingsDialog::selectDefaultProfileDir()
{
    QString lookupDir = QDir::homePath();
    QString directory = QFileDialog::getExistingDirectory(this, tr("Select Default Profile Directory"), lookupDir);
    if (!directory.isEmpty() && QFileInfo(directory).exists())
    {
        ui->profileDefaultDirLineEdit->setText(directory);
    }
}

void MainSettingsDialog::checkLocaleChange()
{
    int row = ui->localeListWidget->currentRow();
    if (row == 0)
    {
        if (settings->contains("Language"))
        {
            settings->remove("Language");
        }

        changeLanguage(QLocale::system().name());
    }
    else
    {
        QString newLocale = "en";
        if (row == 1)
        {
            newLocale = "en";
        }
        else if (row == 2)
        {
            newLocale = "br";
        }
        else if (row == 3)
        {
            newLocale = "fr";
        }

        /*QTranslator myappTranslator;
#if defined(Q_OS_UNIX)
        myappTranslator.load("antimicro_" + newLocale, QApplication::applicationDirPath().append("/../share/antimicro/translations"));
#elif defined(Q_OS_WIN)
        myappTranslator.load("antimicro_" + newLocale, QApplication::applicationDirPath().append("\\share\\antimicro\\translations"));
#endif
        qApp->removeTranslator();
        qApp->installTranslator(&myappTranslator);*/
        settings->setValue("Language", newLocale);

        emit changeLanguage(newLocale);
    }
}

void MainSettingsDialog::findLocaleItem()
{
    QLocale::Language currentLocale = QLocale().language();
    QLocale::Language systemLocale = QLocale::system().language();

    if (currentLocale == systemLocale)
    {
        ui->localeListWidget->setCurrentRow(0);
    }
    else if (currentLocale == QLocale::English)
    {
        ui->localeListWidget->setCurrentRow(1);
    }
    else if (currentLocale == QLocale::Portuguese)
    {
        ui->localeListWidget->setCurrentRow(2);
    }
}

void MainSettingsDialog::populateAutoProfiles()
{
    exeAutoProfiles.clear();
    defaultAutoProfiles.clear();

    settings->beginGroup("DefaultAutoProfiles");
    QStringList defaultkeys = settings->allKeys();
    settings->endGroup();

    QString allProfile = settings->value(QString("DefaultAutoProfileAll/Profile"), "").toString();
    QString allActive = settings->value(QString("DefaultAutoProfileAll/Active"), "0").toString();

    if (!allProfile.isEmpty())
    {
        bool defaultActive = allActive == "1" ? true : false;
        allDefaultProfile = new AutoProfileInfo("all", allProfile, defaultActive, this);
        allDefaultProfile->setDefaultState(true);
    }
    else
    {
        allDefaultProfile = new AutoProfileInfo("all", "", false, this);
        allDefaultProfile->setDefaultState(true);
    }

    QStringListIterator iter(defaultkeys);
    while (iter.hasNext())
    {
        QString tempkey = iter.next();
        QString guid = QString(tempkey).replace("GUID", "");

        QString profile = settings->value(QString("DefaultAutoProfile-%1/Profile").arg(guid), "").toString();
        QString active = settings->value(QString("DefaultAutoProfile-%1/Active").arg(guid), "0").toString();
        QString deviceName = settings->value(QString("DefaultAutoProfile-%1/DeviceName").arg(guid), "").toString();

        if (!guid.isEmpty() && !profile.isEmpty() && !deviceName.isEmpty())
        {
            bool profileActive = active == "1" ? true : false;
            if (!defaultAutoProfiles.contains(guid) && guid != "all")
            {
                AutoProfileInfo *info = new AutoProfileInfo(guid, profile, profileActive, this);
                info->setDefaultState(true);
                info->setDeviceName(deviceName);
                defaultAutoProfiles.insert(guid, info);
                defaultList.append(info);
                QList<AutoProfileInfo*> templist;
                templist.append(info);
                deviceAutoProfiles.insert(guid, templist);
            }
        }
    }

    settings->beginGroup("AutoProfiles");
    bool quitSearch = false;

    QHash<QString, QList<QString> > tempAssociation;
    for (int i = 1; !quitSearch; i++)
    {
        QString exe = settings->value(QString("AutoProfile%1Exe").arg(i), "").toString();
        QString guid = settings->value(QString("AutoProfile%1GUID").arg(i), "").toString();
        QString profile = settings->value(QString("AutoProfile%1Profile").arg(i), "").toString();
        QString active = settings->value(QString("AutoProfile%1Active").arg(i), 0).toString();
        QString deviceName = settings->value(QString("AutoProfile%1DeviceName").arg(i), "").toString();

        // Check if all required elements exist. If not, assume that the end of the
        // list has been reached.
        if (!exe.isEmpty() && !guid.isEmpty() && !profile.isEmpty() && !deviceName.isEmpty())
        {
            bool profileActive = active == "1" ? true : false;
            QList<AutoProfileInfo*> templist;
            if (exeAutoProfiles.contains(exe))
            {
                templist = exeAutoProfiles.value(exe);
            }

            QList<QString> tempguids;
            if (tempAssociation.contains(exe))
            {
                tempguids = tempAssociation.value(exe);
            }

            if (!tempguids.contains(guid) && guid != "all")
            {
                AutoProfileInfo *info = new AutoProfileInfo(guid, profile, exe, profileActive, this);
                info->setDeviceName(deviceName);
                tempguids.append(guid);
                tempAssociation.insert(exe, tempguids);
                templist.append(info);
                exeAutoProfiles.insert(exe, templist);
                profileList.append(info);
                QList<AutoProfileInfo*> templist;
                if (deviceAutoProfiles.contains(guid))
                {
                    templist = deviceAutoProfiles.value(guid);
                }
                templist.append(info);
                deviceAutoProfiles.insert(guid, templist);
            }
        }
        else
        {
            quitSearch = true;
        }
    }

    settings->endGroup();
}

void MainSettingsDialog::fillAutoProfilesTable(QString guid)
{
    //ui->autoProfileTableWidget->clear();
    for (int i = ui->autoProfileTableWidget->rowCount()-1; i >= 0; i--)
    {
        ui->autoProfileTableWidget->removeRow(i);
    }

    //QStringList tableHeader;
    //tableHeader << tr("Active") << tr("GUID") << tr("Profile") << tr("Application") << tr("Default?")
    //            << tr("Instance");
    //ui->autoProfileTableWidget->setHorizontalHeaderLabels(tableHeader);
    //ui->autoProfileTableWidget->horizontalHeader()->setVisible(true);

#if (QT_VERSION >= QT_VERSION_CHECK(5, 0, 0))
    ui->autoProfileTableWidget->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
#else
    ui->autoProfileTableWidget->horizontalHeader()->setResizeMode(QHeaderView::Stretch);
#endif

    ui->autoProfileTableWidget->hideColumn(5);

    if (defaultAutoProfiles.contains(guid) ||
        deviceAutoProfiles.contains(guid))
    {
        int i = 0;

        AutoProfileInfo *defaultForGUID = 0;
        if (defaultAutoProfiles.contains(guid))
        {
            AutoProfileInfo *info = defaultAutoProfiles.value(guid);
            defaultForGUID = info;
            ui->autoProfileTableWidget->insertRow(i);

            QTableWidgetItem *item = new QTableWidgetItem();
            item->setCheckState(info->isActive() ? Qt::Checked : Qt::Unchecked);

            ui->autoProfileTableWidget->setItem(i, 0, item);

            QString deviceName = info->getDeviceName();
            QString guidDisplay = info->getGUID();
            if (!deviceName.isEmpty())
            {
                guidDisplay = QString("%1 ").arg(info->getDeviceName());
                guidDisplay.append(QString("(%1)").arg(info->getGUID()));
            }
            item = new QTableWidgetItem(guidDisplay);
            item->setFlags(item->flags() & ~Qt::ItemIsEditable);
            item->setData(Qt::UserRole, info->getGUID());
            item->setToolTip(info->getGUID());
            ui->autoProfileTableWidget->setItem(i, 1, item);

            QFileInfo profilePath(info->getProfileLocation());
            item = new QTableWidgetItem(profilePath.fileName());
            item->setFlags(item->flags() & ~Qt::ItemIsEditable);
            item->setData(Qt::UserRole, info->getProfileLocation());
            item->setToolTip(info->getProfileLocation());
            ui->autoProfileTableWidget->setItem(i, 2, item);

            QFileInfo exeInfo(info->getExe());
            item = new QTableWidgetItem(exeInfo.fileName());
            item->setFlags(item->flags() & ~Qt::ItemIsEditable);
            item->setData(Qt::UserRole, info->getExe());
            item->setToolTip(info->getExe());
            ui->autoProfileTableWidget->setItem(i, 3, item);

            item = new QTableWidgetItem("Default");
            item->setData(Qt::UserRole, "default");
            ui->autoProfileTableWidget->setItem(i, 4, item);

            item = new QTableWidgetItem("Instance");
            item->setData(Qt::UserRole, QVariant::fromValue<AutoProfileInfo*>(info));
            ui->autoProfileTableWidget->setItem(i, 5, item);

            i++;
        }

        QListIterator<AutoProfileInfo*> iter(deviceAutoProfiles.value(guid));
        while (iter.hasNext())
        {
            AutoProfileInfo *info = iter.next();
            if (!defaultForGUID || info != defaultForGUID)
            {
                ui->autoProfileTableWidget->insertRow(i);

                QTableWidgetItem *item = new QTableWidgetItem();
                item->setCheckState(info->isActive() ? Qt::Checked : Qt::Unchecked);
                ui->autoProfileTableWidget->setItem(i, 0, item);

                QString deviceName = info->getDeviceName();
                QString guidDisplay = info->getGUID();
                if (!deviceName.isEmpty())
                {
                    guidDisplay = QString("%1 ").arg(info->getDeviceName());
                    guidDisplay.append(QString("(%1)").arg(info->getGUID()));
                }
                item = new QTableWidgetItem(guidDisplay);
                item->setFlags(item->flags() & ~Qt::ItemIsEditable);
                item->setData(Qt::UserRole, info->getGUID());
                item->setToolTip(info->getGUID());
                ui->autoProfileTableWidget->setItem(i, 1, item);

                QFileInfo profilePath(info->getProfileLocation());
                item = new QTableWidgetItem(profilePath.fileName());
                item->setFlags(item->flags() & ~Qt::ItemIsEditable);
                item->setData(Qt::UserRole, info->getProfileLocation());
                item->setToolTip(info->getProfileLocation());
                ui->autoProfileTableWidget->setItem(i, 2, item);

                QFileInfo exeInfo(info->getExe());
                item = new QTableWidgetItem(exeInfo.fileName());
                item->setFlags(item->flags() & ~Qt::ItemIsEditable);
                item->setData(Qt::UserRole, info->getExe());
                item->setToolTip(info->getExe());
                ui->autoProfileTableWidget->setItem(i, 3, item);

                item = new QTableWidgetItem("Instance");
                item->setData(Qt::UserRole, QVariant::fromValue<AutoProfileInfo*>(info));
                ui->autoProfileTableWidget->setItem(i, 5, item);

                i++;
            }
        }
    }
}

void MainSettingsDialog::clearAutoProfileData()
{

}

void MainSettingsDialog::fillGUIDComboBox()
{
    ui->devicesComboBox->clear();
    ui->devicesComboBox->addItem(tr("All"), QVariant("all"));
    QList<QString> guids = deviceAutoProfiles.keys();
    QListIterator<QString> iter(guids);
    while (iter.hasNext())
    {
        QString guid = iter.next();
        QList<AutoProfileInfo*> temp = deviceAutoProfiles.value(guid);
        if (temp.count() > 0)
        {
            QString deviceName = temp.first()->getDeviceName();
            if (!deviceName.isEmpty())
            {
                ui->devicesComboBox->addItem(deviceName, QVariant(guid));
            }
            else
            {
                ui->devicesComboBox->addItem(guid, QVariant(guid));
            }
        }
        else
        {
            ui->devicesComboBox->addItem(guid, QVariant(guid));
        }
    }
}

void MainSettingsDialog::changeDeviceForProfileTable(int index)
{
    if (index == 0)
    {
        fillAllAutoProfilesTable();
    }
    else
    {
        QString guid = ui->devicesComboBox->itemData(index).toString();
        fillAutoProfilesTable(guid);
    }
}

void MainSettingsDialog::saveAutoProfileSettings()
{

    settings->beginGroup("DefaultAutoProfiles");
    QStringList defaultkeys = settings->allKeys();
    settings->endGroup();

    QStringListIterator iterDefaults(defaultkeys);
    while (iterDefaults.hasNext())
    {
        QString tempkey = iterDefaults.next();
        QString guid = QString(tempkey).replace("GUID", "");
        QString testkey = QString("DefaultAutoProfile-%1").arg(guid);
        settings->beginGroup(testkey);
        settings->remove("");
        settings->endGroup();
    }

    settings->beginGroup("DefaultAutoProfiles");
    settings->remove("");
    settings->endGroup();

    settings->beginGroup("DefaultAutoProfileAll");
    settings->remove("");
    settings->endGroup();

    settings->beginGroup("AutoProfiles");
    settings->remove("");
    settings->endGroup();

    if (allDefaultProfile)
    {
        QString profile = allDefaultProfile->getProfileLocation();
        QString defaultActive = allDefaultProfile->isActive() ? "1" : "0";
        if (!profile.isEmpty())
        {
            settings->setValue(QString("DefaultAutoProfileAll/Profile"), profile);
            settings->setValue(QString("DefaultAutoProfileAll/Active"), defaultActive);
        }
    }

    QMapIterator<QString, AutoProfileInfo*> iter(defaultAutoProfiles);
    while (iter.hasNext())
    {
        iter.next();
        QString guid = iter.key();
        AutoProfileInfo *info = iter.value();
        QString profileActive = info->isActive() ? "1" : "0";
        QString deviceName = info->getDeviceName();
        settings->setValue(QString("DefaultAutoProfiles/GUID%1").arg(guid), guid);
        settings->setValue(QString("DefaultAutoProfile-%1/Profile").arg(guid), info->getProfileLocation());
        settings->setValue(QString("DefaultAutoProfile-%1/Active").arg(guid), profileActive);
        settings->setValue(QString("DefaultAutoProfile-%1/DeviceName").arg(guid), deviceName);
    }

    settings->beginGroup("AutoProfiles");
    QString autoActive = ui->activeCheckBox->isChecked() ? "1" : "0";
    settings->setValue("AutoProfilesActive", autoActive);

    QListIterator<AutoProfileInfo*> iterProfiles(profileList);
    int i = 1;
    while (iterProfiles.hasNext())
    {
        AutoProfileInfo *info = iterProfiles.next();
        QString defaultActive = info->isActive() ? "1" : "0";
        settings->setValue(QString("AutoProfile%1Exe").arg(i), info->getExe());
        settings->setValue(QString("AutoProfile%1GUID").arg(i), info->getGUID());
        settings->setValue(QString("AutoProfile%1Profile").arg(i), info->getProfileLocation());
        settings->setValue(QString("AutoProfile%1Active").arg(i), defaultActive);
        settings->setValue(QString("AutoProfile%1DeviceName").arg(i), info->getDeviceName());
    }
    settings->endGroup();
}

void MainSettingsDialog::fillAllAutoProfilesTable()
{
    for (int i = ui->autoProfileTableWidget->rowCount()-1; i >= 0; i--)
    {
        ui->autoProfileTableWidget->removeRow(i);
    }

    //QStringList tableHeader;
    //tableHeader << tr("Active") << tr("GUID") << tr("Profile") << tr("Application") << tr("Default?")
    //            << tr("Instance");
    //ui->autoProfileTableWidget->setHorizontalHeaderLabels(tableHeader);

    ui->autoProfileTableWidget->horizontalHeader()->setVisible(true);

#if (QT_VERSION >= QT_VERSION_CHECK(5, 0, 0))
    ui->autoProfileTableWidget->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
#else
    ui->autoProfileTableWidget->horizontalHeader()->setResizeMode(QHeaderView::Stretch);
#endif

    ui->autoProfileTableWidget->hideColumn(5);

    int i = 0;

    AutoProfileInfo *info = allDefaultProfile;

    ui->autoProfileTableWidget->insertRow(i);
    QTableWidgetItem *item = new QTableWidgetItem();
    item->setCheckState(info->isActive() ? Qt::Checked : Qt::Unchecked);
    ui->autoProfileTableWidget->setItem(i, 0, item);

    QString deviceName = info->getDeviceName();
    QString guidDisplay = info->getGUID();
    if (!deviceName.isEmpty())
    {
        guidDisplay = QString("%1 ").arg(info->getDeviceName());
        guidDisplay.append(QString("(%1)").arg(info->getGUID()));
    }
    item = new QTableWidgetItem(guidDisplay);
    item->setFlags(item->flags() & ~Qt::ItemIsEditable);
    item->setData(Qt::UserRole, info->getGUID());
    item->setToolTip(info->getGUID());
    ui->autoProfileTableWidget->setItem(i, 1, item);

    QFileInfo profilePath(info->getProfileLocation());
    item = new QTableWidgetItem(profilePath.fileName());
    item->setFlags(item->flags() & ~Qt::ItemIsEditable);
    item->setData(Qt::UserRole, info->getProfileLocation());
    item->setToolTip(info->getProfileLocation());
    ui->autoProfileTableWidget->setItem(i, 2, item);

    QFileInfo exeInfo(info->getExe());
    item = new QTableWidgetItem(exeInfo.fileName());
    item->setFlags(item->flags() & ~Qt::ItemIsEditable);
    item->setData(Qt::UserRole, info->getExe());
    item->setToolTip(info->getExe());
    ui->autoProfileTableWidget->setItem(i, 3, item);

    item = new QTableWidgetItem("Default");
    item->setData(Qt::UserRole, "default");
    ui->autoProfileTableWidget->setItem(i, 4, item);

    item = new QTableWidgetItem("Instance");
    item->setData(Qt::UserRole, QVariant::fromValue<AutoProfileInfo*>(info));
    ui->autoProfileTableWidget->setItem(i, 5, item);

    i++;

    QListIterator<AutoProfileInfo*> iterDefaults(defaultList);
    while (iterDefaults.hasNext())
    {
        AutoProfileInfo *info = iterDefaults.next();
        ui->autoProfileTableWidget->insertRow(i);

        QTableWidgetItem *item = new QTableWidgetItem();
        item->setCheckState(info->isActive() ? Qt::Checked : Qt::Unchecked);
        ui->autoProfileTableWidget->setItem(i, 0, item);

        QString deviceName = info->getDeviceName();
        QString guidDisplay = info->getGUID();
        if (!deviceName.isEmpty())
        {
            guidDisplay = QString("%1 ").arg(info->getDeviceName());
            guidDisplay.append(QString("(%1)").arg(info->getGUID()));
        }

        item = new QTableWidgetItem(guidDisplay);
        item->setFlags(item->flags() & ~Qt::ItemIsEditable);
        item->setData(Qt::UserRole, info->getGUID());
        item->setToolTip(info->getGUID());
        ui->autoProfileTableWidget->setItem(i, 1, item);

        QFileInfo profilePath(info->getProfileLocation());
        item = new QTableWidgetItem(profilePath.fileName());
        item->setFlags(item->flags() & ~Qt::ItemIsEditable);
        item->setData(Qt::UserRole, info->getProfileLocation());
        item->setToolTip(info->getProfileLocation());
        ui->autoProfileTableWidget->setItem(i, 2, item);

        QFileInfo exeInfo(info->getExe());
        item = new QTableWidgetItem(exeInfo.fileName());
        item->setFlags(item->flags() & ~Qt::ItemIsEditable);
        item->setData(Qt::UserRole, info->getExe());
        item->setToolTip(info->getExe());
        ui->autoProfileTableWidget->setItem(i, 3, item);

        item = new QTableWidgetItem("Default");
        item->setData(Qt::UserRole, "default");
        ui->autoProfileTableWidget->setItem(i, 4, item);

        item = new QTableWidgetItem("Instance");
        item->setData(Qt::UserRole, QVariant::fromValue<AutoProfileInfo*>(info));
        ui->autoProfileTableWidget->setItem(i, 5, item);

        i++;
    }

    QListIterator<AutoProfileInfo*> iter(profileList);
    while (iter.hasNext())
    {
        AutoProfileInfo *info = iter.next();
        ui->autoProfileTableWidget->insertRow(i);

        QTableWidgetItem *item = new QTableWidgetItem();
        item->setCheckState(info->isActive() ? Qt::Checked : Qt::Unchecked);
        ui->autoProfileTableWidget->setItem(i, 0, item);

        QString deviceName = info->getDeviceName();
        QString guidDisplay = info->getGUID();
        if (!deviceName.isEmpty())
        {
            guidDisplay = QString("%1 ").arg(info->getDeviceName());
            guidDisplay.append(QString("(%1)").arg(info->getGUID()));
        }
        item = new QTableWidgetItem(guidDisplay);
        item->setFlags(item->flags() & ~Qt::ItemIsEditable);
        item->setData(Qt::UserRole, info->getGUID());
        item->setToolTip(info->getGUID());
        ui->autoProfileTableWidget->setItem(i, 1, item);

        QFileInfo profilePath(info->getProfileLocation());
        item = new QTableWidgetItem(profilePath.fileName());
        item->setFlags(item->flags() & ~Qt::ItemIsEditable);
        item->setData(Qt::UserRole, info->getProfileLocation());
        item->setToolTip(info->getProfileLocation());
        ui->autoProfileTableWidget->setItem(i, 2, item);

        QFileInfo exeInfo(info->getExe());
        item = new QTableWidgetItem(exeInfo.fileName());
        item->setFlags(item->flags() & ~Qt::ItemIsEditable);
        item->setData(Qt::UserRole, info->getExe());
        item->setToolTip(info->getExe());
        ui->autoProfileTableWidget->setItem(i, 3, item);

        item = new QTableWidgetItem();
        item->setData(Qt::UserRole, "");
        ui->autoProfileTableWidget->setItem(i, 4, item);

        item = new QTableWidgetItem("Instance");
        item->setData(Qt::UserRole, QVariant::fromValue<AutoProfileInfo*>(info));
        ui->autoProfileTableWidget->setItem(i, 5, item);

        i++;
    }
}

void MainSettingsDialog::processAutoProfileActiveClick(QTableWidgetItem *item)
{
    int selectedRow = ui->autoProfileTableWidget->currentRow();
    if (selectedRow >= 0 && item->column() == 0)
    {
        qDebug() << item->row();
        QTableWidgetItem *infoitem = ui->autoProfileTableWidget->item(item->row(), 5);
        AutoProfileInfo *info = infoitem->data(Qt::UserRole).value<AutoProfileInfo*>();
        Qt::CheckState active = item->checkState();
        if (active == Qt::Unchecked)
        {
            info->setActive(false);
        }
        else if (active == Qt::Checked)
        {
            info->setActive(true);
        }
    }
}

void MainSettingsDialog::openAddAutoProfileDialog()
{
    int selectedRow = ui->autoProfileTableWidget->currentRow();
    if (selectedRow >= 0)
    {
        //if (ui->devicesComboBox->currentIndex() != 0 || selectedRow != 0)
        //{
            QList<QString> reservedGUIDs = defaultAutoProfiles.keys();
            AutoProfileInfo *info = new AutoProfileInfo(this);
            AddEditAutoProfileDialog *dialog = new AddEditAutoProfileDialog(info, settings, connectedDevices, reservedGUIDs, false, this);
            connect(dialog, SIGNAL(accepted()), this, SLOT(addNewAutoProfile()));
            connect(dialog, SIGNAL(rejected()), info, SLOT(deleteLater()));
            dialog->show();
        //}
    }
}

void MainSettingsDialog::openEditAutoProfileDialog()
{
    int selectedRow = ui->autoProfileTableWidget->currentRow();
    if (selectedRow >= 0)
    {
        QTableWidgetItem *item = ui->autoProfileTableWidget->item(selectedRow, 5);
        //QTableWidgetItem *itemDefault = ui->autoProfileTableWidget->item(selectedRow, 4);
        AutoProfileInfo *info = item->data(Qt::UserRole).value<AutoProfileInfo*>();
        if (info != allDefaultProfile)
        {
            QList<QString> reservedGUIDs = defaultAutoProfiles.keys();
            if (info->getGUID() != "all")
            {
                AutoProfileInfo *temp = defaultAutoProfiles.value(info->getGUID());
                if (info == temp)
                {
                    reservedGUIDs.removeAll(info->getGUID());
                }
            }
            AddEditAutoProfileDialog *dialog = new AddEditAutoProfileDialog(info, settings, connectedDevices, reservedGUIDs, true, this);
            connect(dialog, SIGNAL(accepted()), this, SLOT(transferEditsToCurrentTableRow()));
            dialog->show();
        }
        else
        {
            EditAllDefaultAutoProfileDialog *dialog = new EditAllDefaultAutoProfileDialog(info, settings, this);
            dialog->show();
            connect(dialog, SIGNAL(accepted()), this, SLOT(transferEditsToCurrentTableRow()));
        }
    }
}

void MainSettingsDialog::openDeleteAutoProfileConfirmDialog()
{
    QMessageBox msgBox;
    msgBox.setText(tr("Are you sure you want to delete the profile?"));
    msgBox.setStandardButtons(QMessageBox::Discard | QMessageBox::Cancel);
    msgBox.setDefaultButton(QMessageBox::Cancel);
    int ret = msgBox.exec();
    if (ret == QMessageBox::Discard)
    {
        int selectedRow = ui->autoProfileTableWidget->currentRow();
        if (selectedRow >= 0)
        {
            QTableWidgetItem *item = ui->autoProfileTableWidget->item(selectedRow, 5);
            //QTableWidgetItem *itemDefault = ui->autoProfileTableWidget->item(selectedRow, 4);
            AutoProfileInfo *info = item->data(Qt::UserRole).value<AutoProfileInfo*>();
            if (info->isCurrentDefault())
            {
                if (info->getGUID() == "all")
                {
                    delete allDefaultProfile;
                    allDefaultProfile = 0;
                }
                else if (defaultAutoProfiles.contains(info->getGUID()))
                {
                    defaultAutoProfiles.remove(info->getGUID());
                    defaultList.removeAll(info);
                    delete info;
                    info = 0;
                }
            }
            else
            {
                if (deviceAutoProfiles.contains(info->getGUID()))
                {
                    QList<AutoProfileInfo*> temp = deviceAutoProfiles.value(info->getGUID());
                    temp.removeAll(info);
                    deviceAutoProfiles.insert(info->getGUID(), temp);
                }

                if (exeAutoProfiles.contains(info->getExe()))
                {
                    QList<AutoProfileInfo*> temp = exeAutoProfiles.value(info->getExe());
                    temp.removeAll(info);
                    exeAutoProfiles.insert(info->getExe(), temp);
                }

                profileList.removeAll(info);

                delete info;
                info = 0;
            }
        }
        ui->autoProfileTableWidget->removeRow(selectedRow);
    }
}

void MainSettingsDialog::changeAutoProfileButtonsState()
{
    int selectedRow = ui->autoProfileTableWidget->currentRow();
    if (selectedRow >= 0)
    {
        QTableWidgetItem *item = ui->autoProfileTableWidget->item(selectedRow, 5);
        //QTableWidgetItem *itemDefault = ui->autoProfileTableWidget->item(selectedRow, 4);
        AutoProfileInfo *info = item->data(Qt::UserRole).value<AutoProfileInfo*>();

        if (info == allDefaultProfile)
        {
            ui->autoProfileAddPushButton->setEnabled(true);
            ui->autoProfileEditPushButton->setEnabled(true);
            ui->autoProfileDeletePushButton->setEnabled(false);
        }
        else
        {
            ui->autoProfileAddPushButton->setEnabled(true);
            ui->autoProfileEditPushButton->setEnabled(true);
            ui->autoProfileDeletePushButton->setEnabled(true);
        }
    }
    else
    {
        ui->autoProfileAddPushButton->setEnabled(false);
        ui->autoProfileDeletePushButton->setEnabled(false);
        ui->autoProfileEditPushButton->setEnabled(false);
    }
}

void MainSettingsDialog::transferEditsToCurrentTableRow()
{
    AddEditAutoProfileDialog *dialog = static_cast<AddEditAutoProfileDialog*>(sender());
    AutoProfileInfo *info = dialog->getAutoProfile();

    // Delete pointers to object that might be misplaced
    // due to an association change.
    QString oldGUID = dialog->getOriginalGUID();
    QString originalExe = dialog->getOriginalExe();
    if (oldGUID != info->getGUID())
    {
        if (defaultAutoProfiles.value(oldGUID) == info)
        {
            defaultAutoProfiles.remove(oldGUID);
        }

        if (info->isCurrentDefault())
        {
            defaultAutoProfiles.insert(info->getGUID(), info);
        }
    }

    if (oldGUID != info->getGUID() && deviceAutoProfiles.contains(oldGUID))
    {
        QList<AutoProfileInfo*> temp = deviceAutoProfiles.value(oldGUID);
        temp.removeAll(info);
        if (temp.count() > 0)
        {
            deviceAutoProfiles.insert(oldGUID, temp);
        }
        else
        {
            deviceAutoProfiles.remove(oldGUID);
        }

        if (deviceAutoProfiles.contains(info->getGUID()))
        {
            QList<AutoProfileInfo*> temp2 = deviceAutoProfiles.value(oldGUID);
            if (!temp2.contains(info))
            {
                temp2.append(info);
                deviceAutoProfiles.insert(info->getGUID(), temp2);
            }
        }
        else if (info->getGUID().toLower() != "all")
        {
            QList<AutoProfileInfo*> temp2;
            temp2.append(info);
            deviceAutoProfiles.insert(info->getGUID(), temp2);
        }
    }
    else if (oldGUID != info->getGUID() && info->getGUID().toLower() != "all")
    {
        QList<AutoProfileInfo*> temp;
        temp.append(info);
        deviceAutoProfiles.insert(info->getGUID(), temp);
    }

    if (!info->isCurrentDefault())
    {
        defaultList.removeAll(info);

        if (!profileList.contains(info))
        {
            profileList.append(info);
        }
    }
    else
    {
        profileList.removeAll(info);

        if (!defaultList.contains(info))
        {
            defaultList.append(info);
        }
    }


    if (originalExe != info->getExe() &&
        exeAutoProfiles.contains(originalExe))
    {
        QList<AutoProfileInfo*> temp = exeAutoProfiles.value(originalExe);
        temp.removeAll(info);
        exeAutoProfiles.insert(originalExe, temp);

        if (exeAutoProfiles.contains(info->getExe()))
        {
            QList<AutoProfileInfo*> temp2 = exeAutoProfiles.value(info->getExe());
            if (!temp2.contains(info))
            {
                temp2.append(info);
                exeAutoProfiles.insert(info->getExe(), temp2);
            }
        }
        else
        {
            QList<AutoProfileInfo*> temp2;
            temp2.append(info);
            exeAutoProfiles.insert(info->getExe(), temp2);
        }

        if (deviceAutoProfiles.contains(info->getGUID()))
        {
            QList<AutoProfileInfo*> temp2 = deviceAutoProfiles.value(info->getGUID());
            if (!temp2.contains(info))
            {
                temp2.append(info);
                deviceAutoProfiles.insert(info->getGUID(), temp2);
            }
        }
        else
        {
            QList<AutoProfileInfo*> temp2;
            temp2.append(info);
            deviceAutoProfiles.insert(info->getGUID(), temp2);
        }
    }

    fillGUIDComboBox();
    changeDeviceForProfileTable(ui->devicesComboBox->currentIndex());
}

void MainSettingsDialog::addNewAutoProfile()
{
    AddEditAutoProfileDialog *dialog = static_cast<AddEditAutoProfileDialog*>(sender());
    AutoProfileInfo *info = dialog->getAutoProfile();

    bool found = false;
    if (info->isCurrentDefault())
    {
        if (defaultAutoProfiles.contains(info->getGUID()))
        {
            found = true;
        }
    }
    else
    {
        QList<AutoProfileInfo*> templist;
        if (exeAutoProfiles.contains(info->getExe()))
        {
            templist = exeAutoProfiles.value(info->getExe());
        }

        QListIterator<AutoProfileInfo*> iterProfiles(templist);
        while (iterProfiles.hasNext())
        {
            AutoProfileInfo *oldinfo = iterProfiles.next();
            if (info->getExe() == oldinfo->getExe() &&
                info->getGUID() == oldinfo->getGUID())
            {
                found = true;
                iterProfiles.toBack();
            }
        }
    }

    if (!found)
    {
        if (info->isCurrentDefault())
        {
            if (!info->getGUID().isEmpty() && !info->getProfileLocation().isEmpty())
            {
                defaultAutoProfiles.insert(info->getGUID(), info);
                defaultList.append(info);
            }
        }
        else
        {
            if (!info->getGUID().isEmpty() &&
                !info->getProfileLocation().isEmpty() &&
                !info->getExe().isEmpty())
            {
                QList<AutoProfileInfo*> templist;
                templist.append(info);
                exeAutoProfiles.insert(info->getExe(), templist);
                profileList.append(info);

                QList<AutoProfileInfo*> tempDevProfileList;
                if (deviceAutoProfiles.contains(info->getGUID()))
                {
                    tempDevProfileList = deviceAutoProfiles.value(info->getGUID());
                }

                tempDevProfileList.append(info);
                deviceAutoProfiles.insert(info->getGUID(), tempDevProfileList);
            }
        }

        fillGUIDComboBox();
        changeDeviceForProfileTable(ui->devicesComboBox->currentIndex());
    }
}
