#ifndef MODULES_HEADER_
#define MODULES_HEADER_

#include "util.h"

//	Forwards for all classes visible outside this header
class Module;
class ModuleFactory;
template <class Iface> class Interface;


/// \defgroup modules Module implementations
/** A common base class for all modules */
class Module {
	friend class ModuleFactory; ///< Permission for the ModuleFactory to manipulate Modules
//	Type definitions
public:
	/** Two types of cloning, used in ::clone */
	enum CloneMethod { ShallowCopy, DeepCopy };
	/** Types of a setting */
	enum ChoiceType {
		Stop,		///< a list-terminator
		Int,		///< integer from an interval
		IntLog2,	///< integer from an interval prefixed with 2^
		Float,		///< real number from an interval
		ModuleCombo,///< a connection to another module (shown as a combo-box)
		Combo		///  choice from a list of strings
	};
	/** Represents one setting - one number, real or integer */
	union SettingValue {
		int i;		///< Setting value for integer/module types (see Module::ChoiceType)
		float f;	///< Setting value for real-number type (see Module::ChoiceType)
	};
	/** Represents the type of one module's setting - without label and description */
	struct SettingType {
		ChoiceType type;		///< The type of the item
		SettingValue defaults;	///< The default setting

		union {
			int i[2];			///< Lower and upper bound (#type==Int,IntLog2)
			float f[2];			///< Lower and upper bound (#type==Float)
			const char *text;	///< Lines of the combo-box (#type==Combo)
			/** Pointer to the vector of IDs of compatible modules (#type==ModuleCombo),
			 *	meant to be one of Interface::getCompMods() */
			const std::vector<int> *compatIDs;
		} data;				///< Additional data, differs for different #type
	};

	/** Represents the type of one setting - including label, description, etc.\ */
	struct SettingTypeItem {
		static const SettingTypeItem stopper; ///< Predefined list terminator

		const char *label	///  The text label of the setting
		, *desc;			///< Description text
		SettingType type;	///< The type of this setting
	};

	/** Represents one setting value in a module */
	struct SettingItem {
		Module *m;	///< Pointer to the connected module (if type is Module::ModuleCombo)
		SettingValue val;	///< The setting value

		/** Just nulls the module pointer */
		SettingItem(): m(0) { DEBUG_ONLY( val.f= std::numeric_limits<float>::quiet_NaN(); ) }

		/** Creates a default settings-item for a settings-item-type */
		SettingItem(const SettingTypeItem &typeItem): m(0), val(typeItem.type.defaults) {}

		/** Just deletes the module pointer */
		~SettingItem() { delete m; }
	};

	/** Information about one module-type */
	struct TypeInfo {
		int id;							///< The module-type's ID
		const char *name				///  The module-type's name
		, *desc;						///< The module-type's description
		int setLength;					///< The number of setting items
		const SettingTypeItem *setType;	///< The types of module's settings
	};

//	Data definitions
protected:
	SettingItem *settings; ///< The current setting values of this Module

/** \name Construction, destruction and related methods
 *	@{	- all private, only to be used by ModuleFactory on module prototypes */
private:
	/**	Creates a copy of module's settings (includes the whole module subtree) */
	SettingItem* copySettings(CloneMethod method) const;
	/** Called for prototypes to initialize the default links to other modules */
	void initDefaultModuleLinks();
	/** Called for prototypes to null links to other modules */
	void nullModuleLinks();
	/** Called for prototypes to create the settings and inicialize them with defaults */
	void createDefaultSettings();

	/** Denial of assigment */
	Module& operator=(const Module&);
	/** Denial of copying */
	Module(const Module&);
///	@}
protected:
	/**	Initializes an empty module */
	Module(): settings(0) {}

	/** Cloning method, implemented in all modules via macros \return a new module
	 *	\param method determines creation of a deep or a shallow copy (zeroed children) */
	virtual Module* abstractClone(CloneMethod method) const =0;
	/** Concrete cloning method - templated by the actual type of the module */
	template<class M> M* concreteClone(CloneMethod method) const;
	
public:
	/** Deletes the settings, destroying child modules as well */
	virtual ~Module()
		{ delete[] settings; }

	/** Returns reference to module-type's information,
	 *	like count and types of settings, etc.\ Implemented in all modules via macros */
	virtual const TypeInfo& info() const =0;

	DECLARE_debugModule_empty

/**	\name Settings visualization and related methods
 *	@{	- common code for all modules, implemented in gui.cpp */
public:
	/** Creates or updates settings-box and/or settings-tree.\ 
	 *	if overridden in derived modules, it is recommended to call this one at first */
	virtual void adjustSettings( int which, QTreeWidgetItem *myTree, QGroupBox *setBox );
protected:
	/** Reads a setting value from a widget */
	void widget2settings( const QWidget *widget, int which );
	/** Writes a setting value into a widget */
	void settings2widget( QWidget *widget, int which );
	/** Initializes a widget according to a setting-item type */
	void settingsType2widget( QWidget *widget, const SettingTypeItem &typeItem );
///	@}

//	Other methods
protected:
	/** Saves all the settings, icluding child modules */
	void file_saveAllSettings(std::ostream &stream);
	/** Loads all the settings, icluding child modules (and their settings) */
	void file_loadAllSettings(std::istream &stream);
	/** Puts a module-identifier in a stream (\p which is the index in settings) */
	void file_saveModuleType( std::ostream &os, int which );
	/** Gets an module-identifier from the stream, initializes the pointer in settings
	 *	with a new empty instance and does some checking (bounds,Iface) */
	void file_loadModuleType( std::istream &is, int which );
	/** A shortcut method for working with integer settings */
	int& settingsInt(int index)
		{ return settings[index].val.i; }
	int settingsInt(int index) const
		{ return settings[index].val.i; }

/**	\name SettingType construction methods
 *	@{	- to be used within DECLARE_TypeInfo macros when declaring modules */
 	/** Creates a bounded integer setting, optionally shown as a power of two */
	static SettingType settingInt(int min,int defaults,int max,ChoiceType type=Int) {
		ASSERT( type==Int || type==IntLog2 );
		SettingType result;
		result.type= type;
		result.defaults.i= defaults;
		result.data.i[0]= min;
		result.data.i[1]= max;
		return result;
	}
	/** Creates a bounded real-number setting */
	static SettingType settingFloat(float min,float defaults,float max) {
		SettingType result;
		result.type= Float;
		result.defaults.f= defaults;
		result.data.f[0]= min;
		result.data.f[1]= max;
		return result;
	}
	/** Creates a choose-one-of setting shown as a combo-box */
	static SettingType settingCombo(const char *text,int defaults) {
		ASSERT( defaults>=0 && defaults<1+countEOLs(text) );
		SettingType result;
		result.type= Combo;
		result.defaults.i= defaults;
		result.data.text= text;
		return result;
	}
	/** Creates a connection to another module (type specified as a template parameter). 
	 *	It will be shown as a combo-box and automatically filled by compatible modules.
	 *	The default possibility can be specified */
	template<class Iface> static SettingType settingModule(int defaultID) {
		const std::vector<int> &compMods= Iface::getCompMods();
		int index=  find( compMods.begin(), compMods.end(), defaultID ) - compMods.begin();
		ASSERT( index < (int)compMods.size() );
		SettingType result;
		result.type= ModuleCombo;
		result.defaults.i= index;
		result.data.compatIDs= &compMods;
		return result;
	}
	/** A shortcut to set the first compatible module as the default one */
	template<class Iface> static SettingType settingModule() {
		return settingModule<Iface>( Iface::getCompMods().front() );
	}
///	@}
}; // Module class


////	Macros for easier Module-writing

#define DECLARE_TypeInfo_helper(CNAME_,NAME_,DESC_,SETTYPE_...) \
/** \cond */ \
	friend class Module; /* Needed for concreteClone */ \
	/*friend class ModuleFactory;*/ \
	friend struct ModuleFactory::Creator<CNAME_>; /* Needed for GCC-3 and ICC */ \
public: \
	CNAME_* abstractClone(CloneMethod method=DeepCopy) const \
		{ return concreteClone<CNAME_>(method); } \
 \
	const TypeInfo& info() const { \
		static SettingTypeItem setType_[]= {SETTYPE_}; \
		static TypeInfo info_= { \
			id: ModuleFactory::getModuleID<CNAME_>(), \
			name: NAME_, \
			desc: DESC_, \
			setLength: sizeof(setType_)/sizeof(*setType_)-1, \
			setType: setType_ \
		}; \
		return info_; \
	} \
/** \endcond */ \

/** Declares technical stuff within a Module descendant that contains no settings 
 *	- parameters: the name of the class (Token),
 *	the name of the module ("Name"), some description ("Desc...") */
#define DECLARE_TypeInfo_noSettings(CNAME_,NAME_,DESC_) \
	DECLARE_TypeInfo_helper(CNAME_,NAME_,DESC_,SettingTypeItem::stopper)

/** Like DECLARE_TypeInfo_noSettings, but for a module containing settings
 *	- additional parameter: an array of SettingTypeItem */
#define DECLARE_TypeInfo(CNAME_,NAME_,DESC_,SETTYPE_...) \
	DECLARE_TypeInfo_helper(CNAME_,NAME_,DESC_,SETTYPE_,SettingTypeItem::stopper)



/** A singleton factory class for creating modules */
class ModuleFactory {
	/** Static pointer to the only instance of the ModuleFactory */
	static ModuleFactory *instance;
	/** Pointers to the prototypes for every module type (indexed by their ID's) */
	std::vector<Module*> prototypes;

	/** Private constructor to avoid uncontrolled instantiation */
	ModuleFactory() {}
	/** Deletes the prototypes */
	~ModuleFactory() {
		for_each( prototypes.begin(), prototypes.end(), std::mem_fun(&Module::nullModuleLinks) );
		for_each( prototypes.begin(), prototypes.end(), SingleDeleter() );
	}
	/** The real constructing routine */
	void initialize();

public:
	/** Returns the ID of a module-type */
	template<class M> static int getModuleID();

	/** Method to instantiate the singleton */
	static void init()
		{ ASSERT(!instance); (instance=new ModuleFactory)->initialize(); }
	/** Method to delete the singleton */
	static void destroy()
		{ delete instance; instance=0; }

	/** Returns a reference to the module prototype with given id */
	static const Module& prototype(int id) {
		ASSERT( instance && instance->prototypes.at(id) );
		return *instance->prototypes[id];
	}
	/** Returns a new instance of the module-type with given id */
	static Module* newModule( int id, Module::CloneMethod method=Module::DeepCopy )
		{ return prototype(id).abstractClone(method); }

	/** Sets appropriate prototype settings according to given module's settings */
	static void changeDefaultSettings(const Module &module);

#ifndef __ICC
private:
#endif
//	some helper stuff
	template<class T> struct Creator {
		T* operator()() const { return new T; }
	};
private:
	template<class T> struct Instantiator {
		int operator()() const;
	};
	/** Never called, only exists for certain templates to be instantiated in modules.cpp */
	static void instantiateModules();
}; // ModuleFactory class


/**	A base class for all interfaces, parametrized by the interface's type */
template<class Iface> class Interface: public Module {
	static std::vector<int> compMods_; ///< IDs of modules derived from this interface
public:
	/** Returns a reference to #compMods_ (initializes it if empty) */
	static const std::vector<int>& getCompMods();
	/** Returns a reference to the index-th prototype implementing this interface */
	static const Iface& compatiblePrototype(int index=0) {
		ASSERT( index>=0 && index<(int)getCompMods().size() );
		return *debugCast<const Iface*>(&ModuleFactory::prototype( getCompMods()[index] ));
	}
	/** Creates a new instance of the index-th compatible module (deep copy of the prototype) */
	static Iface* newCompatibleModule(int index=0)
		{ return compatiblePrototype(index).clone(); }
		
	/** Works like abstractClone(), but is public and returns the correct type */
	Iface* clone(CloneMethod method=DeepCopy) const
		{ return debugCast<Iface*>( abstractClone(method) ); }
};

#endif // MODULES_HEADER_
