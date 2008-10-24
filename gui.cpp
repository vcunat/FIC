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
, readAct(this), writeAct(this), compareAct(this), exitAct(this)
, settingsAct(this), encodeAct(this), saveAct(this)
, loadAct(this), clearAct(this), iterateAct(this) {
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
	clearAct.setEnabled( mode != IRoot::Clear );
	iterateAct.setEnabled( mode != IRoot::Clear );
}

////	Private slots
void ImageViewer::read() {
//	get the file name
	QString fname= QFileDialog::getOpenFileName( this, tr("Read image file")
	, QDir::currentPath(), tr("PNG images (*.png)\nAll files (*.*)") );
	if (fname.isEmpty())
	//	no file selected
		return;
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
	QString fname= QFileDialog::getSaveFileName( this, tr("Write image file")
	, QDir::currentPath(), tr("PNG images (*.png)\nAll files (*.*)") );
	if (fname.isEmpty())
		return;
	if ( !imageLabel->pixmap()->save(fname) ) {
		QMessageBox::information( this, tr("Error"), tr("Cannot write file %1.").arg(fname) );
		return;
	}
	updateActions();
}
void ImageViewer::compare() {
//	let the user choose a file
	QString fname= QFileDialog::getOpenFileName
	( this, tr("Compare to image"), QDir::currentPath()
	, tr("PNG images (*.png)\nJFIF images (*.jpg *.jpeg)\nAll files (*.*)") );
	if (fname.isEmpty())
		return;
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
	IRoot *newSettings= clone(modules_settings);
	SettingsDialog dialog(this,newSettings);
	if ( dialog.exec() )
	//	the dialog wasn't cancelled -> swap with the current and new settings
		swap(newSettings,modules_settings);
	delete newSettings;
}
namespace NOSPACE {
	class EncThread: public QThread {
		IRoot *root;
		QImage image;
		bool success;
	public:
		EncThread(IRoot *root_,const QImage &image_)
		: root(root_), image(image_) {}
		virtual void run()
			{  success= root->encode(image); }
		bool getSuccess() const
			{ return success; }
	};
}
void ImageViewer::encode() {
//	start measuring the time
	QTime encTime;
    encTime.start();
//	create a new encoding tree
	IRoot *modules_old= modules_encoding;
	modules_encoding= clone(modules_settings);
//	create the dialog and the encoding thread
	EncThread encThread( modules_encoding, imageLabel->pixmap()->toImage() );
	{
		EncodingProgress encDialog(this);
	//	ensure the dialog is closed when the encoding finishes
		aConnect( &encThread, SIGNAL(finished()), &encDialog, SLOT(accept()) );
	//	start the thread and execute the dialog
		#ifdef NDEBUG
			encThread.start(QThread::LowPriority);
			encDialog.exec();
			encThread.wait();
			//0=0; // waiting for the thread, etc.
		#else
			encThread.run(); // no threading in debug mode
		#endif
	}
	if ( !encThread.getSuccess() )
	//	the encoding was interrupted - return to the backup
		swap(modules_encoding,modules_old);
	else { // encoding successful - iterate the image and display some info
		//#ifdef NDEBUG
			QImage beforeImg= modules_encoding->toImage();
			modules_encoding->decodeAct(MTypes::Clear);
			modules_encoding->decodeAct(MTypes::Iterate,AutoIterationCount);
			QImage afterImg= modules_encoding->toImage();
			changePixmap( QPixmap::fromImage(afterImg) );
			QString message= tr("Time to encode: %1 seconds\n") .arg(encTime.elapsed()/1000.0f)
			+ getPSNRmessage(beforeImg,afterImg);
			QMessageBox::information( this, tr("encoded"), message );
		//#endif
	}
//	delete the unneeded modules (either the backup or the unsuccessful)
	delete modules_old;
	updateActions();
}
void ImageViewer::save() {
	QString fname= QFileDialog::getSaveFileName
	( this, tr("Save encoded image"), QDir::currentPath(), tr("FIC images (*.fic)") );
	if (fname.isEmpty())
		return;
	if ( !modules_encoding->toFile( fname.toStdString().c_str() ) )
		QMessageBox::information( this, tr("Error"), tr("Cannot write file %1.").arg(fname) );
}
void ImageViewer::load() {
	QString fname= QFileDialog::getOpenFileName
	( this, tr("Load encoded image"), QDir::currentPath(), tr("FIC images (*.fic)") );
	if (fname.isEmpty())
		return;
//	IRoot needs to be loaded from cleared state
	IRoot *modules_old= modules_encoding;
	modules_encoding= clone(modules_settings,Module::ShallowCopy);

	if ( !modules_encoding->fromFile( fname.toStdString().c_str() ) ) {
		QMessageBox::information( this, tr("Error"), tr("Cannot load file %1.").arg(fname) );
		swap(modules_encoding,modules_old);
	} else { // loading was successful
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

////	SettingsDialog class

SettingsDialog::SettingsDialog( ImageViewer *parent, IRoot *settingsHolder )
: QDialog(parent,Qt::Dialog), settings(settingsHolder) {
	setWindowTitle(tr("Compression settings"));
	setModal(true);
//	create a grid layout for the dialog
	QGridLayout *layout= new QGridLayout;
	setLayout(layout);
//	add a tree widget
	treeWidget= new QTreeWidget(this);
	treeWidget->setHeaderLabel(tr("Modules"));
	layout->addWidget(treeWidget,0,0);
//	add a group-box
	setBox= new QGroupBox(this);
	layout->addWidget(setBox,0,1);
//	add a button-box and connect the clicking actions
	QDialogButtonBox *buttons= new QDialogButtonBox
	( QDialogButtonBox::Ok|QDialogButtonBox::Cancel, Qt::Horizontal, this );
	layout->addWidget(buttons,1,0,1,-1); //	span across the bottom
	aConnect( buttons, SIGNAL(accepted()), this, SLOT(accept()) );
	aConnect( buttons, SIGNAL(rejected()), this, SLOT(reject()) );
//	create the tree
	QTreeWidgetItem *treeRoot= new QTreeWidgetItem;
	treeRoot->setText( 0, tr("Root") );
	settings->adjustSettings(-1,treeRoot,setBox);
	treeWidget->addTopLevelItem(treeRoot);
	treeWidget->expandAll();

	aConnect( treeWidget, SIGNAL(currentItemChanged(QTreeWidgetItem*,QTreeWidgetItem*))
	, this, SLOT(currentItemChanged(QTreeWidgetItem*,QTreeWidgetItem*)) );
	treeWidget->setCurrentItem(treeRoot);
}
void SettingsDialog::currentItemChanged(QTreeWidgetItem *curItem,QTreeWidgetItem*) {
//	get the module that should show its settings
	assert(curItem);
	Module *curMod= static_cast<Module*>( curItem->data(0,Qt::UserRole).value<void*>() );
	assert(curMod);
//	clear the settings box and make the module fill it
	clearContainer( setBox->children() );
	setBox->setTitle( tr("%1 module settings") .arg(curMod->moduleName()) );
	curMod->adjustSettings(-1,0,setBox);
}
void SettingsDialog::settingChanges(int which) {
	QTreeWidgetItem *tree= treeWidget->currentItem();
	Module *module= static_cast<Module*>( tree->data(0,Qt::UserRole).value<void*>() );
	module->adjustSettings(which,tree,setBox);
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
			assert(false);
		}//	switch
		return result;
	}
}
void Module::adjustSettings(int which,QTreeWidgetItem *myTree,QGroupBox *setBox) {
	if (which<0) {
	//	no change has really happened
		assert(which==-1);
		if (myTree) {
		//	I should create the subtree -> store this-pointer as data
			assert( myTree->childCount() == 0 );
			myTree->setData( 0, Qt::UserRole, QVariant::fromValue((void*)this) );
			if (!settings) {
				myTree->setFlags(0);
				return;
			}
		//	find child modules and make them create their subtrees
			const SettingsTypeItem *setType= settingsType();
			const SettingsItem *setItem= settings;
			for (; setType->type!=Stop; ++setItem,++setType)
				if ( setType->type == ModuleCombo ) {
				//	it is a module -> label its subtree-root and recurse (polymorphically)
					assert(setItem->m);
					if ( !setItem->m->settingsLength() )
						continue;

					QTreeWidgetItem *childTree= new QTreeWidgetItem(myTree);
					childTree->setText( 0, QObject::tr(setType->label) );
					setItem->m->adjustSettings( -1, childTree, 0 );
				}
		}
		int setLength= settingsLength();
		if ( setBox && setLength ) {
		//	fill the group-box
			QGridLayout *layout= new QGridLayout(setBox);
			setBox->setLayout(layout);
			const SettingsTypeItem *typeItem= settingsType();
			for (int i=0; i<setLength; ++i,++typeItem) {
			//	add a line - one option
				QString desc(typeItem->desc);
				QLabel *label= new QLabel( typeItem->label, setBox );
				label->setToolTip(desc);

				QWidget *widget= newSettingsWidget( typeItem->type, setBox );
				settingsType2widget( widget, *typeItem );
				settings2widget( widget, i );
				widget->setToolTip(desc);
				label->setBuddy(widget);
				layout->addWidget( label, i, 0 );
				layout->addWidget( widget, i, 1 );

				SignalChanger *changer= new SignalChanger(i,widget,typeItem->type);
				aConnect( changer, SIGNAL(notify(int))			// assuming the setBox's
				, setBox->parent(), SLOT(settingChanges(int)) ); // parent is SettingsDialog
			}//	for loop
		}//	filling the box
	} else {
	//	which>=0... a setting has changed -> get the change from the widget
		assert( which<settingsLength() );
		QLayoutItem *item= setBox->layout()->itemAt(2*which+1);
		assert(item);
		widget2settings( item->widget() , which );
	//	handle module-type settings
		const SettingsTypeItem *setType= settingsType();
		if ( setType[which].type == ModuleCombo ) {
			assert(myTree);
		//	get the new module id and check whether it has really changed
			SettingsItem &setItem= settings[which];
			int newId= (*setType->data.compatIDs)[setItem.i];
			if ( newId == setItem.m->moduleId() )
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
	assert( 0<=which && which<settingsLength() );
	switch( settingsType()[which].type ) {
	case Int:
	case IntLog2:
		settings[which].i= debugCast<const QSpinBox*>(widget)->value();
		break;
	case Float:
		settings[which].f= debugCast<const QDoubleSpinBox*>(widget)->value();
		break;
	case ModuleCombo:
	case Combo:
		settings[which].i= debugCast<const QComboBox*>(widget)->currentIndex();
		break;
	default:
		assert(false);
	}//	switch
}
void Module::settings2widget(QWidget *widget,int which) {
	assert( 0<=which && which<settingsLength() );
	switch( settingsType()[which].type ) {
	case Int:
	case IntLog2:
		debugCast<QSpinBox*>(widget)->setValue( settings[which].i );
		break;
	case Float:
		debugCast<QDoubleSpinBox*>(widget)->setValue( settings[which].f );
		break;
	case ModuleCombo:
	case Combo:
		debugCast<QComboBox*>(widget)->setCurrentIndex( settings[which].i );
		break;
	default:
		assert(false);
	}//	switch
}
namespace NOSPACE {
	class ItemAdder {
		QComboBox *box;
	public:
		ItemAdder(QWidget *widget): box( debugCast<QComboBox*>(widget) ) {}
		void operator()(int i) {
			box->addItem( ModuleFactory::moduleName(i) );
			box->setItemData( box->count()-1
			, QObject::tr(ModuleFactory::moduleDescription(i)), Qt::ToolTipRole );
		}
	};
}
void Module::settingsType2widget(QWidget *widget,const SettingsTypeItem &typeItem) {
	assert( widget );
	switch( typeItem.type ) {
	case IntLog2:
		debugCast<QSpinBox*>(widget)->setPrefix(QObject::tr("2^"));
	//	fall through
	case Int:
		debugCast<QSpinBox*>(widget)->setRange( typeItem.data.i[0], typeItem.data.i[1] );
		break;
	case Float:
		debugCast<QDoubleSpinBox*>(widget)->setRange( typeItem.data.f[0], typeItem.data.f[1] );
		break;
	case ModuleCombo: {
		const vector<int> &modules= *typeItem.data.compatIDs;
		for_each( modules.begin(), modules.end(), ItemAdder(widget) );
		break;
		}
	case Combo:
		debugCast<QComboBox*>(widget)->addItems( QString(typeItem.data.text).split('\n') );
		break;
	default:
		assert(false);
	}//	switch
}


EncodingProgress *EncodingProgress::instance= 0;


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
