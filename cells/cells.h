#ifndef _CELLS_VH5234262356_HG_
#define _CELLS_VH5234262356_HG_
#include "common.h"

#include <QString>
#include <QImage>
#include <QMap>

#include <cv.h>
#include <opencv2/imgproc/imgproc_c.h>
#include <opencv2/core/core.hpp>
#include <opencv2/highgui/highgui.hpp>
#include <opencv2/features2d/features2d.hpp>
#include <opencv2/nonfree/features2d.hpp>
#include <list>
#include <vector>

typedef std::vector<cv::Rect> TRectList;
typedef std::vector<cv::RotatedRect> TRotatedRectList;
typedef std::list<cv::Mat> TMatList;

#define STD_USE_SIMILAR_CELLS 0
#define STD_USE_ROI 0

class CELLS_EXPORT _T
{
	public:
		enum ChannelMode { Red, Green, Blue, RGB, Grey1Ch, Grey3Ch };
		static cv::Mat toMat( QImage img, ChannelMode in=Green, ChannelMode out=Grey1Ch );
};

class CELLS_EXPORT CellExtractor
{

	float _minCell;
	float _maxCell;
	int _minThreshold;
	int _maxThreshold;
	int _levels;
	TRotatedRectList _cellRegions;
	TRectList _cellRects;
	std::vector<std::vector<cv::Point> > _contours;
	cv::Mat _imgIn;
	cv::Mat _mask;
	cv::Mat _norm;	
	
public:
	CellExtractor( float minCell=60, float maxCell=150, int minThreshold=0, int maxThreshold=4, int levels=5 ):
		_minCell(minCell),
		_maxCell(maxCell),
		_minThreshold(minThreshold),
		_maxThreshold(maxThreshold),
		_levels(levels)
	{};

	void setMinCell( float v ){_minCell=v;}
	void setMaxCell( float v ){_maxCell=v;}
	void setMinTh( float v ){_minThreshold=v;}
	void setMaxTh( float v ){_maxThreshold=v;}
	void setLevels( float v ){_levels=v;}
	
	const TRotatedRectList & findCellRects( cv::Mat, QString dataHash = QString::null, int minCellNumber=30, int findSimilar=STD_USE_SIMILAR_CELLS, int useRoi=STD_USE_ROI );//TODO use enums for 2 last parameters
	const TRotatedRectList & findCellRects1( cv::Mat, QString dataHash = QString::null, int minCellNumber=30, int findSimilar=STD_USE_SIMILAR_CELLS, int useRoi=STD_USE_ROI );//TODO use enums for 2 last parameters
	void setCellsImage( cv::Mat m){ _imgIn = m; };//replace math when using operators
	const std::vector< std::vector<cv::Point> > & cellContours();
	const TRectList & cellRects() const { return  _cellRects;}

	cv::RotatedRect cellRegion( int );
	cv::Rect cellRect( int );
	int cells( ) const { return _cellRegions.size();}

	cv::Mat cellAt( cv::Point, int*index = 0 );
	cv::Mat cell( int idx );
	cv::Mat cellMask( int idx );
	cv::Mat cellsMask() const;
	cv::Mat cellsNormImage() const;
	int _roiX;
	int _roiY;
	cv::Mat normalizedCellAt( cv::Point, const cv::Size & outSize, int*index = 0 );
	cv::Mat normalizeCell( cv::Mat, const cv::RotatedRect &rr, const cv::Size & outSize  );
	
	static QString getCellPixelFeatures( int offset, cv::Mat cell, const cv::Size & outSize );
	static QMap<int, double> getCellPixelFeaturesMap( int offset, cv::Mat cell, const cv::Size & outSize );
};

#endif
