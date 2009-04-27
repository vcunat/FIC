#include "gui.h"
#include "modules/colorModel.h"	//	using color coefficients for RGB->gray conversion

using namespace std;

////	Non-member functions
static vector<double> getColorPSNR(const QImage &a,const QImage &b) {
	int width= a.width(), height= a.height();
	int x, y;
	QRgb *line1, *line2;
	int sum, sumR, sumG, sumB;
	sum= sumR= sumG= sumB= 0;
	for (y=0; y<height; ++y) {
		line1= (QRgb*)a.scanLine(y);
		line2= (QRgb*)b.scanLine(y);
		for (x=0; x<width; ++x) {
			sum+= sqr( getGray(line1[x]) - getGray(line2[x]) );
			sumR+= sqr( qRed(line1[x]) - qRed(line2[x]) );
			sumG+= sqr( qGreen(line1[x]) - qGreen(line2[x]) );
			sumB+= sqr( qBlue(line1[x]) - qBlue(line2[x]) );
		}
	}
	vector<double> result(4);
	result[0]= sumR;
	result[1]= sumG;
	result[2]= sumB;
	result[3]= sum;
	double mul= double(width*height) * double(sqr(255));
	for (vector<double>::iterator it=result.begin(); it!=result.end(); ++it)
		*it= 10.0 * log10(mul / *it);
	return result;
}
static QString getPSNRmessage(const QImage &img1,const QImage &img2) {
	vector<double> psnr= getColorPSNR(img1,img2);
	return QObject::tr("gray PSNR: %3 dB\nR,G,B: %4,%5,%6 dB") .arg(psnr[3],0,'f',2)
		.arg(psnr[0],0,'f',2) .arg(psnr[1],0,'f',2) .arg(psnr[2],0,'f',2);
}


////	Public members
ImageViewer::ImageViewer(QApplication &app)
: modules_settings( IRoot::newCompatibleModule() ), modules_encoding(0)
, zoom(0), lastPath(QDir::current().filePath("x"))
, readAct(this), writeAct(this), compareAct(this), exitAct(this)
, settingsAct(this), encodeAct(this), saveAct(this)
, loadAct(this), clearAct(this), iterateAct(this), zoomIncAct(this), zoomDecAct(this) {
//	try to load guessed language translation
	if ( translator.load( "lang-"+QLocale::system().name(), app.applicationDirPath() ) )
		app.installTranslator(&translator);
//	create a scrolling area with a label for viewing images
	imageLabel= new QLabel(this);
	imageLabel->setBackgroundRole(QPalette::Dark);
	imageLabel->setSizePolicy(QSizePolicy::Ignored,QSizePolicy::Ignored);

	QScrollArea *scrollArea= new QScrollArea(this);
	scrollArea->setBackgroundRole(QPalette::Dark);
	scrollArea->setWidget(imageLabel);
	setCentralWidget(scrollArea);

	setStatusBar(new QStatusBar(this));

	createActions();
	createMenus();
	translateUi();
	updateActions();

	resize(800,600);
}

////	Private methods
void ImageViewer::createActions() {
//	create all actions and connect them to the appropriate slots
	#define A(name) \
		aConnect( &name##Act, SIGNAL(triggered()), this, SLOT(name()) );
	#define AS(name,signal) \
		aConnect( &name##Act, SIGNAL(triggered()), this, SLOT(signal()) );
	A(read)
	A(write)
	A(compare)
	AS(exit,close)

	A(settings)
	A(encode)
	A(save)

	A(load)
	A(clear)
	A(iterate)
	A(zoomInc)
	A(zoomDec)

	#undef A
	#undef AS
}
void ImageViewer::createMenus() {
	imageMenu.addAction(&readAct);
	imageMenu.addAction(&writeAct);
	imageMenu.addSeparator();
	imageMenu.addAction(&compareAct);
	imageMenu.addSeparator();
	imageMenu.addAction(&exitAct);

	compMenu.addAction(&settingsAct);
	compMenu.addAction(&encodeAct);
	compMenu.addAction(&saveAct);

	decompMenu.addAction(&loadAct);
	decompMenu.addAction(&clearAct);
	decompMenu.addAction(&iterateAct);
	decompMenu.addSeparator();
	decompMenu.addAction(&zoomIncAct);
	decompMenu.addAction(&zoomDecAct);

	menuBar()->addMenu(&imageMenu);
	menuBar()->addMenu(&compMenu);
	menuBar()->addMenu(&decompMenu);
	//menuBar()->addMenu(langMenu);
	//menuBar()->addMenu(helpMenu);
}
void ImageViewer::translateUi() {
	setWindowTitle(tr("Fractal Image Compressor"));
//	set action names and shortcuts
	#define A(name,shortcut,text) \
		name##Act.setText(tr(text)); \
		name##Act.setShortcut(tr(shortcut));
	A(read,		"Ctrl+R",	"Read...")
	A(write,	"Ctrl+W",	"Write...")
	A(compare,	"",			"Compare to...")
	A(exit,		"Ctrl+Q",	"Quit")

	A(settings,	"",			"Settings")
	A(encode,	" ",		"Start encoding")
	A(save,		"Ctrl+S",	"Save FIC...")

	A(load,		"Ctrl+L", 	"Load FIC...")
	A(clear,	"Ctrl+C", 	"Clear image")
	A(iterate,	"Ctrl+I",	"Iterate image")
	A(zoomInc,	"Ctrl++",	"Increase zoom")
	A(zoomDec,	"Ctrl+-",	"Decrease zoom")
	#undef A

//	set the tiles of menu items
	#define M(name,title) \
		name##Menu.setTitle(tr(title));
	M(image,	"&Image");
	M(comp,		"&Compression");
	M(decomp,	"&Decompression");
	#undef M
}
void ImageViewer::updateActions() {
	bool pixmapOk= imageLabel->pixmap();
	IRoot::Mode mode= modules_encoding ? modules_encoding->getMode() : IRoot::Clear;

	//readAct.setEnabled(true);
	writeAct.setEnabled(pixmapOk);
	compareAct.setEnabled(pixmapOk);
	//exitAct.setEnabled(true);

	//settingsAct.setEnabled(true);
	encodeAct.setEnabled(pixmapOk);
	saveAct.setEnabled( mode != IRoot::Clear );

	//loadAct.setEnabled(true);
	clearAct.setEnabled  ( mode != IRoot::Clear );
	iterateAct.setEnabled( mode != IRoot::Clear );
	zoomIncAct.setEnabled( mode != IRoot::Clear && zoom<3 );
	zoomDecAct.setEnabled( mode != IRoot::Clear && zoom>0 );
}

////	Private slots
void ImageViewer::read() {
//	get the file name
	QString fname= QFileDialog::getOpenFileName( this, tr("Read image file")
	, lastDir(), tr("PNG images (*.png)\nAll files (*.*)") );
	if (fname.isEmpty())
	//	no file selected
		return;
	lastPath.setPath(fname);
//	try to load, check for errors
	QImage image(fname);
	if (image.isNull()) {
		QMessageBox::information( this, tr("Error"), tr("Cannot open %1.").arg(fname) );
		return;
	}
//	convert to 32-bits
	if (image.depth()<32)
		image= image.convertToFormat(QImage::Format_RGB32);
//  display it
	changePixmap(QPixmap::fromImage(image));
	updateActions();
}
void ImageViewer::write() {
//	get the file name
	QString fname= QFileDialog::getSaveFileName( this, tr("Write image file")
	, lastDir(), tr("PNG images (*.png)\nAll files (*.*)") );
	if (fname.isEmpty())
		return;
	lastPath.setPath(fname);
//	try to save the image
	if ( !imageLabel->pixmap()->save(fname) ) {
		QMessageBox::information( this, tr("Error"), tr("Cannot write file %1.").arg(fname) );
		return;
	}
	updateActions();
}
void ImageViewer::compare() {
//	let the user choose a file
	QString fname= QFileDialog::getOpenFileName
	( this, tr("Compare to image"), lastDir()
	, tr("PNG images (*.png)\nJFIF images (*.jpg *.jpeg)\nAll files (*.*)") );
	if (fname.isEmpty())
		return;
	lastPath.setPath(fname);
//	open the file as an image, check it's got the same dimensions as the diplayed one
	QImage image(fname);
	image= image.convertToFormat(QImage::Format_RGB32);
	if (image.isNull()) {
		QMessageBox::information( this, tr("Error"), tr("Cannot open %1.").arg(fname) );
		return;
	}
	if ( image.width()!=imageLabel->pixmap()->width()
	|| image.height()!=imageLabel->pixmap()->height() ) {
		QMessageBox::information
		( this, tr("Error"), tr("Images don't have the same dimensions.").arg(fname) );
		return;
	}
//	compute the PSNRs and display them
	QString message= getPSNRmessage( image, imageLabel->pixmap()->toImage() );
	QMessageBox::information( this, tr("Comparison"), message );
}
void ImageViewer::settings() {
	IRoot *newSettings= modules_settings->clone();
	SettingsDialog dialog(this,newSettings);
	newSettings= dialog.getSettings();
	if ( dialog.exec() )
	//	the dialog wasn't cancelled -> swap with the current and new settings
		swap(newSettings,modules_settings);
	delete newSettings;
}
void ImageViewer::encode() {
	EncodingProgress::create(this);
}
void ImageViewer::encDone() {
	int encMsecs;
	IRoot *modules_encoded= EncodingProgress::destroy(encMsecs);

	if (modules_encoded) { // encoding successful - iterate the image and display some info
	//	replace the old state
		delete modules_encoding;
		modules_encoding= modules_encoded;
	//	decode the image
		QImage beforeImg= modules_encoding->toImage();

		QTime decTime;
		decTime.start();
		modules_encoding->decodeAct(MTypes::Clear);
		modules_encoding->decodeAct(MTypes::Iterate,AutoIterationCount);
		int decMsecs= decTime.elapsed();

		QImage afterImg= modules_encoding->toImage();
		changePixmap( QPixmap::fromImage(afterImg) );
	//	show some info
		QString message= tr("Time to encode: %1 seconds\nTime to decode: %2 seconds\n")
			.arg(encMsecs/1000.0) .arg(decMsecs/1000.0)	+ getPSNRmessage(beforeImg,afterImg);
		QMessageBox::information( this, tr("encoded"), message );
		encData.clear();
	}

	updateActions();
}
void ImageViewer::save() {
//	get a filename to suggest
    QFileInfo finfo(lastPath.path());
    QString fname= finfo.dir().filePath( finfo.completeBaseName() + tr(".fic") );
	fname= QFileDialog::getSaveFileName
	( this, tr("Save encoded image"), fname, tr("FIC images (*.fic)") );
	if (fname.isEmpty())
		return;
	lastPath.setPath(fname);
	if ( !modules_encoding->toFile( fname.toStdString().c_str() ) )
		QMessageBox::information( this, tr("Error"), tr("Cannot write file %1.").arg(fname) );
}
void ImageViewer::load() {
	QString fname= QFileDialog::getOpenFileName
	( this, tr("Load encoded image"), lastDir(), tr("FIC images (*.fic)") );
	if (fname.isEmpty())
		return;
	lastPath.setPath(fname);
//	IRoot needs to be loaded from cleared state
	IRoot *modules_old= modules_encoding;
	modules_encoding= modules_settings->clone(Module::ShallowCopy);

	string decData;
	bool error= !file2string( fname.toStdString().c_str(), decData );
	if (!error) {
		stringstream stream(decData);
		error= !modules_encoding->fromStream( stream, zoom );
	}

	if (error) {
		QMessageBox::information( this, tr("Error"), tr("Cannot load file %1.").arg(fname) );
		swap(modules_encoding,modules_old);
	} else { // loading was successful
		swap(encData,decData);
		modules_encoding->decodeAct(Clear);
		modules_encoding->decodeAct(MTypes::Iterate,AutoIterationCount);
		changePixmap( QPixmap::fromImage(modules_encoding->toImage()) );
		updateActions();
	}

	delete modules_old;
}
void ImageViewer::clear() {
	modules_encoding->decodeAct(Clear);
	changePixmap( QPixmap::fromImage(modules_encoding->toImage()) );
	updateActions();
}
void ImageViewer::iterate() {
	modules_encoding->decodeAct(Iterate);
	changePixmap( QPixmap::fromImage(modules_encoding->toImage()) );
	updateActions();
}
void ImageViewer::zoomInc() {
	++zoom;
	if (!rezoom())
		--zoom, QMessageBox::information( this, tr("Error"), tr("Zooming failed.") );
}
void ImageViewer::zoomDec() {
	--zoom;
	ASSERT(zoom>=0);
	if (!rezoom())
		++zoom, QMessageBox::information( this, tr("Error"), tr("Zooming failed.") );
}

bool ImageViewer::rezoom() {
//	create a stream that contains the "saved image"
	stringstream stream;
	if ( encData.empty() ) {	// cache is empty - we have to create it (save the image)
		if ( !modules_encoding->toStream(stream) )
			return false;
		encData= stream.str();
	} else						// reusing the cache
		stream.str(encData);
//	reload the image from the stream
	IRoot *newRoot= modules_settings->clone(Module::ShallowCopy);
	if ( newRoot->fromStream(stream,zoom) ) {
		delete modules_encoding;
		modules_encoding= newRoot;
	} else {
		delete newRoot;
		return false;
	}
//	successfully reloaded -> auto-iterate the image and show it
	modules_encoding->decodeAct(MTypes::Clear);
	modules_encoding->decodeAct(MTypes::Iterate,AutoIterationCount);
	changePixmap( QPixmap::fromImage(modules_encoding->toImage()) );
	updateActions();
	return true;
}


////	SettingsDialog class

SettingsDialog::SettingsDialog( ImageViewer *parent, IRoot *settingsHolder )
: QDialog(parent,Qt::Dialog), settings(settingsHolder) {
	setWindowTitle(tr("Compression settings"));
	setModal(true);
//	create a grid layout for the dialog
	QGridLayout *layout= new QGridLayout;
	setLayout(layout);
//	add a tree widget
	this->treeWidget= new QTreeWidget(this);
	treeWidget->setHeaderLabel(tr("Modules"));
	layout->addWidget(treeWidget,0,0);
//	add a group-box to display settings of a module
	this->setBox= new QGroupBox(this);
	layout->addWidget(setBox,0,1);
//	add a load/save button-box and connect it to a slot of this dialog
	this->loadSaveButtons= new QDialogButtonBox
	( QDialogButtonBox::Open|QDialogButtonBox::Save, Qt::Horizontal, this );
	aConnect( loadSaveButtons, SIGNAL(clicked(QAbstractButton*))
	, this, SLOT(loadSaveClick(QAbstractButton*)) );
	layout->addWidget(loadSaveButtons,1,0,Qt::AlignLeft);
//	add a button-box and connect the clicking actions
	QDialogButtonBox *buttons= new QDialogButtonBox
	( QDialogButtonBox::Ok|QDialogButtonBox::Cancel, Qt::Horizontal, this );
	layout->addWidget(buttons,1,1); // the right-bottom cell
	aConnect( buttons, SIGNAL(accepted()), this, SLOT(accept()) );
	aConnect( buttons, SIGNAL(rejected()), this, SLOT(reject()) );
//	create the tree
	QTreeWidgetItem *treeRoot= new QTreeWidgetItem;
	treeWidget->addTopLevelItem(treeRoot);
	treeRoot->setText( 0, tr("Root") );

	aConnect( treeWidget, SIGNAL(currentItemChanged(QTreeWidgetItem*,QTreeWidgetItem*))
	, this, SLOT(currentItemChanged(QTreeWidgetItem*,QTreeWidgetItem*)) );

	initialize();
}
void SettingsDialog::initialize() {
	QTreeWidgetItem *treeRoot= treeWidget->topLevelItem(0);
	ASSERT( treeRoot && settings && setBox );
	clearContainer( treeRoot->takeChildren() );
	settings->adjustSettings(-1,treeRoot,setBox);
	treeWidget->setCurrentItem(treeRoot);
	treeWidget->expandAll();
}
void SettingsDialog::currentItemChanged(QTreeWidgetItem *curItem,QTreeWidgetItem*) {
//	get the module that should show its settings
	ASSERT(curItem);
	Module *curMod= static_cast<Module*>( curItem->data(0,Qt::UserRole).value<void*>() );
	ASSERT(curMod);
//	clear the settings box and make the module fill it
	clearContainer( setBox->children() );
	setBox->setTitle( tr("%1 module settings") .arg(curMod->info().name) );
	curMod->adjustSettings(-1,0,setBox);
}
void SettingsDialog::settingChanges(int which) {
	QTreeWidgetItem *tree= treeWidget->currentItem();
	Module *module= static_cast<Module*>( tree->data(0,Qt::UserRole).value<void*>() );
	module->adjustSettings(which,tree,setBox);
}
void SettingsDialog::loadSaveClick(QAbstractButton *button) {
	QDialogButtonBox::StandardButton test= loadSaveButtons->standardButton(button);
	switch ( test ) {
	case QDialogButtonBox::Open: { // open-button has been clicked
	//	ask for the name to load from
		QString fname= QFileDialog::getOpenFileName( this, tr("Load settings file")
		, parentViewer().lastDir(), tr("FIC settings (*.fms)") );
		if (fname.isEmpty())
			return;
	//	try to load the settings
		IRoot *newSettings= settings->clone(Module::ShallowCopy);
		if ( newSettings->allSettingsFromFile(fname.toStdString().c_str()) ) {
			delete settings;
			settings= newSettings;
			initialize();
		} else {
			QMessageBox::information( this, tr("Error")
			, tr("Cannot load settings from %1.").arg(fname) );
			delete newSettings;
		}
	}	break;
	case QDialogButtonBox::Save: { // save-button has been clicked
	//	ask for the name to save in
		QString fname= QFileDialog::getSaveFileName( this, tr("Save settings file")
		, parentViewer().lastDir(), tr("FIC settings (*.fms)") );
		if (fname.isEmpty())
			return;
	//	try to save the settings
		if ( !settings->allSettingsToFile(fname.toStdString().c_str()) )
			QMessageBox::information( this, tr("Error")
			, tr("Cannot save settings into %1.").arg(fname) );
	}	break;
	default:
		ASSERT(false);
	}
}

////	GUI-related Module members
namespace NOSPACE {
	QWidget* newSettingsWidget(Module::ChoiceType type,QWidget *parent) {
		QWidget *result=0;
		switch (type) {
		case Module::Int:
		case Module::IntLog2:
			result= new QSpinBox(parent);
			break;
		case Module::Float:
			result= new QDoubleSpinBox(parent);
			break;
		case Module::ModuleCombo:
		case Module::Combo:
			result= new QComboBox(parent);
			break;
		default:
			ASSERT(false);
		}//	switch
		return result;
	}
}
void Module::adjustSettings(int which,QTreeWidgetItem *myTree,QGroupBox *setBox) {
	if (which<0) {
	//	no change has really happened
		ASSERT(which==-1);
		if (myTree) {
		//	I should create the subtree -> store this-pointer as data
			ASSERT( myTree->childCount() == 0 );
			myTree->setData( 0, Qt::UserRole, QVariant::fromValue((void*)this) );
			if (!settings) {
				myTree->setFlags(0);
				return;
			}
		//	find child modules and make them create their subtrees
			const SettingTypeItem *setType= info().setType;
			const SettingItem *setItem= settings;
			for (; setType->type.type!=Stop; ++setItem,++setType)
				if ( setType->type.type == ModuleCombo ) {
				//	it is a module -> label its subtree-root and recurse (polymorphically)
					ASSERT(setItem->m);
					if ( !setItem->m->info().setLength )
						continue; // skipping modules with no settings
					QTreeWidgetItem *childTree= new QTreeWidgetItem(myTree);
					childTree->setText( 0, QObject::tr(setType->label) );
					setItem->m->adjustSettings( -1, childTree, 0 );
				}
		}
		int setLength= info().setLength;
		if ( setBox && setLength ) {
		//	fill the group-box
			QGridLayout *layout= new QGridLayout(setBox);
			setBox->setLayout(layout);
			const SettingTypeItem *typeItem= info().setType;
			for (int i=0; i<setLength; ++i,++typeItem) {
			//	add a line - one option
				QString desc(typeItem->desc);
				QLabel *label= new QLabel( typeItem->label, setBox );
				label->setToolTip(desc);

				QWidget *widget= newSettingsWidget( typeItem->type.type, setBox );
				settingsType2widget( widget, *typeItem );
				settings2widget( widget, i );
				widget->setToolTip(desc);
				label->setBuddy(widget);
				layout->addWidget( label, i, 0 );
				layout->addWidget( widget, i, 1 );

				SignalChanger *changer= new SignalChanger(i,widget,typeItem->type.type);
				aConnect( changer, SIGNAL(notify(int))			// assuming the setBox's
				, setBox->parent(), SLOT(settingChanges(int)) ); // parent is SettingsDialog
			}//	for loop
		}//	filling the box
	} else {
	//	which>=0... a setting has changed -> get the change from the widget
		ASSERT( which < info().setLength );
		QLayoutItem *item= setBox->layout()->itemAt(2*which+1);
		ASSERT(item);
		widget2settings( item->widget() , which );
	//	handle module-type settings
		const SettingTypeItem *setType= info().setType;
		if ( setType[which].type.type == ModuleCombo ) {
			ASSERT(myTree);
		//	get the new module id and check whether it has really changed
			SettingItem &setItem= settings[which];
			int newId= (*setType->type.data.compatIDs)[setItem.val.i];
			if ( newId == setItem.m->info().id )
				return;
		//	replace the child module
			clearContainer( myTree->takeChildren() );
			delete setItem.m;
			setItem.m= ModuleFactory::newModule(newId);
			adjustSettings( -1, myTree, 0 );
		}
	//	update module defaults
		ModuleFactory::changeDefaultSettings(*this);
	}
}
void Module::widget2settings(const QWidget *widget,int which) {
	ASSERT( 0<=which && which<info().setLength );
	switch( info().setType[which].type.type ) {
	case Int:
	case IntLog2:
		settings[which].val.i= debugCast<const QSpinBox*>(widget)->value();
		break;
	case Float:
		settings[which].val.f= debugCast<const QDoubleSpinBox*>(widget)->value();
		break;
	case ModuleCombo:
	case Combo:
		settings[which].val.i= debugCast<const QComboBox*>(widget)->currentIndex();
		break;
	default:
		ASSERT(false);
	}//	switch
}
void Module::settings2widget(QWidget *widget,int which) {
	ASSERT( 0<=which && which<info().setLength );
	switch( info().setType[which].type.type ) {
	case Int:
	case IntLog2:
		debugCast<QSpinBox*>(widget)->setValue( settings[which].val.i );
		break;
	case Float:
		debugCast<QDoubleSpinBox*>(widget)->setValue( settings[which].val.f );
		break;
	case ModuleCombo:
	case Combo:
		debugCast<QComboBox*>(widget)->setCurrentIndex( settings[which].val.i );
		break;
	default:
		ASSERT(false);
	}//	switch
}
namespace NOSPACE {
	class ItemAdder {
		QComboBox *box;
	public:
		ItemAdder(QWidget *widget): box( debugCast<QComboBox*>(widget) ) {}
		void operator()(int i) {
			const Module::TypeInfo &info= ModuleFactory::prototype(i).info();
			box->addItem(info.name);
			box->setItemData( box->count()-1, QObject::tr(info.desc), Qt::ToolTipRole );
		}
	};
}
void Module::settingsType2widget(QWidget *widget,const SettingTypeItem &typeItem) {
	ASSERT( widget );
	switch( typeItem.type.type ) {
	case IntLog2:
		debugCast<QSpinBox*>(widget)->setPrefix(QObject::tr("2^"));
	//	fall through
	case Int:
		debugCast<QSpinBox*>(widget)->
			setRange( typeItem.type.data.i[0], typeItem.type.data.i[1] );
		break;
	case Float:
		debugCast<QDoubleSpinBox*>(widget)->
			setRange( typeItem.type.data.f[0], typeItem.type.data.f[1] );
		break;
	case ModuleCombo: {
		const vector<int> &modules= *typeItem.type.data.compatIDs;
		for_each( modules.begin(), modules.end(), ItemAdder(widget) );
		break;
		}
	case Combo:
		debugCast<QComboBox*>(widget)->
			addItems( QString(typeItem.type.data.text).split('\n') );
		break;
	default:
		ASSERT(false);
	}//	switch
}


#ifndef NDEBUG
void ImageViewer::mousePressEvent(QMouseEvent *event) {
//	check the event, get the clicking point coordinates relative to the image
	if ( !event || !modules_encoding
	|| modules_encoding->getMode() == IRoot::Clear)
		return;
	const QPoint click= imageLabel->mapFrom( this, event->pos() );
//	get the image
	QPixmap pixmap= *imageLabel->pixmap();
	if ( click.isNull() )
		return;

	static QWidget *debugWidget= 0;
	delete debugWidget;
	debugWidget= modules_encoding->debugModule(pixmap,click);
	debugWidget->setParent(this);
	debugWidget->setWindowFlags(Qt::Dialog);
	debugWidget->show();

	changePixmap(pixmap);
}
#endif


EncodingProgress *EncodingProgress::instance= 0;
