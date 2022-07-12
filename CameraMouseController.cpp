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

#include "qdebug.h"
#include <QObject>
#ifdef Q_OS_LINUX
#include <opencv2/imgproc.hpp>
#endif

#include "CameraMouseController.h"
#include "ImageProcessing.h"
#include <QKeyEvent>

namespace CMS {

CameraMouseController::CameraMouseController(Settings &settings, ITrackingModule *trackingModule, MouseControlModule *controlModule) :
    settings(settings), trackingModule(trackingModule), controlModule(controlModule)
{
    featureCheckTimer.start();
}

CameraMouseController::~CameraMouseController()
{
    delete trackingModule;
    delete controlModule;
}

void CameraMouseController::processFrame(cv::Mat &frame)
{
    prevFrame = frame;

    if (trackingModule->isInitialized())
    {
        Point featurePosition = trackingModule->track(frame);
        if (!featurePosition.empty())
        {
            if (settings.isAutoDetectNoseEnabled() && featureCheckTimer.elapsed() > 1000)
            {
                Point autoFeaturePosition = initializationModule.initializeFeature(frame);
                if (!autoFeaturePosition.empty())
                {
                    double distThreshSq = settings.getResetFeatureDistThreshSq();
                    Point disp = autoFeaturePosition - featurePosition;
                    if (disp * disp > distThreshSq)
                    {
                        trackingModule->setTrackPoint(frame, autoFeaturePosition);
                        controlModule->setScreenReference(controlModule->getPrevPos());
                        controlModule->restart();
                        featurePosition = autoFeaturePosition;
                    }
                    featureCheckTimer.restart();
                }
            } else if (featurePosition.X() >= 1828 || featurePosition.X() <= 90) { // Track point is too far to the left or the right. Chosen X coordinates are arbitrary
                startAutoResetInterval();
            }

            trackingModule->drawOnFrame(frame, featurePosition);

            controlModule->update(featurePosition);
        }
    }
    else if (settings.isAutoDetectNoseEnabled())
    {
        Point initialFeaturePosition = initializationModule.initializeFeature(frame);
        if (!initialFeaturePosition.empty())
        {
            trackingModule->setTrackPoint(frame, initialFeaturePosition);
            controlModule->setScreenReference(settings.getScreenResolution()/2);
            controlModule->restart();
        }
    } else if (settings.isResetOnF5Enabled() || settings.isShowResetButtonEnabled() || settings.isAutoResetTimerEnabled() || settings.isTrackPointLossResetEnabled()) {
            if (timer->isActive()) {
                drawCountdown();
                drawSecondsText();
            }

        }
}

void CameraMouseController::processClick(Point position)
{
    if (!prevFrame.empty())
    {
        trackingModule->setTrackPoint(prevFrame, position);
        controlModule->restart();
    }
}

bool CameraMouseController::isAutoDetectWorking()
{
    return initializationModule.allFilesLoaded();
}


void CameraMouseController::keyPress() {
    if  (settings.isResetOnF5Enabled() || settings.isShowResetButtonEnabled() || settings.isAutoResetTimerEnabled()) {

        connect(timer, SIGNAL(timeout()), this, SLOT(resetCountdown()));
        timer->start(1000);
        // Draw a rectangle with a text for the current second - every second for 4 seconds, then call processClick on the 5th second while supplying the center of the image
    } else {
        trackingModule->stopTracking();
    }
}

void CameraMouseController::resetCountdown() {

        if (time->second() > 1) {
            *time = time->addSecs(-1);
        } else {
            timer->stop();

            timer = new QTimer();
            time = new QTime(0,0,5,0);

            int width = (int) (prevFrame.size().width);
            int height = (int) (prevFrame.size().height);
            Point center = Point(width/2, height/2);
            processClick(center);

            if (settings.isAutoResetTimerEnabled() && !autoResetTimer->isActive()) {
                autoResetTimer->start(autoResetInterval);
            }
        }
}



void CameraMouseController::drawCountdown() {

    float ratio = 0.03;

    int width = (int) (prevFrame.size().width);
    int height = (int) (prevFrame.size().height);
    Point center = Point(width/2, height/2);

    cv::Rect rectangle(center.X() - ((width*ratio)/2), center.Y() - ((height*ratio)/2), (width*ratio), (height*ratio));
    ImageProcessing::drawWhiteRectangle(prevFrame, rectangle);
}

void CameraMouseController::drawSecondsText() {
    std::string sec = std::to_string(time->second());

    int width = (int) (prevFrame.size().width);
    int height = (int) (prevFrame.size().height);

    ImageProcessing::drawTimerSecond(prevFrame, sec, Point((width/2)-15, (height/2)-40));
}

void CameraMouseController::resetInterval(int interval) {
    autoResetInterval = interval;
    autoResetTimer = new QTimer();
    autoResetTime = new QTime(0,0,interval/1000,0);
    connect(autoResetTimer, SIGNAL(timeout()), this, SLOT(startAutoResetInterval()));

    autoResetTimer->start(interval);
}

void CameraMouseController::startAutoResetInterval() {
      trackingModule->stopTracking();
      connect(timer, SIGNAL(timeout()), this, SLOT(resetCountdown()));
      timer->start(1000);

      if (autoResetTimer->isActive()) {
          autoResetTimer->stop();
      }


}


} // namespace CMS

