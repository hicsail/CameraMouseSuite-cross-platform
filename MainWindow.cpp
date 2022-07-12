/*                         Camera Mouse Suite
 *  Copyright (C) 2015, Andrew Kurauchi
 *
 *  This program is free software: you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation, either version 3 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include <QCameraInfo>
#include <QMessageBox>
#include <QTimer>
#include <QKeyEvent>

#include "MainWindow.h"
#include "ui_mainWindow.h"
#include "VideoManagerSurface.h"
#include "CameraMouseController.h"
#include "TemplateTrackingModule.h"
#include "MouseControlModule.h"
#include "ImageProcessing.h"


Q_DECLARE_METATYPE(QCameraInfo)

namespace CMS {

MainWindow::MainWindow(QWidget *parent) :
    QMainWindow(parent),
    ui(new Ui::MainWindow),
    camera(0),
    settings(this)
{
    ui->setupUi(this);
    setWindowTitle(tr("CameraMouseSuite"));
    setupCameraWidgets();
    setupSettingsWidgets();

}

MainWindow::~MainWindow()
{
    delete ui;
}

void MainWindow::setupCameraWidgets()
{
    // Create video manager
    ITrackingModule *trackingModule = new TemplateTrackingModule(0.08); // TODO magic constants are not nice :(
    MouseControlModule *controlModule = new MouseControlModule(settings);
    CameraMouseController *controller = new CameraMouseController(settings, trackingModule, controlModule);
    videoManagerSurface = new VideoManagerSurface(settings, controller, ui->frameLabel, this);

    // Create device selection menu
    QActionGroup *cameraGroup = new QActionGroup(this);
    cameraGroup->setExclusive(true);
    foreach (const QCameraInfo &cameraInfo, QCameraInfo::availableCameras()) {
        QAction *cameraAction = new QAction(cameraInfo.description(), cameraGroup);
        cameraAction->setCheckable(true);
        cameraAction->setData(QVariant::fromValue(cameraInfo));
        if (cameraInfo == QCameraInfo::defaultCamera())
            cameraAction->setChecked(true);

        ui->menuDevices->addAction(cameraAction);
    }

    connect(cameraGroup, SIGNAL(triggered(QAction*)), SLOT(updateSelectedCamera(QAction*)));

    setCamera(QCameraInfo::defaultCamera());
    if (!controller->isAutoDetectWorking()) ui->autoDetectNoseCheckBox->setVisible(false);
}

void MainWindow::setupSettingsWidgets()
{
    // Clicking
    connect(ui->enableClickingCheckBox, SIGNAL(toggled(bool)), ui->dwellSlider, SLOT(setEnabled(bool)));
    connect(ui->enableClickingCheckBox, SIGNAL(toggled(bool)), ui->dwellSpinBox, SLOT(setEnabled(bool)));
    connect(ui->enableClickingCheckBox, SIGNAL(toggled(bool)), &settings, SLOT(setEnableClicking(bool)));
    bool clickingEnabled = settings.isClickingEnabled();
    ui->enableClickingCheckBox->setChecked(clickingEnabled);
    ui->dwellSlider->setEnabled(clickingEnabled);
    ui->dwellSpinBox->setEnabled(clickingEnabled);

    // Dwell Time
    connect(ui->dwellSpinBox, SIGNAL(valueChanged(double)), &settings, SLOT(setDwellTime(double)));
    connect(ui->dwellSpinBox, SIGNAL(valueChanged(double)), this, SLOT(updateDwellSlider(double)));
    connect(ui->dwellSlider, SIGNAL(valueChanged(int)), this, SLOT(updateDwellSpinBox(int)));
    ui->dwellSpinBox->setValue(1.0);

    // Mouse Movement
    connect(ui->reverseHorizontalCheckBox, SIGNAL(toggled(bool)), &settings, SLOT(setReverseHorizontal(bool)));
    ui->reverseHorizontalCheckBox->setChecked(settings.getReverseHorizontal());

    // Gain
    connect(ui->horizontalGainSlider, SIGNAL(valueChanged(int)), &settings, SLOT(setHorizontalGain(int)));
    connect(ui->horizontalGainSlider, SIGNAL(valueChanged(int)), this, SLOT(horizontalGainChanged(int)));
    connect(ui->verticalGainSlider, SIGNAL(valueChanged(int)), &settings, SLOT(setVerticalGain(int)));
    connect(ui->verticalGainSlider, SIGNAL(valueChanged(int)), this, SLOT(verticalGainChanged(int)));
    connect(ui->lockGainButton, SIGNAL(toggled(bool)), this, SLOT(lockGainClicked(bool)));
    ui->lockGainButton->setChecked(true);
    settings.setHorizontalGain(ui->horizontalGainSlider->value());
    settings.setVerticalGain(ui->verticalGainSlider->value());

    // Smoothing
    connect(ui->smoothingCheckBox, SIGNAL(toggled(bool)), ui->smoothingSlider, SLOT(setEnabled(bool)));
    connect(ui->smoothingCheckBox, SIGNAL(toggled(bool)), &settings, SLOT(setEnableSmoothing(bool)));
    connect(ui->smoothingSlider, SIGNAL(valueChanged(int)), &settings, SLOT(setDampingPercent(int)));
    settings.setDampingPercent(ui->smoothingSlider->value());
    ui->smoothingCheckBox->setChecked(true);

    // Auto Detect Nose
    connect(ui->autoDetectNoseCheckBox, SIGNAL(toggled(bool)), &settings, SLOT(setAutoDetectNose(bool)));
    ui->autoDetectNoseCheckBox->setChecked(settings.isAutoDetectNoseEnabled());

    // Reset on F5
    connect(ui->resetCheckF5, SIGNAL(toggled(bool)), &settings, SLOT(setResetOnF5(bool)));

    // Reset button under the camera
    connect(ui->placeResetButton, SIGNAL(toggled(bool)), this, SLOT(toggleResetButton(bool)));
    connect(ui->placeResetButton, SIGNAL(toggled(bool)), &settings, SLOT(setShowResetButton(bool)));
    ui->resetButton->setVisible(false);

    // Auto reset interval
    connect(ui->resetCheckTime, SIGNAL(toggled(bool)), this, SLOT(enableResetInterval(bool)));
    connect(ui->resetCheckTime, SIGNAL(toggled(bool)), &settings, SLOT(setAutoResetTimer(bool)));

    // Auto reset on trackpoint loss
    connect(ui->trackpointLossCheck, SIGNAL(toggled(bool)), &settings, SLOT(setTrackPointLossReset(bool state)));


}

void MainWindow::updateSelectedCamera(QAction *action)
{
    setCamera(qvariant_cast<QCameraInfo>(action->data()));
}

void MainWindow::displayCameraError()
{
    QMessageBox::warning(this, tr("Camera error"), camera->errorString());
}

void MainWindow::setCamera(const QCameraInfo &cameraInfo)
{
    if (camera)
        delete camera;

    camera = new QCamera(cameraInfo);

    connect(camera, SIGNAL(error(QCamera::Error)), this, SLOT(displayCameraError()));
    camera->setViewfinder(videoManagerSurface);

    camera->start();

    camera->setCaptureMode(QCamera::CaptureViewfinder);
    camera->searchAndLock();
}

void MainWindow::updateDwellSpinBox(int dwellMillis)
{
    double dwellTime = dwellMillis / 1000.0;
    ui->dwellSpinBox->setValue(dwellTime);
}

void MainWindow::updateDwellSlider(double dwellSecs)
{
    int dwellMillis = (int) (dwellSecs * 1000);
    ui->dwellSlider->setValue(dwellMillis);
}

void MainWindow::horizontalGainChanged(int horizontalGain)
{
    if (ui->lockGainButton->isChecked())
        ui->verticalGainSlider->setValue(horizontalGain);
}

void MainWindow::verticalGainChanged(int verticalGain)
{
    if (ui->lockGainButton->isChecked())
        ui->horizontalGainSlider->setValue(verticalGain);
}

void MainWindow::lockGainClicked(bool lock)
{
    if (lock)
        ui->verticalGainSlider->setValue(ui->horizontalGainSlider->value());
}

void MainWindow::toggleResetButton(bool state) {
    ui->resetButton->setVisible(state);
}

void MainWindow::on_resetButton_clicked()
{
   videoManagerSurface->keyPress();
}


void MainWindow::updateResetButton()
{
    /* Below code is just for testing of the signal-slot connection and the timer
    QString resetTime = ui->resetTime->text();
    QString resetUnit = ui->resetTimeUnit->currentText();
    ui->resetButton->setText("5-4-3-2-1 choices: " + resetTime + " " + resetUnit); */
}

void MainWindow::keyPressEvent(QKeyEvent *event) {
    if (event->key() == Qt::Key_F5) {
        if  (ui->resetCheckF5->isChecked()) {
            qDebug() << "F5 pressed on MainWindow";
            videoManagerSurface->keyPress();
            // Draw a rectangle with a text for the current second - every second for 4 seconds, then call processClick on the 5th second while supplying the center of the image
        }

    }
}

void MainWindow::resetCountdown() {
        qDebug() << (time->second());

        if (time->second() > 0) {
            ui->resetButton->setText(QString::number(time->second()));
            videoManagerSurface->drawCountdownRectangle( std::to_string( time->second() ) );
            *time = time->addSecs(-1);
        } else {
            timer->stop();
        }
}

void MainWindow::enableResetInterval(bool state) {
        int time = (ui->resetTime->text()).toInt();
        QString unit = (ui->resetTimeUnit->currentText());

        int resetInterval;

        if (unit=="seconds") {
           resetInterval = time * 1000;

        } else {
           resetInterval = time * 60 * 1000;
        }
        qDebug() << "Reset interval is " << resetInterval << " ms";
        videoManagerSurface->triggerResetInterval(resetInterval);
}


void MainWindow::on_stopTrackingButton_clicked()
{
  videoManagerSurface->stopTracking();
}

}


// namespace CMS
