#ifndef GUI_HEADER_
#define GUI_HEADER_

#include "headers.h"

#include <QAction>
#include <QApplication>
#include <QComboBox>
#include <QDialogButtonBox>
#include <QDoubleSpinBox>
#include <QFileDialog>
#include <QFileInfo>
#include <QGridLayout>
#include <QGroupBox>
#include <QLabel>
#include <QMainWindow>
#include <QMenu>
#include <QMenuBar>
#include <QMessageBox>
#include <QMouseEvent>
#include <QProgressDialog>
#include <QScrollArea>
#include <QScrollBar>
#include <QSpinBox>
#include <QStatusBar>
#include <QThread>
#include <QTime>
#include <QTimer>
#include <QTranslator>
#include <QTreeWidgetItem>

class ImageViewer;
class SettingsDialog;
class EncodingProgress;

/** Represents the main window of the program, providing a GUI */
class ImageViewer: public QMainWindow { Q_OBJECT
	friend class SettingsDialog;
	friend class EncodingProgress;
	
	static const int AutoIterationCount= 10;

	IRoot *modules_settings	///  Module tree holding current settings
	, *modules_encoding;	///< Module tree that's currently encoding or the last one
	int zoom;				///< The current zoom, see IRoot::fromStream
	std::string encData;	///< String containing (if nonempty) the last encoded/decoded data

	QTranslator translator;	///< The application's only translator
	QLabel *imageLabel; 	///< A pointer to the label showing images
	
	QDir lastPath;
/**	\name Actions
 *	@{ */
	QAction
        readAct, writeAct, compareAct, exitAct,
        settingsAct, encodeAct, saveAct,
        loadAct, clearAct, iterateAct, zoomIncAct, zoomDecAct;
///	@}
/**	\name Menus
 *	@{ */
	QMenu imageMenu, compMenu, decompMenu;//, langMenu, helpMenu;
///	@}
	void createActions();	///< Creates all actions and connects them to correct slots
    void createMenus();		///< Creates the menu bar and fills it with actions
    void translateUi();		///< Sets all texts in the application (uses current language)
    void updateActions();	///< Updates the availability of all actions
    void changePixmap(const QPixmap &pixmap) {
		imageLabel->setPixmap(pixmap);
		imageLabel->resize( pixmap.size() );
	}						///< Shows a new image
private slots:
/**	\name Methods performing the actions
	@{ */
	void read();
	void write();
	void compare();
	void settings();
	void encode();
	void save();
	void load();
	void clear();
	void iterate();
	void zoomInc();
	void zoomDec();
///	@}
	void encDone();
public:
	/** Initializes the object to default settings */
	ImageViewer(QApplication &app);
	/** Only releases the modules */
	virtual ~ImageViewer()
		{ delete modules_settings; delete modules_encoding; }
private:
	/** Reloads the image, iterates and shows it (returns true on success) */
	bool rezoom();
	/** Gets the path of the last used directory (from ::lastPath) */
	QString lastDir() 
		{ QDir dir(lastPath); return dir.cdUp() ? dir.path() : QDir::currentPath();	}
#ifndef NDEBUG
	void mousePressEvent(QMouseEvent *event);
#endif
};

/** A simple wrapper around QObject::connect that asserts successfulness of the connection */
inline void aConnect( const QObject *sender, const char *signal, const QObject *receiver
, const char *slot, Qt::ConnectionType type=Qt::AutoCompatConnection ) {
	DEBUG_ONLY(	bool result= )
		QObject::connect(sender,signal,receiver,slot,type);
	ASSERT(result);
}

/** Represents the encoding-settings dialog  */
class SettingsDialog: public QDialog { Q_OBJECT
	QGroupBox *setBox;					///< the settings group-box
	QTreeWidget *treeWidget;			///< the settings tree
	QDialogButtonBox *loadSaveButtons;	///< the button-box for loading and saving
	IRoot *settings;					///< the settings we edit
	
	/** Returns a reference to the parent instance of ImageViewer */
	ImageViewer& parentViewer() { 
		ImageViewer *result= debugCast<ImageViewer*>(parent()); 
		ASSERT(result); 
		return *result; 
	}
	/** Initializes the settings tree and group-box contents from #settings */
	void initialize();
private slots:
	/** Changes the contents of the settings group-box when selecting in the settings tree */
	void currentItemChanged(QTreeWidgetItem *current,QTreeWidgetItem *previous);
	/** Adjusts the #settings when changed in the settings group-box */
	void settingChanges(int which);
	/** Handles loading and saving of the settings */
	void loadSaveClick(QAbstractButton *button);
public:
	/** Initializes all the widgets in the dialog */
	SettingsDialog(ImageViewer *parent,IRoot *settingsHolder);
	/** Returns the edited #settings */
	IRoot* getSettings() { return settings; }
};

/** Converts signals from various types of widgets */
class SignalChanger: public QObject { Q_OBJECT
	int signal;
public:
	/** Configures to emit notify(\p whichSignal) when the state of widget \p parent changes
	 *	(it represents settings of type \p type) */
	SignalChanger( int whichSignal, QWidget *parent=0, Module::ChoiceType type=Module::Stop )
	: QObject(parent), signal(whichSignal) {
		if (!parent)
			return;
	//	connect widget's changing signal to my notifyXxx slot
		const char *signalString=0, *slotString=0;
		switch (type) {
			case Module::Int:
			case Module::IntLog2:
				signalString= SIGNAL(valueChanged(int));
				slotString= SLOT(notifyInt(int));
				break;
			case Module::Float:
				signalString= SIGNAL(valueChanged(double));
				slotString= SLOT(notifyDouble(double));
				break;
			case Module::ModuleCombo:
			case Module::Combo:
				signalString= SIGNAL(currentIndexChanged(int));
				slotString= SLOT(notifyInt(int));
				break;
			default:
				ASSERT(false);
		}//	switch
		aConnect( parent, signalString, this, slotString );
	}
public slots:
	void notifyInt(int)			{ emit notify(signal); }
	void notifyDouble(double)	{ emit notify(signal); }
signals:
	void notify(int);
}; // SignalChanger class



/** A dialog showing encoding progress */
class EncodingProgress: public QProgressDialog { Q_OBJECT
	/** Encoding-thread type */
	class EncThread: public QThread { //Q_OBJECT
		IRoot *root;
		QImage image;
		bool success;
		UpdateInfo updateInfo;
	public:
		EncThread(IRoot *root_,const QImage &image_,const UpdateInfo &updateInfo_)
		: root(root_), image(image_), updateInfo(updateInfo_) {}
		virtual void run()
			{ success= root->encode(image,updateInfo); }
		bool getSuccess() const
			{ return success; }
	}; // EncThread class
	
	static EncodingProgress *instance; ///< Pointer to the single instance of the class

	bool terminate	///  Value indicating whether the encoding should be interrupted
	, updateMaxProgress
	, updateProgress;
	int progress	///  The progress of the encoding - "the number of pixels encoded"
	, maxProgress;	///< The maximum value of progress
	
	IRoot *modules_encoding;///< The encoding modules
	UpdateInfo updateInfo;	///< UpdateInfo passed to encoding modules
	
	QTimer updateTimer;		///< Updates the dialog regularly
	QTime encTime;			///< Measures encoding time
	EncThread encThread;	///< The encoding thread

private:
	/** Sets the maximum progress (the value corresponding to 100%) */
	static void incMaxProgress(int increment) { 
		instance->maxProgress+= increment;
		instance->updateMaxProgress= true;
	}
	/** Increase the progress by a value */
	static void incProgress(int increment) { 
		instance->progress+= increment;
		instance->updateProgress= true;
	}
	///	\todo Should we provide a thread-safe version?
	///	q_atomic_fetch_and_add_acquire_int(&instance->progress,increment);
	///	instance->setValue((volatile int&)instance->progress);

private slots:
	/** Slot for catching cancel-pressed signal */
	void setTerminate()			
		{ terminate= true; }
	/** Updating slot - called regularly by a timer */
	void update() {
		if (updateMaxProgress) {
			setMaximum(maxProgress);
			updateMaxProgress= false;
		}
		if (updateProgress) {
			setValue(progress);
			updateProgress= false;
		}
	}
private:
	/** Creates and initializes the dialog */
	EncodingProgress(ImageViewer *parent)
	: QProgressDialog( tr("Encoding..."), tr("Cancel"), 0, 100, parent, Qt::Dialog )
	, terminate(false), updateMaxProgress(false), updateProgress(false)
	, progress(0), maxProgress(0)
	, modules_encoding(parent->modules_settings->clone())
	, updateInfo( terminate, &incMaxProgress, &incProgress ), updateTimer(this)
	, encThread( modules_encoding, parent->imageLabel->pixmap()->toImage(), updateInfo ) {
		ASSERT(!instance);
		instance= this;
	//	set some dialog features
		setWindowModality(Qt::ApplicationModal);
		setAutoClose(false);
		setAutoReset(false);
	//	to switch to terminating status when cancel is clicked
		aConnect( this, SIGNAL(canceled()), this, SLOT(setTerminate()) );
	//	start the updating timer
		aConnect( &updateTimer, SIGNAL(timeout()), this, SLOT(update()) );
     	updateTimer.start(1000);
		encTime.start(); //	start measuring the time
	//	start the encoding thread, set it to call ImageViewer::encDone when finished	
		aConnect( &encThread, SIGNAL(finished()), parent, SLOT(encDone()) );
		encThread.start(QThread::LowPriority);
	}
	/** Only zeroes #instance */
	~EncodingProgress() {
		ASSERT(this==instance);
		instance= 0;
	}
public:
	/** Creates the dialog and starts encoding */
	static void create(ImageViewer *parent) {
		new EncodingProgress(parent);
	}
	/** Collects results and destroys the dialog */
	static IRoot* destroy(int &elapsed) {
		ASSERT( instance && instance->encThread.isFinished() );
	//	get the encoding result if successful, delete it otherwise
		IRoot *result= instance->modules_encoding;
		if ( !instance->encThread.getSuccess() ) {
			delete result;
			result= 0;
		}
		elapsed= instance->encTime.elapsed();
	//	delete the dialog
		delete instance;
		return result;
	}
}; // EncodingProgress class

#endif // GUI_HEADER_
