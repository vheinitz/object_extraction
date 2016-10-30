#V. Heinitz, 2016-10-29
#Project file for Cells - the cell-analysis tool library

QT       += core

DEFINES += BUILDING_CELLS_DLL
 
INCLUDEPATH += .


#output directory
DESTDIR = ../../output
TARGET = cells
TEMPLATE = lib 


SOURCES += \
    cells.cpp \

HEADERS  += \
    common.h \
    cells.h \



INCLUDEPATH += . \
		"../../Libraries/3rdparty/opencv/include/opencv" \
		"../../Libraries/3rdparty/opencv/include/" \

win32 {
	CONFIG(debug, debug|release) {
		LIBS += \
		../../Libraries/3rdparty/opencv/lib/opencv_contrib240d.lib \
		../../Libraries/3rdparty/opencv/lib/opencv_core240d.lib \
		../../Libraries/3rdparty/opencv/lib/opencv_features2d240d.lib \
		../../Libraries/3rdparty/opencv/lib/opencv_flann240d.lib \
		../../Libraries/3rdparty/opencv/lib/opencv_nonfree240d.lib \
		../../Libraries/3rdparty/opencv/lib/opencv_highgui240d.lib \
		../../Libraries/3rdparty/opencv/lib/opencv_imgproc240d.lib \
		../../Libraries/3rdparty/opencv/lib/opencv_legacy240d.lib \
		../../Libraries/3rdparty/opencv/lib/opencv_ml240d.lib \
		../../Libraries/3rdparty/opencv/lib/opencv_objdetect240d.lib 
	} else {
		LIBS += \
		../../Libraries/3rdparty/opencv/lib/opencv_contrib240.lib \
		../../Libraries/3rdparty/opencv/lib/opencv_core240.lib \
		../../Libraries/3rdparty/opencv/lib/opencv_features2d240.lib \
		../../Libraries/3rdparty/opencv/lib/opencv_flann240.lib \
		../../Libraries/3rdparty/opencv/lib/opencv_nonfree240.lib \
		../../Libraries/3rdparty/opencv/lib/opencv_highgui240.lib \
		../../Libraries/3rdparty/opencv/lib/opencv_imgproc240.lib \
		../../Libraries/3rdparty/opencv/lib/opencv_legacy240.lib \
		../../Libraries/3rdparty/opencv/lib/opencv_ml240.lib \
		../../Libraries/3rdparty/opencv/lib/opencv_objdetect240.lib 
	}
}





