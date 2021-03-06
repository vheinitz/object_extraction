/*V. Heinitz, 2012-04-05
Tools for cells analysis in microscope images
*/

#include "cells.h"
#include <QMutex>
#include <map>
#include <QtCore/qmath.h>

using std::map;
using namespace cv;

QMap<QString, TRotatedRectList> _RotatedRectListCache;
QMap<QString, TRectList> _RectListCache;
QMap<QString, std::vector<std::vector<cv::Point> > > _ContoursCache;

QMutex _cellMx;

Mat _T::toMat( QImage src, ChannelMode /*in*/, ChannelMode /*out*/ )
{
	//TODO iterate, avoid too much temp memory!
	//TODO evaluate in out settings
	cv::Mat tmp(src.height(),src.width(),CV_8UC4,(uchar*)src.bits(),src.bytesPerLine());
    cv::Mat result;
    cvtColor(tmp, result,CV_BGRA2RGB);
	Mat channels[3];
	split(result,channels); 
    return channels[1];	
}

cv::RotatedRect CellExtractor::cellRegion( int idx )
{
	cv::RotatedRect res;
	if (idx>=0 && idx < _cellRegions.size())
		return _cellRegions.at(idx);

	return res;
}

cv::Rect CellExtractor::cellRect( int idx )
{
	cv::Rect res;
	if (idx>=0 && idx < _cellRegions.size())
		return _cellRects.at(idx);

	return res;
}

const vector<vector<Point> > & CellExtractor::cellContours()
{
	return _contours;
}


void moveContour(vector<Point>& contour, int dx, int dy)
{
    for (size_t i=0; i<contour.size(); i++)
    {
        contour[i].x += dx;
        contour[i].y += dy;
    }
}



/**
Finds cells in image.
img - image
dataHash - unique image identifier for caching
minCellNumber - don't return any if less than minCellNumber
findSimilar - use only cells with common similarity
 0 - all cells
 1 - mean if center in upper 1/4
 2 - mean if center in middle 2/4
 3 - mean if center in lower 1/4
 TODO 4 - sizes and mean of centers

useRoi - use cells only if in defined ROI
 0 - in entire image
 1 - 1/4 of area, up-left
 2 - 1/4 of area, up-right
 3 - 1/4 of area, down-left
 4 - 1/4 of area, down-right
 5 - 1/4 of area, center

Cell location algorithm
*/
const TRotatedRectList  & CellExtractor::findCellRects( cv::Mat img, QString dataHash, int minCellNumber, int findSimilar, int useRoi )
{	
	QString dataId;
	_cellRegions.clear();

	try{
		_imgIn = img.clone();
		_cellRegions.clear();
		_cellRects.clear();
		_contours.clear();
		_roiX= 0;
		_roiY= 0;
		int roiw = img.cols;
		int roih = img.rows;
		

		if(useRoi) //Use 1/4 of the area
		{
			roiw /= 2; 
			roih /= 2; 
			switch (useRoi)
			{
				case 1: //roi up-left
					_roiX= 0;
					_roiY= 0;
					break;
				case 2: //roi up-right
					_roiX= img.cols/2;
					_roiY= 0;
					break;
				case 3: //roi down-left
					_roiX= 0;
					_roiY= img.rows/2;
					break;
				case 4: //roi down-right
					_roiX= img.cols/2;
					_roiY= img.rows/2;
					break;
				case 5: //roi middle
					_roiX= img.cols/4;
					_roiY= img.rows/4;
					break;

			}
		}
		cv::Rect roi( _roiX, _roiY, roiw, roih ); //Use only 1/4 in the middle
		_imgIn = img(roi).clone();

		_norm = Mat::zeros( _imgIn.rows, _imgIn.cols, CV_8UC1 );
		_mask = Mat::zeros(_imgIn.rows, _imgIn.cols, CV_8UC1);		

		normalize(_imgIn,_norm,0,_levels,CV_MINMAX);

		Mat th = Mat::zeros( _imgIn.rows, _imgIn.cols, CV_8UC1 );

		vector<vector<Point> > allContours;
		vector<cv::Rect> allRegions;
		if ( findSimilar == 10 ){
			_minThreshold = _maxThreshold -2 ;
		}
		for ( int testTh=_minThreshold; testTh<_maxThreshold; testTh++ )
		{
			cv::threshold(_norm ,th,testTh, 255,  CV_THRESH_BINARY);
			/*Mat normTmp;
			normalize(th,normTmp,0,255,CV_MINMAX);
			cv::imshow("normTmp", normTmp);
			cv::waitKey(0);*/
				
			vector<vector<Point> > contours;		

  			findContours(th, contours, CV_RETR_EXTERNAL, CV_CHAIN_APPROX_NONE);   
			if ( contours.size() < minCellNumber )//TODO parameter for minimum cell number
				continue;

			int numOfCells=0;
			for( unsigned int i = 0; i < contours.size(); i++ )
			{ 
				if( contours[i].size() < 5  )
					continue;

				RotatedRect region = minAreaRect( Mat(contours[i]) ); //TODO use diameter and compare with circle area of similar diameter
				if (( region.size.width >= _minCell && region.size.width <= _maxCell  )
				&&
				( region.size.height >= _minCell && region.size.height <= _maxCell  ) )
				{
					//if ( findSimilar == 10 && (std::max( region.size.height, region.size.width ) / std::min( region.size.height, region.size.width ) < 2) )
					//	continue;
					int foundBigger=false;
					cv::Rect newRect = region.boundingRect();
					foreach( Rect cr, allRegions )
					{
						double newRectArea = (double)newRect.area();
						double fitArea1 = (double)(cr & newRect).area();
						double fitArea2 = (double)(newRect & cr).area();
						if ( fitArea1 > 0 )
						{
							if ( fitArea1 > newRectArea * .6 )
							{
								foundBigger = true;
								break;
							}							
						}
					}
					if (!foundBigger)
					{
						allContours.push_back( contours[i] );
						allRegions.push_back( newRect );
					}
				}						
			}						
		}
	
		
		QMultiMap<int, cv::RotatedRect > _cellAreas;
		for( unsigned int i = 0; i < allContours.size(); i++ )
		{ 
			if( (  findSimilar >= 10 ) && (allContours[i].size() < 5)  ) // fsimilas > 10 special treatment
			{
				continue;
			}
			cv::RotatedRect region = fitEllipse( Mat(allContours[i]) );
			region.center.x += _roiX;
			region.center.y += _roiY;
			cv::Rect br = region.boundingRect();

			/// use cell only if entirely within image and within defined size-range
			if ( (br.x>=0 && br.y >=0 && (br.x + br.width)< img.size().width && (br.y + br.height)< img.size().height )
				 &&
				 ( region.size.width >= _minCell && region.size.width <= _maxCell  )
				 &&
				 ( region.size.height >= _minCell && region.size.height <= _maxCell  ) )
			{
				drawContours(_mask, allContours, i, Scalar(255), CV_FILLED);
				_cellRegions.push_back( region  );
				_cellRects.push_back( br ); 
				_contours.push_back( allContours.at(i) );
				cv::Size cs = region.size;
				int rectEdge = qSqrt(cs.area());
				if( (rectEdge+rectEdge/2) > cs.height)             //Should be oval, not line
				{
					_cellAreas.insert(region.size.area(), region);
				}
				else if ( findSimilar >= 10 )
				{
					_cellAreas.insert(region.size.area(), region);//Except it is likely to be mitosis
				}
			}
		}

		//cv::imshow( "Mask", _mask );
		

		if (findSimilar)
		{
			QMultiMap<int, cv::RotatedRect > _cells;
			
			for ( QMultiMap<int, cv::RotatedRect >::iterator it = _cellAreas.begin(); it != _cellAreas.end(); ++it)
			{
				cv::Rect br = it.value().boundingRect();
				br.x = br.x+br.width/4;
				br.y = br.y+br.height/4;
				br.width = br.width/2;
				br.height = br.height/2;
				if ( (br.x>=0 && br.y >=0 && (br.x + br.width)< img.size().width && (br.y + br.height)< img.size().height ) )
				{
					Mat c = img(br);		

					cv::Mat cellMask(c.size(), CV_8UC1);
					cv::threshold(c, cellMask, 35, 255, cv::THRESH_BINARY);					
					int cmean = cv::mean(c,cellMask)[0];
					_cells.insert( cmean, it.value() );
				}
			}

			int cells = _cells.size();
			int from= 0; 
			int to = cells;

			switch ( findSimilar )
			{
				case 1:
					to = cells / 4; //use the 1/4 of lower
					break;
				case 2:
					from= (cells/4); // 1/4
					to = from * 3; //use the 2/4 in the middle
					break;
				case 3:
					from = (cells/4)*3; // 1/4 of upper
					break;
				case 4:
					{
						int lastVal=0;
						int lastDiff = 0;
						int idx=0;
						for ( QMultiMap<int, cv::RotatedRect >::iterator it = _cells.begin(); it != _cells.end(); ++it)
						{
							if ( lastVal != 0 )							
							{
								int diff = it.key() - lastVal;
								if ( lastDiff <= diff )
								{
									lastDiff = diff;
									from = idx;
								}
							}
							lastVal = it.key();	
							idx++;
						}
					}
					break;
				case 5:
					from = cells -6 > 0 ? cells -6 : cells; //use brightest 5 (likely to be mitosis)
					to = cells; //use brightest 5 (likely to be mitosis)
					break;
				default:
					//TODO
					break;
			}

			int idx=0;
			_cellRegions.clear();
			for ( QMultiMap<int, cv::RotatedRect >::iterator it = _cells.begin(); it != _cells.end(); ++it)
			{
				if (idx<to && idx > from)
					_cellRegions.push_back( it.value() );

				idx++;
			}
		}
	}
	catch( ... )
	{
		int i;
		i++;
	}
	if ( !dataId.isEmpty() )
	{
		_cellMx.lock();
		 _RotatedRectListCache[dataId] = _cellRegions;		
		 _RectListCache[dataId] = _cellRects;
		 _ContoursCache[dataId] = _contours;
        _cellMx.unlock();
	}
	
	return _cellRegions;
}


/**
Finds cells in image.
img - image
dataHash - unique image identifier for caching
minCellNumber - don't return any if less than minCellNumber
findSimilar - use only cells with common similarity
 0 - all cells
 1 - mean if center in upper 1/4
 2 - mean if center in middle 2/4
 3 - mean if center in lower 1/4
 TODO 4 - sizes and mean of centers

useRoi - use cells only if in defined ROI
 0 - in entire image
 1 - 1/4 of area, up-left
 2 - 1/4 of area, up-right
 3 - 1/4 of area, down-left
 4 - 1/4 of area, down-right
 5 - 1/4 of area, center

Cell location algorithm
*/

const TRotatedRectList  & CellExtractor::findCellRects1( cv::Mat img, QString dataHash, int minCellNumber, int findSimilar, int useRoi )
{	

	QString dataId;
	_cellRegions.clear();	
	_cellRects.clear();
	_contours.clear();
	_roiX= 0;
	_roiY= 0;
	
	
	try{


		
		int roiw = img.cols;
		int roih = img.rows;
		

		if(useRoi) //Use 1/4 of the area
		{
			roiw /= 2; 
			roih /= 2; 
			switch (useRoi)
			{
				case 1: //roi up-left
					_roiX= 0;
					_roiY= 0;
					break;
				case 2: //roi up-right
					_roiX= img.cols/2;
					_roiY= 0;
					break;
				case 3: //roi down-left
					_roiX= 0;
					_roiY= img.rows/2;
					break;
				case 4: //roi down-right
					_roiX= img.cols/2;
					_roiY= img.rows/2;
					break;
				case 5: //roi middle
					_roiX= img.cols/4;
					_roiY= img.rows/4;
					break;

			}
		}
		cv::Rect roi( _roiX, _roiY, roiw, roih ); //Use only 1/4 in the middle
		_imgIn = img(roi).clone();




		_norm = Mat::zeros( _imgIn.rows, _imgIn.cols, CV_8UC1 );
		_mask = Mat::zeros(_imgIn.rows, _imgIn.cols, CV_8UC1);		
		
		normalize(_imgIn,_norm,0,255,CV_MINMAX);

		//cv::imshow("_norm", _norm);

		cv::Mat bw;// = src.clone();

		cv::threshold(_norm, bw, 128, 255, CV_THRESH_BINARY);
		//cv::imshow("bw", bw);
		// Perform the distance transform algorithm
		

		std::vector<std::vector<cv::Point> > contours;
		cv::findContours(bw, contours, CV_RETR_EXTERNAL, CV_CHAIN_APPROX_SIMPLE);

		QMultiMap<int, cv::RotatedRect > _cellAreas;
		for( unsigned int i = 0; i < contours.size(); i++ )
		{ 
			if( contours[i].size() < 5  )
				continue;
			cv::RotatedRect region = fitEllipse( Mat(contours[i]) );
			region.center.x += _roiX;
			region.center.y += _roiY;
			cv::Rect br = region.boundingRect();

			/// use cell only if entirely within image and within defined size-range
			if ( (br.x>=0 && br.y >=0 && (br.x + br.width)< img.size().width && (br.y + br.height)< img.size().height )
				 &&
				 ( region.size.width >= _minCell && region.size.width <= _maxCell  )
				 &&
				 ( region.size.height >= _minCell && region.size.height <= _maxCell  ) )
			{
				drawContours(_mask, contours, i, Scalar(255), CV_FILLED);
				_cellRegions.push_back( region  );
				_cellRects.push_back( br ); 
				_contours.push_back( contours.at(i) );
				cv::Size cs = region.size;
				int rectEdge = qSqrt(cs.area());
				if( (rectEdge+rectEdge/2) > cs.height)
					_cellAreas.insert(region.size.area(), region);
			}
		}

		//cv::imshow("Mask", _mask);
		

		if (findSimilar)
		{
			QMultiMap<int, cv::RotatedRect > _cells;

			for ( QMultiMap<int, cv::RotatedRect >::iterator it = _cellAreas.begin(); it != _cellAreas.end(); ++it)
		{
				cv::Rect br = it.value().boundingRect();
				br.x = br.x+br.width/4;
				br.y = br.y+br.height/4;
				br.width = br.width/2;
				br.height = br.height/2;
				if ( (br.x>=0 && br.y >=0 && (br.x + br.width)< img.size().width && (br.y + br.height)< img.size().height ) )
			{
					Mat c = img(br);					
					cv::Mat cellMask(c.size(), CV_8UC1);
					cv::threshold(c, cellMask, 35, 255, cv::THRESH_BINARY);					
					int cmean = cv::mean(c,cellMask)[0];
					_cells.insert( cmean, it.value() );
				}
			}

			int cells = _cells.size();
			int from= 0; 
			int to = cells;

			switch ( findSimilar )
			{
				case 1:
					to = cells / 4; //use the 1/4 of lower
					break;
				case 2:
					from= (cells/4); // 1/4
					to = from * 3; //use the 2/4 in the middle
					break;
				case 3:
					from= (cells/4)*3; // 1/4 of upper
					break;
				case 4:
					{
						int lastVal=0;
						int lastDiff = 0;
						int idx=0;
						for ( QMultiMap<int, cv::RotatedRect >::iterator it = _cells.begin(); it != _cells.end(); ++it)
						{
							if ( lastVal != 0 )							
							{
								int diff = it.key() - lastVal;
								if ( lastDiff < diff )
								{
									lastDiff = diff;
									from = idx;
								}
							}
							lastVal = it.key();	
							idx++;
						}
					}
					break;
				default:
					//TODO
					break;
		}

			int idx=0;
			_cellRegions.clear();
			for ( QMultiMap<int, cv::RotatedRect >::iterator it = _cells.begin(); it != _cells.end(); ++it)
			{
				if (idx<to && idx > from)
					_cellRegions.push_back( it.value() );

				idx++;

			}
		}

	}
	catch( ... )
	{
		//TODO LOG!
		int i;
		i++;
	}
	if ( !dataId.isEmpty() )
	{
		_cellMx.lock();
		 _RotatedRectListCache[dataId] = _cellRegions;		
		 _RectListCache[dataId] = _cellRects;
		 _ContoursCache[dataId] = _contours;
        _cellMx.unlock();
	}
	_imgIn = img.clone();
	return _cellRegions;
}


#ifdef FindReallyAllCells
/*
Cell location algorithm
*/
const TRotatedRectList  & CellExtractor::findCellRects( cv::Mat img, QString dataHash, int minCellNumber )
{		

	QString dataId;
	
	
	
	try{

		QMap<int, vector<vector<Point> > > level_contours;
		_cellRegions.clear();
		_cellRects.clear();
		_contours.clear();
		Mat kernel = cv::getStructuringElement(cv::MORPH_ELLIPSE,
                      cv::Size(3,3), 
                      cv::Point(1,1) );
		const int tiles = 2;
		for (int rx=0; rx < tiles; rx++)
		{
			for (int ry=0; ry < tiles; ry++)
			{

				int roiX = img.cols/tiles *rx;
				int roiY = img.rows/tiles *ry;

				cv::Rect roi( roiX, roiY, img.cols/tiles, img.rows/tiles );

				//_imgIn = img.clone();//Mat::zeros( img.rows, img.cols, CV_8UC1 );// TODO check type of input image
				//img.copyTo(_imgIn);
				_imgIn = img(roi).clone();

				if ( !dataHash.isEmpty() )
				{
					dataId = QString( "%1_%2_%3_%4_%5_%6" )
					.arg((int)_minCell)
					.arg((int)_maxCell)
					.arg(_minThreshold)
					.arg(_maxThreshold)
					.arg(_levels)
					.arg(dataHash);
					if ( _RotatedRectListCache.contains(dataId) )
					{
						_cellRects = _RectListCache[dataId];
						_contours = _ContoursCache[dataId];
						return _RotatedRectListCache[dataId];
					}
				}

				_norm = Mat::zeros( _imgIn.rows, _imgIn.cols, CV_8UC1 );
				_mask = Mat::zeros(_imgIn.rows, _imgIn.cols, CV_8UC1);		
				
				/*QString winname = QString("Tile:%1_%2").arg(rx).arg(ry);
				Mat test = _imgIn.clone();
				imshow(winname.toStdString().c_str(), _imgIn );
				*/

				normalize(_imgIn,_norm,0,_levels,CV_MINMAX);

				/*
				winname = QString("Norm:%1_%2").arg(rx).arg(ry);
				normalize(_norm,test,0,255,CV_MINMAX);
				imshow(winname.toStdString().c_str(),test );
				*/

				int bestTh=_minThreshold;
				int maxNumOfCells=0;
				

				Mat th = Mat::zeros( _imgIn.rows, _imgIn.cols, CV_8UC1 );

								

				for ( int testTh=_minThreshold; testTh<_maxThreshold; testTh++ )
				{
					

						
					vector<vector<Point> > contours;
					for (int s=0; s<=2; ++s)
					{
						if ( s == 0 )
						{
							cv::threshold(_norm ,th,testTh, 255,  CV_THRESH_BINARY);
						}
						else if ( s == 1 )
						{
							cv::threshold(_norm ,th,testTh, 255,  CV_THRESH_BINARY);
							erode(th,th,kernel);
						}
						else if ( s == 2 )
						{
							cv::threshold(_norm ,th,testTh, 255,  CV_THRESH_BINARY);
							cv::dilate(th,th,kernel);
						}
  						findContours(th, contours, CV_RETR_EXTERNAL, CV_CHAIN_APPROX_NONE);   
						if ( contours.size() < minCellNumber )//TODO parameter for minimum cell number
							continue;

						int numOfCells=0;
						for( unsigned int i = 0; i < contours.size(); i++ )
						{ 
							if( contours[i].size() < 5  )
								continue;

							RotatedRect region = minAreaRect( Mat(contours[i]) );
							if (( region.size.width >= _minCell && region.size.width <= _maxCell  )
							&&
							( region.size.height >= _minCell && region.size.height <= _maxCell  ) )
							{
								numOfCells++;
								moveContour( contours[i], roiX, roiY );
								level_contours[testTh].push_back(contours[i]);
							}						
						}
						/*if( numOfCells > maxNumOfCells )
						{
							bestTh = testTh;
							maxNumOfCells = numOfCells;
						}*/
					}
					
				}

				//cv::threshold(_norm ,th,bestTh, 255,  CV_THRESH_BINARY);

				
			}
		}

		vector<vector<Point> > contours;
		vector< cv::Rect > contourCenters;
		for( QMap<int, vector<vector<Point> > >::iterator it =  level_contours.begin(); it != level_contours.end();  ++it )
		{
			foreach( vector<Point> c, it.value() )
			{
				cv::Rect cr = fitEllipse( Mat(c) ).boundingRect();
				cr.x = cr.x+cr.width/4;
				cr.y = cr.y+cr.height/4;
				cr.width = cr.width/2;
				cr.height = cr.height/2;

				bool found=false;
				foreach( cv::Rect r, contourCenters )
				{
					cv::Rect ir = (r & cr);
					if ( ir.area()>0 )
					{
						/*if ( cr.area() < r.area() ) TODO leave the smallest!
						{
							contourCenters.
						}*/
						found = true;
						break;
					}

				}
				if (!found)
				{
					contourCenters.push_back(cr);
					contours.push_back(c);
				}
			}
		}

		//findContours(th, contours, CV_RETR_EXTERNAL, CV_CHAIN_APPROX_NONE);   
		
		QMultiMap<int, cv::RotatedRect > _cellAreas;
		for( unsigned int i = 0; i < contours.size(); i++ )
		{ 
			if( contours[i].size() < 5  )
				continue;
			cv::RotatedRect region = fitEllipse( Mat(contours[i]) );
			//region.center.x += roiX;
			//region.center.y += roiY;
			cv::Rect br = region.boundingRect();

			/// use cell only if entirely within image and within defined size-range
			if ( (br.x>=0 && br.y >=0 && (br.x + br.width)< _imgIn.size().width && (br.y + br.height)< _imgIn.size().height )
				 &&
				 ( region.size.width >= _minCell && region.size.width <= _maxCell  )
				 &&
				 ( region.size.height >= _minCell && region.size.height <= _maxCell  ) )
			{
				drawContours(_mask, contours, i, Scalar(255), CV_FILLED);
				_cellRegions.push_back( region );
				_cellRects.push_back( br );
				_contours.push_back( contours.at(i) );
				cv::Size cs = region.size;
				int rectEdge = qSqrt(cs.area());
				if( (rectEdge+rectEdge/2) > cs.height)
					_cellAreas.insert(region.size.area(), region);
			}
		}
		

		QMultiMap<int, cv::RotatedRect > _cells;
		
		for ( QMultiMap<int, cv::RotatedRect >::iterator it = _cellAreas.begin(); it != _cellAreas.end(); ++it)
		{
			cv::Rect br = it.value().boundingRect();
			if ( (br.x>=0 && br.y >=0 && (br.x + br.width)< img.size().width && (br.y + br.height)< img.size().height ) )
			{
				Mat c = img(br);
				int cmean = cv::mean(c)[0];
				_cells.insert( cmean, it.value() );
			}
		}

		int cells = _cells.size();
		int from= (cells/3); // 1/3
		int to = from + from; //use the 1/3 in the middle

		int idx=0;
		_cellRegions.clear();
		for ( QMultiMap<int, cv::RotatedRect >::iterator it = _cells.begin(); it != _cells.end(); ++it)
		{
			if (idx<to && idx > from)
				_cellRegions.push_back( it.value() );

			idx++;

		}


	}
	catch( ... )
	{
		int i;
		i++;
	}
	if ( !dataId.isEmpty() )
	{
		_cellMx.lock();
		 _RotatedRectListCache[dataId] = _cellRegions;		
		 _RectListCache[dataId] = _cellRects;
		 _ContoursCache[dataId] = _contours;
        _cellMx.unlock();
	}
	return _cellRegions;
}


#endif

cv::Mat CellExtractor::cellAt( cv::Point p, int*index )
{

	cv::Mat result;
	if ( !_imgIn.data /*|| !cv::boundingRect(_imgIn).contains(p) TODO check this*/ )
		return result;

	int idx=0;
	for( TRectList::const_iterator prect =_cellRects.begin(), end =_cellRects.end(); prect!=end; ++prect )
	{		
		if ( (*prect).contains(p) )
		{
			result = Mat::zeros( (*prect).size(), CV_8UC1);
			_imgIn( *prect ).copyTo(result);
			break;
		}
		++idx;
	}
	if ( index )
		*index = idx;
	return result;
}

cv::Mat CellExtractor::cell( int idx )
{	
	if ( idx >= 0 && idx < _cellRects.size() )
	{
		return _imgIn( _cellRects.at(idx) );
	}
	return Mat();
}

cv::Mat CellExtractor::cellMask( int idx )
{
	if ( idx >= 0 && idx < _cellRects.size() )
	{
		return _mask( _cellRects.at(idx) );
	}
	return Mat();
}


cv::Mat CellExtractor::cellsMask() const 
{ 
	return _mask; 
}

cv::Mat CellExtractor::cellsNormImage() const 
{ 
	return _norm; 
}

cv::Mat CellExtractor::normalizedCellAt( cv::Point p, const cv::Size & outSize, int*index )
{	
	Mat result;

	int idx=0;
	cv::Mat cell = cellAt( p, &idx ); //TODO not required
	if ( !cell.data )
		return result;	
	if ( index )
		*index = idx;
	
	cv::Mat mask = Mat::zeros( _imgIn.size(), CV_8UC1 );
	cv::Mat oneCellImage = Mat::zeros( _imgIn.size(), CV_8UC1 );//TODO Optimize!!!
	drawContours(mask, _contours, idx, Scalar(255), CV_FILLED);	
	_imgIn.copyTo( oneCellImage, mask );
	cell = oneCellImage( _cellRects.at(idx) );


	result = normalizeCell( cell, cellRegion(idx), outSize );

	
	return result;
}

cv::Mat CellExtractor::normalizeCell( cv::Mat cell, const cv::RotatedRect & rr, const cv::Size & outSize  )
{
	//create bigger scene for working with rotated image
	Mat working_scene =  Mat::zeros( cell.size()*4, CV_8UC1 );
	
	//copy cell in scene center for not loosing edges on rotation
	cv::Rect cellAreaRect = cv::Rect( working_scene.size().width/4, working_scene.size().height/4,working_scene.size().width/2, working_scene.size().height/2);
	cv::Mat cellArea = working_scene(cellAreaRect);
	cell.copyTo(cellArea);

	//cv::imshow("WorkingScene",working_scene );

	Point center = Point( working_scene.size().width/2, working_scene.size().height/2 );
    double angle = rr.angle;
    double scale = 1;

    /// Prepare rotatin matrix
    Mat rot_mat = getRotationMatrix2D( center, angle, scale );

   /// Rotate the image
   warpAffine( working_scene, working_scene, rot_mat, working_scene.size() );

   //cv::imshow("Rotated",working_scene );

   vector<vector<Point> > contours;

   //TODO check transformation/offset on rotation   
   cellAreaRect = cv::Rect( working_scene.size().width/2 - rr.size.width/2, working_scene.size().height/2 - rr.size.height/2, rr.size.width, rr.size.height );
   Mat ret;

  ret = working_scene( cellAreaRect );

  
  if (!ret.data)
  	  return ret;

  //cv::imshow("IsolatedImg",ret );

  Point2f srcTri[3];
  Point2f dstTri[3];
  srcTri[0] = Point2f( 0,0 );
   srcTri[1] = Point2f( ret.cols - 1, 0 );
   srcTri[2] = Point2f( 0, ret.rows - 1 );

   double wf = double(outSize.width) / cellAreaRect.size().width;
   double hf = double(outSize.height) / cellAreaRect.size().height;

   dstTri[0] = Point2f( 0,0);
   dstTri[1] = Point2f( ret.cols*wf , 0 );
   dstTri[2] = Point2f( 0,ret.rows *hf );
		  
   Mat warp_mat( 2, 3, CV_32FC1 );
   warp_mat = getAffineTransform( srcTri, dstTri );
   Mat result =  Mat::zeros( outSize, CV_8UC1 );
   warpAffine( ret, result, warp_mat, outSize );
	
  return result;
}

QString CellExtractor::getCellPixelFeatures( int offset, cv::Mat cell, const cv::Size & outSize )
{
	QString svmline;
	cv::Mat_<unsigned char> mcell =  cell;	
	int ftIdx=offset;
	for( int x=0; x<outSize.height; ++x )
	{
		for( int y=0; y<outSize.width; ++y )		
		{
			double val = mcell(x,y);
			double vmin = 0; 
			double vmax = 255;			
			double sval = -1.0 + (1.0 - -1.0) * (val - vmin)/(vmax-vmin);
			if ( sval != sval ) //isnan
				sval = 0.0;

			svmline+=QString(" %1:%2").arg(ftIdx).arg(sval);
			ftIdx++;
		}
	}
	return svmline;
}

QMap<int, double> CellExtractor::getCellPixelFeaturesMap( int offset, cv::Mat cell, const cv::Size & outSize )
{
	QMap<int, double> svmline;
	cv::Mat_<unsigned char> mcell =  cell;	
	int ftIdx=offset;
	for( int x=0; x<outSize.height; ++x )
	{
		for( int y=0; y<outSize.width; ++y )		
		{
			double val = mcell(x,y);
			double vmin = 0; 
			double vmax = 255;			
			double sval = -1.0 + (1.0 - -1.0) * (val - vmin)/(vmax-vmin);
			if ( sval != sval ) //isnan
				sval = 0.0;

			svmline[ftIdx] = sval;
			ftIdx++;
		}
	}
	return svmline;
}