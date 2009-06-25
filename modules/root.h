#ifndef ROOT_HEADER_
#define ROOT_HEADER_

#include "../headers.h"

#include <QThread> // ::idealThreadCount

/// \ingroup modules
/** The root module implementation. Controls the number of encoding threads,
 *	the color-transforming module (IColorTransformer)
 *	the pixel-shape-transforming module (IShapeTransformer), quality 0-100%,
 *	the module for quality conversion (IQuality2SE) and the maximum domain count. */
class MRoot: public IRoot {
	DECLARE_debugModule;

 	DECLARE_TypeInfo( MRoot, "Root"
 	, "Standard root module"
	, {
		label:	"Maximal number of threads",
		desc:	"Note: the actual number of threads is bound by\n"
				"(the number of parts)*(the number of color planes)",
		type:	settingInt(1,QThread::idealThreadCount(),16)
	}, {
		label:	"Color transformer",
		desc:	"The module that will be used to transform colors",
		type:	settingModule<IColorTransformer>()
	}, {
		label:	"Pixel-shape transformer",
		desc:	"The module that is responsible for shape-transformation\n"
				"of the pixels and for further (de)compression",
		type:	settingModule<IShapeTransformer>()
	}, {
		label:	"Encoding quality (%)",
		desc:	"Quality - how much accurate the mappings have to be",
		type:	settingInt(0,90,100)
	}, {
		label:	"Quality converter",
		desc:	"For given quality and size computes maximum square error allowed",
		type:	settingModule<IQuality2SE>()
	}, {
		label:	"Maximum domain count",
		desc:	"Maximum domain count for level 2 range blocks\n"
				"(for this purpose are different rotations\n"
				"of one domain counted as different domains)",
		type:	settingInt(0,15,24,IntLog2)
	} )

protected:
	/** Indices for settings */
	enum Settings { MaxThreads, ModuleColor, ModuleShape, Quality, ModuleQuality
	, DomainCountLog2 };
//	Settings-retrieval methods
	int maxThreads() const
		{ return settingsInt(MaxThreads); }
	IColorTransformer* moduleColor() const
		{ return debugCast<IColorTransformer*>(settings[ModuleColor].m); }
	IShapeTransformer* moduleShape() const
		{ return debugCast<IShapeTransformer*>(settings[ModuleShape].m); }
	float quality()
		{ return settingsInt(Quality)/100.0; }
	IQuality2SE* moduleQuality() const
		{ return debugCast<IQuality2SE*>(settings[ModuleQuality].m); }

	typedef IColorTransformer::PlaneSettings PlaneSettings;
	typedef IColorTransformer::PlaneList PlaneList;

protected:
//	Module's data
	Mode myMode;///< the mode of the tree, returned by ::getMode
	int width	///  zoomed width of the image
	, height	///  zoomed height of the image
	, zoom;		///< the zoom used (dimensions multiplied by 2^\p zoom)

protected:
//	Construction and destruction
	MRoot(): myMode(Clear), width(0), height(0), zoom(-1) {}

public:
/**	\name IRoot interface
 *	@{ */
	Mode getMode()		{ return myMode; }
	QImage toImage();

	bool encode(const QImage &toEncode,const UpdateInfo &updateInfo);
	void decodeAct(DecodeAct action,int count=1);

	bool toStream(std::ostream &file);
	bool fromStream(std::istream &file,int zoom);
///	@}
};

#endif // ROOT_HEADER_
