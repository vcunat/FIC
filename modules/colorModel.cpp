#include "colorModel.h"
#include "fileUtil.h"
#include <QTreeWidgetItem>
using namespace std;

const float
    MColorModel::YCbCrCoeffs[][4]={
        { 0.299,     0.587,     0.114,    0   },
        {-0.168736, -0.331264,  0.5,      0.5 },
        { 0.5,      -0.418688, -0.081312, 0.5 },

        { 1,         1,         1,        0   },
        { 0,        -0.34414,   1.772,   -0.5 },
        { 1.402,    -0.71414,   0,       -0.5 }
    },
    MColorModel::RGBCoeffs[][4]={
        {1,0,0,0},{0,1,0,0},{0,0,1,0},
        {1,0,0,0},{0,1,0,0},{0,0,1,0}
    };


MColorModel::PlaneList MColorModel::image2planes
( const QImage &image, const Plane &prototype ) {
//	get the correct coeffitients and plane count
	assert( colorModel()>=0 && colorModel()<numOfModels() );
	const float (*coeffs)[4]= ( colorModel()==0 ? RGBCoeffs : YCbCrCoeffs );
	int planeCount=3;
//	create all planes
	PlaneList result( planeCount, prototype );
	int width=image.width(), height=image.height();
	for (int i=0; i<planeCount; ++i,++coeffs) {
	//	get one plane
		float **pixels=newMatrix<float>(width,height);
		result[i].pixels=pixels;
	//	fill the pixels
		for (int y=0; y<height; ++y) {
			const QRgb *line=(QRgb*)image.scanLine(y);
			for (int x=0; x<width; ++x,++line)
				pixels[x][y]=getColor(*line,*coeffs);
		}
	//	TODO (admin#4#): We don't adjust quality and max. domain count
	}
	return result;
}

QImage MColorModel::planes2image(const MatrixList &pixels,int width,int height) {
//	get the correct coeffitients
	assert( colorModel()>=0 && colorModel()<numOfModels() );
	const float (*coeffs)[4]= 3+( colorModel()==0 ? RGBCoeffs : YCbCrCoeffs );
//	create and fill the image
	QImage result( width, height, QImage::Format_RGB32 );

	for (int y=0; y<height; ++y) {
		QRgb *line=(QRgb*)result.scanLine(y);
		for (int x=0; x<width; ++x,++line) {
			float vals[3]={
				pixels[0][x][y],
				pixels[1][x][y],
				pixels[2][x][y]
			};
			*line=getColor( coeffs, vals );
		}
	}
	return result;
}

void MColorModel::writeData(std::ostream &file) {
	put<Uchar>( file , colorModel() );
}

int MColorModel::readData(std::istream &file) {
//	read the color-model identifier and check it
	colorModel()=get<Uchar>( file );
	if ( colorModel()<0 || colorModel()>=numOfModels() )
		throw std::exception();
//	return the number of planes
	return 3;
}


