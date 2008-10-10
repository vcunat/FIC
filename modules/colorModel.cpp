#include "colorModel.h"
#include "../fileUtil.h"

#include <QImage>
using namespace std;

const SReal
    MColorModel::YCbCrCoeffs[][4]= {
        { 0.299,     0.587,     0.114,    0   },
        {-0.168736, -0.331264,  0.5,      0.5 },
        { 0.5,      -0.418688, -0.081312, 0.5 },

        { 1,         1,         1,        0   },
        { 0,        -0.34414,   1.772,   -0.5 },
        { 1.402,    -0.71414,   0,       -0.5 }
    },
    MColorModel::RGBCoeffs[][4]= {
        {1,0,0,0},{0,1,0,0},{0,0,1,0},
        {1,0,0,0},{0,1,0,0},{0,0,1,0}
    };


MColorModel::PlaneList MColorModel::image2planes(const QImage &image,const Plane &prototype) {
	assert( !image.isNull() );
//	get the dimensions and create the planes
	int width= image.width(), height= image.height();
	PlaneList result= createPlanes(IRoot::Encode,prototype,width,height);
//	get the correct coefficients and plane count
	const SReal (*coeffs)[4]= (colorModel() ? YCbCrCoeffs : RGBCoeffs);
	int planeCount= result.size();
//	fill pixels in all planes
	for (int i=0; i<planeCount; ++i) {
		SReal **pixels= result[i].pixels;
	//	fill the pixels in this plane
		for (int y=0; y<height; ++y) {
		//	fill pixels in this line
			const QRgb *line= (QRgb*)image.scanLine(y);
			for (int x=0; x<width; ++x)
				pixels[x][y]= getColor(line[x],coeffs[i]);
		}
	}
	return result;
}

QImage MColorModel::planes2image(const MatrixList &pixels,int width,int height) {
	assert( colorModel()>=0 && colorModel()<numOfModels() && pixels.size()==3 );
//	get the correct coefficients
	const SReal (*coeffs)[4]= 3 + (colorModel() ? YCbCrCoeffs : RGBCoeffs);
//	create and fill the image
	QImage result( width, height, QImage::Format_RGB32 );

	for (int y=0; y<height; ++y) {
		QRgb *line= (QRgb*)result.scanLine(y);
		for (int x=0; x<width; ++x) {
			SReal vals[3]= {
				pixels[0][x][y],
				pixels[1][x][y],
				pixels[2][x][y]
			};
			line[x]= getColor( coeffs, vals );
		}
	}
	return result;
}

void MColorModel::writeData(std::ostream &file) {
	put<Uchar>( file , colorModel() );
}

MColorModel::PlaneList MColorModel::readData
( std::istream &file, const Plane &prototype, int width, int height ) {
//	read the color-model identifier and check it
	colorModel()= get<Uchar>(file);
	checkThrow( 0<=colorModel() && colorModel()<numOfModels() );
	return createPlanes(IRoot::Decode,prototype,width,height);
}

MColorModel::PlaneList MColorModel::createPlanes
( IRoot::Mode mode, const Plane &prototype, int width, int height ) {
	assert( 0<=colorModel() && colorModel()<numOfModels() 
	&& width>0 && height>0 && mode!=IRoot::Clear );
//	create the plane list	
	int planeCount= 3;
	PlaneList result( planeCount, prototype );
	for (int i=0; i<planeCount; ++i)
		result[i].pixels= newMatrix<SReal>(width,height);
	//	TODO (admin#4#): We don't adjust max. domain count and quality in encoding mode
	return result;
}
