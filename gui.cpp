#include "gui.h"
#include "modules/colorModel.h"	//	using color coefficients for RGB->gray conversion

using namespace std;

////	Non-member functions
static inline int getGray(QRgb color) {
	static const float *coeffs= MColorModel::YCbCrCoeffs[0];
	int res= int(
		0.5f +
		coeffs[0] * qRed(color) +
		coeffs[1] * qGreen(color) +
		coeffs[2] * qBlue(color)
	);
	checkByte(res);
	return res;
}
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
        *it= 10.0*log10( mul / *it );
    return result;
}

////	Public members
ImageViewer::ImageViewer(QApplication &app)
: modules_settings( IRoot::newCompatibleModule() ), modules_encoding(0)
, openAct(this), saveAct(this), compareAct(this), exitAct(this)
, settingsAct(this), encodeAct(this), clearAct(this), iterateAct(this) {
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
		connect(&name##Act,SIGNAL(triggered()),this,SLOT(name()));
	#define AS(name,signal) \
		connect(&name##Act,SIGNAL(triggered()),this,SLOT(signal()));
	A(open)
	A(save)
	A(compare)
	AS(exit,close)

	A(settings)
	A(encode)

	A(clear)
	A(iterate)

	#undef A
	#undef AS
}
void ImageViewer::createMenus() {
    imageMenu.addAction(&openAct);
    imageMenu.addAction(&saveAct);
    imageMenu.addSeparator();
    imageMenu.addAction(&compareAct);
    imageMenu.addSeparator();
    imageMenu.addAction(&exitAct);

    compMenu.addAction(&settingsAct);
    compMenu.addAction(&encodeAct);

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
	A(open,		"Ctrl+R",	"Read...")
	A(save,		"Ctrl+W",	"Write...")
	A(compare,	"",			"Compare to...")
	A(exit,		"Ctrl+Q",	"Quit")

	A(settings,	"",			"Settings")
	A(encode,	" ",		"Start encoding")

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

	//openAct.setEnabled(true);
    saveAct.setEnabled(pixmapOk);
    compareAct.setEnabled(pixmapOk);
    //exitAct.setEnabled(true);

    //settingsAct.setEnabled(true);
    encodeAct.setEnabled(pixmapOk);

    clearAct.setEnabled( mode != IRoot::Clear );
    iterateAct.setEnabled( mode != IRoot::Clear );
}

////	Private slots
void ImageViewer::open() {
//	get the file name
	QString fname= QFileDialog::getOpenFileName( this, tr("Open file")
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
void ImageViewer::save() {
	QString fname= QFileDialog::getSaveFileName( this, tr("Save file")
	, QDir::currentPath(), tr("PNG images (*.png)\nAll files (*.*)") );
    if (!fname.isEmpty()) {
        if (!imageLabel->pixmap()->save(fname)) {
            QMessageBox::information( this, tr("Error"), tr("Cannot save %1.").arg(fname) );
            return;
        }
        updateActions();
    }
}
void ImageViewer::compare() {
    QString fname= QFileDialog::getOpenFileName
    ( this, tr("Compare to image"), QDir::currentPath()
    , tr("PNG images (*.png)\nJFIF images (*.jpg *.jpeg)\nAll files (*.*)") );
    if (fname.isEmpty())
        return;
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
    vector<double> psnr= getColorPSNR( image, imageLabel->pixmap()->toImage() );
    QString message= tr("gray PSNR: %3 dB\nR,G,B: %4,%5,%6 dB") .arg(psnr[3],0,'f',2)
        .arg(psnr[0],0,'f',2) .arg(psnr[1],0,'f',2) .arg(psnr[2],0,'f',2);
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
namespace {
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
//	create a new encoding tree
	IRoot *modules_old= modules_encoding;
	modules_encoding= clone(modules_settings);
//	create the dialog and the encoding thread
	EncodingProgress encDialog(this);
	EncThread encThread( modules_encoding, imageLabel->pixmap()->toImage() );
//	ensure the dialog is closed when the encoding finishes
	connect( &encThread, SIGNAL(finished()), &encDialog, SLOT(accept()) );
//	start the thread and execute the dialog
	#ifdef NDEBUG
		encThread.start(QThread::LowPriority);
		encDialog.exec();
	#else
		encThread.run(); // no threading in debug mode
	#endif
	if ( !encThread.getSuccess() ) {
	//	the encoding was interrupted
		delete modules_encoding;
		modules_encoding= modules_old;
		return;
	}
//	the encoding was successful
	delete modules_old;
	updateActions();
}
void ImageViewer::clear() {
	modules_encoding->decodeAct(Clear);
	imageLabel->setPixmap( QPixmap::fromImage(modules_encoding->toImage()) );
}
void ImageViewer::iterate() {
	modules_encoding->decodeAct(Iterate);
	imageLabel->setPixmap( QPixmap::fromImage(modules_encoding->toImage()) );
}

////	SettingsDialog class

SettingsDialog::SettingsDialog(ImageViewer *parent,IRoot *settingsHolder)
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
	setBox= new QGroupBox(tr("Module settings"),this);
	layout->addWidget(setBox,0,1);
//	add a button-box and connect the clicking actions
	QDialogButtonBox *buttons= new QDialogButtonBox
	( QDialogButtonBox::Ok|QDialogButtonBox::Cancel, Qt::Horizontal, this );
	layout->addWidget(buttons,1,0,1,-1); //	span across the bottom
	connect( buttons, SIGNAL(accepted()), this, SLOT(accept()) );
	connect( buttons, SIGNAL(rejected()), this, SLOT(reject()) );
//	create the tree
	QTreeWidgetItem *treeRoot= new QTreeWidgetItem;
	treeRoot->setText( 0, tr("Root") );
	settings->adjustSettings(-1,treeRoot,setBox);
	treeWidget->addTopLevelItem(treeRoot);

	treeWidget->setCurrentItem(treeRoot);
	connect( treeWidget, SIGNAL(currentItemChanged(QTreeWidgetItem*,QTreeWidgetItem*))
	, this, SLOT(currentItemChanged(QTreeWidgetItem*,QTreeWidgetItem*)) );
}
void SettingsDialog::currentItemChanged(QTreeWidgetItem *curItem,QTreeWidgetItem*) {
//	get the module that should show its settings
	assert(curItem);
	Module *curMod= static_cast<Module*>( curItem->data(0,Qt::UserRole).value<void*>() );
	assert(curMod);
//	clear the settings box and make the module fill it
	clearContainer( setBox->children() );
	curMod->adjustSettings(-1,0,setBox);
}


EncodingProgress *EncodingProgress::instance= 0;

////	GUI-related Module members
namespace {
	QWidget* newWidget(Module::ChoiceType type,QWidget *parent) {
		switch (type) {
		case Module::Int:
		case Module::IntLog2:
			return new QSpinBox(parent);
		case Module::Float:
			return new QDoubleSpinBox(parent);
		case Module::ModuleCombo:
		case Module::Combo:
			return new QComboBox(parent);
		default:
			return assert(false),(QWidget*)0;
		}//	switch
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
			if (!settings)
				return;
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
				QWidget *widget= newWidget( typeItem->type, setBox );
				settingsType2widget( widget, *typeItem );
				settings2widget( widget, i );
				widget->setToolTip(desc);
				label->setBuddy(widget);
				layout->addWidget( label, i, 0 );
				layout->addWidget( widget, i, 1 );
			}//	for loop
		}//	filling the box
	} else {
	//	which>=0... a setting has changed -> get the change from the widget
		assert( which<settingsLength() );
		QLayoutItem *item= setBox->layout()->itemAt(2*which+1);
		assert(item);
		widget2settings( item->widget() , which );
	//	handle module-type settings
		const SettingsTypeItem *setType=settingsType();
		if ( setType[which].type == ModuleCombo ) {
		//	update the child-tree: get the right child-tree
			int childCount=0;
			for (int i=0; i<which; ++i) {
				if ( setType->type == ModuleCombo )
					++childCount;
				++setType;
			}
			QTreeWidgetItem *childTree= myTree->child(childCount);
			assert(childTree);
		//	get the new module id and check whether it has really changed
			SettingsItem &setItem= settings[which];
			int newId= (*setType->data.compatIDs)[setItem.i];
			if ( newId == setItem.m->moduleId() )
				return;
		//	replace the child module
			clearContainer( childTree->takeChildren() );
			delete setItem.m;
			(setItem.m= ModuleFactory::newModule(newId))
			->adjustSettings( -1, childTree, 0 );
		}
	//	update module defaults
		ModuleFactory::changeDefaultSettings(*this);
	}
}
void Module::widget2settings(const QWidget *widget,int which) {
	assert( which>=0 && settingsLength()<which );
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
	assert( which>=0 && which<settingsLength() );
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
namespace {
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
		debugCast<QDoubleSpinBox*>(widget)->setRange
		( typeItem.data.f[0], typeItem.data.f[1] );
		break;
	case ModuleCombo: {
		const vector<int> &modules=*typeItem.data.compatIDs;
		for_each( modules.begin(), modules.end(), ItemAdder(widget) );
		} break;
	case Combo:
		debugCast<QComboBox*>(widget)->addItems( QString(typeItem.data.text).split('\n') );
		break;
	default:
		assert(false);
	}//	switch
}
