#-------------------------------------------------
#                         Camera Mouse Suite
#  Copyright (C) 2015, Andrew Kurauchi
#
#  This program is free software: you can redistribute it and/or modify
#  it under the terms of the GNU General Public License as published by
#  the Free Software Foundation, either version 3 of the License, or
#  (at your option) any later version.
#
#  This program is distributed in the hope that it will be useful,
#  but WITHOUT ANY WARRANTY; without even the implied warranty of
#  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
#  GNU General Public License for more details.
#
#  You should have received a copy of the GNU General Public License
#  along with this program.  If not, see <http://www.gnu.org/licenses/>.
#-------------------------------------------------

QT       += core gui multimedia multimediawidgets

greaterThan(QT_MAJOR_VERSION, 4): QT += widgets

TARGET = CameraMouseSuite
TEMPLATE = app

unix {
    QT_CONFIG -= no-pkg-config
    CONFIG += c++14 link_pkgconfig
    LIBS += -L/usr/local/lib

    mac {
      PKG_CONFIG = /usr/local/bin/pkg-config
    }

    linux {
        PKGCONFIG += x11
    }

    PKGCONFIG += opencv4
}

mac {
    LIBS += -framework ApplicationServices \
            -framework AppKit \
            -framework Foundation

    OBJECTIVE_SOURCES += MacKeyboard.mm

    QMAKE_INFO_PLIST = Info.plist

}

win32 {
    INCLUDEPATH += $$(OPENCV_INCLUDE) \
                   $$(OPENCV_INCLUDE)/opencv2
    CONFIG(debug, debug|release) {
        LIBS += $$(OPENCV_DIR)/lib/*d.lib \
            -luser32
        message(Debug configuration!)
    }
    CONFIG(release, debug|release) {
        LIBS += $$(OPENCV_DIR)/lib/*.lib \
               -luser32
        message(Release configuration!)
    }
}

SOURCES += *.cpp

HEADERS  += *.h

FORMS    += mainWindow.ui

OTHER_FILES += \
    README.md \
    cascades/*.xml

RESOURCES += \
    icons.qrc

# Copy haar cascade files
SRC = $${PWD}/cascades
DEST = $${OUT_PWD}/cascades

win32 {
CONFIG(debug, debug|release) DEST = $${OUT_PWD}/debug/cascades
CONFIG(release, debug|release) DEST = $${OUT_PWD}/release/cascades

SRC ~= s,/,\\,g
DEST ~= s,/,\\,g
}

mac {
DEST = $${OUT_PWD}/CameraMouseSuite.app/Contents/MacOS
}

copydata.commands = $(COPY_DIR) $$SRC $$DEST
first.depends = $(first) copydata
export(first.depends)
export(copydata.commands)
QMAKE_EXTRA_TARGETS += first copydata
