#include "headers.h"
#include "imageUtil.h"

#include <iostream> // cout and cerr streams

#include <QDir>
#include <QFileInfo>
#include <QImage>
#include <QObject>
#include <QString>
#include <QTime>

using namespace std;

/** A shortcut for Qt's QObject::tr */
inline QString tr(const char *str) { return QObject::tr(str); }

/** Decodes a fractal image into a bitmap image */
void decodeFile(const char *inpName,QString outName) {
	IRoot *root= IRoot::compatiblePrototype().clone(Module::ShallowCopy);
	if ( !root->fromFile(inpName) )
		throw tr("Error while reading file \"%1\"") .arg(inpName);
	root->decodeAct(MTypes::Clear);
	root->decodeAct(MTypes::Iterate,10); ///< \todo constant 10
	if ( !root->toImage().save(outName) )
		throw tr("Error while writing file \"%1\"") .arg(outName);
}
/** Encodes a bitmap image into a fractal image using specified configuration file.
 *	It also measures times, PSNRs, compression ratios, etc. and outputs the information. */
void encodeFile(const char *inpName,QString outName,const char *confName=0) {
//	load the bitmap
	QImage image(inpName);
	if (image.isNull())
		throw tr("Can't read bitmap image \"%1\"") .arg(inpName);
	if ( image.format() != QImage::Format_RGB32 ) // convert to 24-bits
		image= image.convertToFormat(QImage::Format_RGB32);
	
	QTime time;
	time.start();
//	configure the module tree
	IRoot *root= IRoot::compatiblePrototype()
		.clone( confName ? Module::ShallowCopy : Module::DeepCopy );
	if (confName) {
		 if ( !root->allSettingsFromFile(confName) )
		     throw tr("Error while reading configuration file \"%1\"") .arg(confName);
	} else
		confName= "<default>";
//	encode the image
	if ( !root->encode(image) )
		throw tr("Error while encoding file \"%1\" with %2 configuration") 
			.arg(inpName) .arg( tr(confName) );
	float encTime= time.elapsed()/1000.0;
	int outSize= root->toFile(outName.toStdString().c_str());
	if (!outSize)
		throw tr("Can't write output file \"%1\"") .arg(outName);
	
//	decode the image and measure the PSNR
	time.restart();
	root->decodeAct(MTypes::Clear);
	root->decodeAct(MTypes::Iterate,10); ///< \todo constant 10
	float decTime= time.elapsed()/1000.0;
	vector<Real> psnr= Color::getPSNR( root->toImage(), image );
//	output the information
	cout << inpName << " " << confName << " ";		//< the input and config name
	for (int i=0; i<4; ++i)							//  the PSNRs
		cout << psnr[i] << " ";	
	Real grayRatio= image.width()*image.height() / Real(outSize);
	cout << grayRatio << " " << 3*grayRatio << " ";	//< gray and color compression ratio
	cout << encTime << " " << decTime << endl;		//< encoding and decoding time
}

/** A functor providing filename classification into one of FileClassifier::FileType */
struct FileClassifier {
	/** The used file types */
	enum FileType { Fractal, Bitmap, Config, Directory };
	
	FileType operator()(const char *name) const {
		QString suffix= QFileInfo(QString(name)).suffix();
		if (suffix=="")		return Directory;
		if (suffix=="fci")	return Fractal;
		if (suffix=="fcs")	return Config;
		else				return Bitmap;
	}
};

/* Declared and commented in main.cpp */
int batchRun(const vector<const char*> &names) {
	try {
	//	classify the types of the parameters
		vector<FileClassifier::FileType> types;
		transform( names.begin(), names.end(), back_inserter(types), FileClassifier() );
		
		int inpStart, confStart, outpStart, nextStart;
		int length= names.size();
		nextStart= 0;
		while (nextStart<length) { // process a block of files
			inpStart= nextStart;
			
			FileClassifier::FileType inpType= types[inpStart];
		//	find the end of input-file list
			for (	confStart= inpStart+1;
					confStart<length && types[confStart]==inpStart;
					++confStart ) /* no body */;
		//	find the end of config-file list
			for (	outpStart= confStart;
					outpStart<length && types[outpStart]==FileClassifier::Config;
					++outpStart ) /* no body */;
			nextStart= outpStart+1; //< exactly one output per block
			
			if (nextStart>length)
				throw tr("Missing output at the end of the parameter list");
					
			switch (inpType) {
			
			// decompression
			case FileClassifier::Fractal:
				if (confStart<outpStart)
					throw tr("No config file should be specified for decompression"
						" (parameter %1)") .arg(confStart+1);
				switch (types[outpStart]) {
				case FileClassifier::Bitmap: { // the output is a single specified file
					if (confStart-inpStart!=1) //< checking the input is single
						throw tr("A single output file (\"%1\")"
							" can only be used with single input") .arg(names[outpStart]);
					decodeFile(names[inpStart],names[outpStart]);					
					}
					break;
				case FileClassifier::Directory: // the output is a directory
					for (int inputID=inpStart; inputID<confStart; ++inputID) {
					//	test input's existence and permissions
						QFileInfo inputInfo(names[inputID]);
						if ( !inputInfo.isReadable() )
							throw tr("Can't open file \"%1\"") .arg(names[inputID]);
					//	construct output's name and decode the file
						QString outName= QString("%1%2%3.png") .arg(names[outpStart]) 
							.arg(QDir::separator()) .arg(inputInfo.completeBaseName());
						decodeFile( names[inputID], outName );
					}
					break;
				default: // the output is *.fci
					throw tr("The decompression output \"%1\" shouldn't be fractal image")
						.arg(names[outpStart]);
				} // switch (types[outpStart])
				break;
			
			// compression
			case FileClassifier::Bitmap:
				switch (types[outpStart]) {
				case FileClassifier::Fractal: { // the output is a single specified file
					if (confStart-inpStart!=1) //< checking the input is single
						throw tr("A single output file (\"%1\")"
							" can only be used with single input") .arg(names[outpStart]);
					encodeFile(names[inpStart],names[outpStart]);					
					}
					break;
				case FileClassifier::Directory: // the output is a directory					
					for (int inputID=inpStart; inputID<confStart; ++inputID) {
						QFileInfo inputInfo(names[inputID]);
						if ( !inputInfo.isReadable() )
							throw tr("Can't open file \"%1\"") .arg(names[inputID]);
						QString outNameStart= names[outpStart] 
							+ ( QDir::separator() + inputInfo.completeBaseName() );
							
						if (confStart==outpStart) // using default configuration
							encodeFile( names[inputID], outNameStart+".fci" ); 
						else {
							outNameStart+= "_%1.fci";
							for (int confID=confStart; confID<outpStart; ++confID) {
								QString cName= QFileInfo(QString(names[confID]))
									.completeBaseName();
								encodeFile( names[inputID], outNameStart.arg(cName)
									, names[confID] );
							}
						}
					}
					break;
				default:
					throw tr("The compression output \"%1\" should be"
						" either fractal image or a directory") .arg(names[outpStart]);
				} // switch (types[outpStart])
				break;
			
			default:
				throw tr("Invalid input file \"%1\"").arg(names[inpStart]);	
			} // switch (inpType)
			
		} // while - block-of-files processing
	} catch (QString &message) {
		cerr << message.toStdString() << endl;
		return 1;
	}
	return 0;
}