#ifndef GUI_HEADER_
#define GUI_HEADER_

#include "interfaces.h"

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
#include <QTranslator>
#include <QTreeWidgetItem>

class ImageViewer;
class SettingsDialog;
class EncodingProgress;

/** Represents the main window of the program, providing a GUI */
class ImageViewer: public QMainWindow { Q_OBJECT
	friend class SettingsDialog;
	
	static const int AutoIterationCount= 10;

	IRoot *modules_settings		///  Module tree holding current settings
	, *modules_encoding;		///< Module tree that's currently encoding or the last one
	int zoom;					///< The current zoom, see IRoot::fromStream
	std::string encData;		///< String containing (if nonempty) the last encoded/decoded data

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
public:
	/** Initializes the object to default settings */
	ImageViewer(QApplication &app);
	/** Only releases the modules */
	virtual ~ImageViewer()
		{ delete modules_settings; delete modules_encoding; }
private:
	/** Reloads the image, iterates and shows it (returns true on success) */
	bool rezoom();
	/** Gets the path of the last used directory (from #lastPath) */
	QString lastDir() 
		{ QDir dir(lastPath); return dir.cdUp() ? dir.path() : QDir::currentPath();	}
#ifndef NDEBUG
	void mousePressEvent(QMouseEvent *event);
#endif
};

inline void aConnect( const QObject *sender, const char *signal, const QObject *receiver
, const char *slot, Qt::ConnectionType type=Qt::AutoCompatConnection ) {
	#ifndef NDEBUG
		bool result=
	#endif
	QObject::connect(sender,signal,receiver,slot,type);
	assert(result);
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
		assert(result); 
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


class SignalChanger: public QObject { Q_OBJECT
	int signal;
public:
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
				assert(false);
		}//	switch
		aConnect( parent, signalString, this, slotString );
	}
public slots:
	void notifyInt(int)			{ emit notify(signal); }
	void notifyDouble(double)	{ emit notify(signal); }
signals:
	void notify(int);
};

/** A dialog showing encoding progress */
class EncodingProgress: public QProgressDialog { Q_OBJECT
	static EncodingProgress *instance; ///< Pointer to the single instance of the class

	bool terminate;	///< Value indicating whether the encoding should be interrupted
	int progress;	///< The progress of the encoding - "the number of pixels encoded"

private:
	/** Sets the maximum progress (the value corresponding to 100%) */
	static void setMaxProgress(int maximum)
		{ instance->setMaximum(maximum); }
	/** Increase the progress by a value */
	static void incProgress(int increment)
		{ instance->setValue( instance->progress+= increment ); }
	//	TODO: Should we provide a thread-safe version?
	//	q_atomic_fetch_and_add_acquire_int(&instance->progress,increment);
	//	instance->setValue((volatile int&)instance->progress);

private slots:
	/** Slot for catching cancel-pressed signal */
	void setTerminate()
		{ terminate= true; }
public:
	/** Creates and initializes the dialog */
	EncodingProgress(QWidget *parent)
	: QProgressDialog( tr("Encoding..."), tr("Cancel"), 0, 100, parent, Qt::Dialog )
	, terminate(false), progress(0) {
		setWindowModality(Qt::ApplicationModal);
		setValue(progress);
		setAutoClose(false);
		setAutoReset(false);
		aConnect( this, SIGNAL(canceled()), this, SLOT(setTerminate()) );
		instance= this;
	}
	/** Returns a correct UpdatingInfo structure */
	UpdatingInfo getUpdatingInfo() const
		{ return UpdatingInfo( terminate, &setMaxProgress, &incProgress ); }
};

#endif // GUI_HEADER_
