#ifndef ROOT_HEADER_
#define ROOT_HEADER_

#include "../interfaces.h"

//#include <QThread>

/** The root module */
class MRoot: public IRoot {

 	DECLARE_M_cloning_name_desc( MRoot, "Root"
 	, "Standard root module" );

	DECLARE_M_settings_type({
		type:	Int,
		data: {	i:{1,1} },
		label:	"Maximal number of threads",
		desc:	"Note: the actual number of threads is bound by\n"
				"(the number of parts)*(the number of color planes)"
	}, {
		type:	ModuleCombo,
		data: {	compatIDs: &IColorTransformer::getCompMods() },
		label:	"Color transformer",
		desc:	"The module that will be used to transform colors"
	}, {
		type:	ModuleCombo,
		data: {	compatIDs: &IShapeTransformer::getCompMods() },
		label:	"Pixel-shape transformer",
		desc:	"The module that is responsible for shape-transformation\n"
				"of the pixels and for further (de)compression"
	}, {
		type:	Int,
		data: {	i:{0,100} },
		label:	"Encoding quality",
		desc:	"Quality - how big loss will be tolerated by the encoder"
	}, {
		type:	ModuleCombo,
		data: {	compatIDs: &IQuality2SquareError::getCompMods() },
		label:	"Quality converter",
		desc:	"For given quality and size computes maximum square error allowed"
	}, {
		type:	IntLog2,
		data: {	i: {0,18} },
		label:	"Maximum domain count",
		desc:	"Maximum domain count for level 2 range blocks\n"
				"(for this purpose are different rotations "
				"of one domain counted as different domains)"
	});

	DECLARE_M_settings_default(
		1,	//abs(QThread::idealThreadCount()), // the max. number of threads
		0,	// deafult color transformer
		0,	// default shape transformer
		50,	// encoding quality
		0,	// quality converter
		12	// max. domain count
	);
private:
	/** Indices for settings */
	enum Settings { MaxThreads, ModuleColor, ModuleShape, Quality, ModuleQuality
	, DomainCountLog2 };
//	Settings-retrieval methods
	int maxThreads() const
		{ return settings[MaxThreads].i; }
	IColorTransformer* moduleColor() const
		{ return debugCast<IColorTransformer*>(settings[ModuleColor].m); }
	IShapeTransformer* moduleShape() const
		{ return debugCast<IShapeTransformer*>(settings[ModuleShape].m); }
	float quality()
		{ return settings[Quality].i/100.0; }
	IQuality2SquareError* moduleQuality() const
		{ return debugCast<IQuality2SquareError*>(settings[ModuleQuality].m); }

	typedef IColorTransformer::Plane Plane;
	typedef IColorTransformer::PlaneList PlaneList;

private:
//	Module's data
	Mode myMode;
	int width, height;

protected:
//	Construction and destruction
	MRoot(): myMode(Clear), width(0), height(0) {}

public:
/**	\name IRoot interface
 *	@{ */
	Mode getMode()		{ return myMode; }
	QImage toImage();

	bool encode(const QImage &toEncode);
	void decodeAct(DecodeAct action,int count=1);

	bool toFile(const char *fileName);
	bool fromFile(const char *fileName);
///	@}
};

#endif // ROOT_HEADER_
