#include "modules/root.h"
#include "modules/colorModel.h"
#include "modules/squarePixels.h"
#include "modules/quadTree.h"
#include "modules/stdDomains.h"
#include "modules/quality2SE.h"
#include "modules/stdEncoder.h"
#include "modules/vliCodec.h"
#include "modules/saupePredictor.h"

#include "util.h"
#include "fileUtil.h"

#include "FerrisLoki/DataGenerators.h"

using namespace std;

/** MODLIST macro definition */
#define MODLIST_1(T1) 		class T1;
#define MODLIST_2(T1,T...)	class T1; MODLIST_1(T)
#define MODLIST_3(T1,T...)	class T1; MODLIST_2(T)
#define MODLIST_4(T1,T...)	class T1; MODLIST_3(T)
#define MODLIST_5(T1,T...)	class T1; MODLIST_4(T)
#define MODLIST_6(T1,T...)	class T1; MODLIST_5(T)
#define MODLIST_7(T1,T...)	class T1; MODLIST_6(T)
#define MODLIST_8(T1,T...)	class T1; MODLIST_7(T)
#define MODLIST_9(T1,T...)	class T1; MODLIST_8(T)
#define MODLIST_10(T1,T...)	class T1; MODLIST_9(T)
#define MODLIST_11(T1,T...)	class T1; MODLIST_10(T)
#define MODLIST_12(T1,T...)	class T1; MODLIST_11(T)
#define MODLIST_13(T1,T...)	class T1; MODLIST_12(T)
#define MODLIST_14(T1,T...)	class T1; MODLIST_13(T)
#define MODLIST_15(T1,T...)	class T1; MODLIST_14(T)
#define MODLIST_16(T1,T...)	class T1; MODLIST_15(T)
#define MODLIST_17(T1,T...)	class T1; MODLIST_16(T)
#define MODLIST_18(T1,T...)	class T1; MODLIST_17(T)
#define MODLIST_19(T1,T...)	class T1; MODLIST_18(T)
#define MODLIST_20(T1,T...)	class T1; MODLIST_19(T)
#define MODLIST_21(T1,T...)	class T1; MODLIST_20(T)

#define MODLIST(count,args...) MODLIST_##count(args) \
	typedef LOKI_TYPELIST_##count(args)

MODLIST( 9, MRoot, MColorModel, MSquarePixels, MQuadTree, MStandardDomains
, MQuality2SE_std, MStandardEncoder, MDifferentialVLICodec, MSaupePredictor )
Modules;

const int powers[]={1,2,4,8,16,32,64,128,256,512,1024,2*1024,4*1024,8*1024,16*1024
    ,32*1024,64*1024,128*1024,256*1024,512*1024,1024*1024,2*1024*1024,4*1024*1024
    ,8*1024*1024,16*1024*1024,32*1024*1024,64*1024*1024,128*1024*1024,256*1024*1024};

////	Compatible<TypeList,Iface> struct template - leaves in the TypeList only derivates
////	of Iface class parameter - used by Inteface<Iface>, hidden for others
namespace {
	using namespace Loki;
	using namespace Loki::TL;

	template <class Typelist,class Interface>
	struct Compatible;

	template <class Iface>
	struct Compatible<NullType,Iface> {
		typedef NullType Result;
	};

	template <class Head,class Tail,class Iface>
	struct Compatible< Typelist<Head,Tail> , Iface > {
		typedef typename Select
			<SuperSubclass<Iface,Head>::value
			,Typelist< Head , typename Compatible<Tail,Iface>::Result >
			,typename Compatible<Tail,Iface>::Result
		>::Result Result;
	};

	template <class T> struct ExtractId {
		int operator()() const
			{ return Loki::TL::IndexOf<Modules,T>::value; }
	};
}
template<class Iface> const vector<int>&
Interface<Iface>::getCompMods() {
	using namespace Loki::TL;
	typedef typename Compatible<Modules,Iface>::Result CompList;
	if ( compMods_.empty() && Length<CompList>::value ) {
		compMods_.reserve(Length<CompList>::value);
		IterateTypes<CompList,ExtractId> gendata;
		gendata( back_inserter(compMods_) );
	}
	return compMods_;
}
template <class Iface> std::vector<int> Interface<Iface>::compMods_;


////	Module class members
const Module::SettingsTypeItem Module::SettingsTypeItem:: stopper= { Stop, {}, 0, 0 };

Module::SettingsItem* Module::copySettings(CloneMethod method) const {
//	copy the settings array
	int length= settingsLength();
	SettingsItem *result= new SettingsItem[length];
	copy( settings, settings+length, result );

	SettingsItem *item= result, *itemEnd= result+length;
	if (method==DeepCopy)
	//	make child modules copy themselves (deeply again)
		while (item!=itemEnd) {
			if (item->m)
				item->m= clone(item->m);
			++item;
		}
	else // ShallowCopy
	//	null module links
		while (item!=itemEnd)
			item->m= 0;

	return result;
}
void Module::initDefaultModuleLinks() {
//	iterate over all settings
	const SettingsTypeItem *setType=settingsType();
	for (int i=0; setType[i].type!=Stop; ++i )
		if (setType[i].type==ModuleCombo) {
		//	it is a module link -> initialize it with the right prototype
			assert(!settings[i].m);
			settings[i].m=constCast
			(&ModuleFactory::prototype( (*setType[i].data.compatIDs)[settings[i].i] ));
		}
}
void Module::nullModuleLinks() {
//	iterate over all settings
	SettingsItem *itEnd= settings+settingsLength();
	for (SettingsItem *it=settings; it!=itEnd; ++it)
		it->m= 0;
}
template<class M> M* Module::concreteClone(CloneMethod method) const {
	assert( this && moduleId() == ModuleFactory::getModuleID<M>() );
	M *result= new M;
	result->settings= copySettings(method);
	return result;
}

void Module::file_saveModuleType( ostream &os, int which ) {
//	do some assertions - we expect to have the child module, etc.
	assert( which>=0 && which<settingsLength() );
	SettingsItem &setItem=settings[which];
	assert( settingsType()[which].type==ModuleCombo && setItem.m );
//	put the module's identifier
	put<Uchar>( os, setItem.m->moduleId() );
}
void Module::file_loadModuleType( istream &is, int which ) {
//	do some assertions - we expect to have not the child module, etc.
	assert( which>=0 && which<settingsLength() );
	const SettingsTypeItem &setType=settingsType()[which];
	SettingsItem &setItem=settings[which];
	assert( setType.type==ModuleCombo && !setItem.m );
//	get module identifier and check it against the bounds
	int newId=get<Uchar>(is);
	if ( newId<0 || newId>=Loki::TL::Length<Modules>::value )
		throw exception();
//	check module compatibility
	const vector<int> &v=*setType.data.compatIDs;
	if ( find(v.begin(),v.end(),newId) == v.end() )
		throw exception();
//	create a new correct empty module
	setItem.m=ModuleFactory::newModule(newId,ShallowCopy);
}


////	ModuleFactory class members
ModuleFactory* ModuleFactory::instance=0;

template<class M> int ModuleFactory::getModuleID()
	{ return Loki::TL::IndexOf<Modules,M>::value; }

void ModuleFactory::initialize() {
//	create one instance of each module-type
	prototypes.reserve( Loki::TL::Length<Modules>::value );
	Loki::TL::IterateTypes<Modules,Creator> gendata;
	gendata( back_inserter(prototypes) );
//	initialize the prototypes' settings and interconnections
	for_each( prototypes.begin(), prototypes.end()
	, mem_fun(&Module::createDefaultSettings) );
	for_each( prototypes.begin(), prototypes.end()
	, mem_fun(&Module::initDefaultModuleLinks) );
}
void ModuleFactory::changeDefaultSettings(const Module &module) {
//	get the right prototype
	Module::SettingsItem *protSet= prototype(module.moduleId()).settings;
//	replace prototype's settings
	assert( protSet && module.settings );
	copy( module.settings, module.settings+module.settingsLength(), protSet );
//	convert module-links to links to the correct prototypes
	for (const Module::SettingsTypeItem *setType=module.settingsType()
	; setType->type!=Module::Stop; ++setType,++protSet)
		if ( setType->type==Module::ModuleCombo ) {
			assert(protSet->m);
			protSet->m= constCast(&prototype( protSet->m->moduleId() ));
		}
}

template<class T> int ModuleFactory::Instantiator<T>::operator()() const {
	T::newCompatibleModule();
	getModuleID<T>();
	((T*)(0))->T::abstractClone();
	return 0;
}
void ModuleFactory::instantiateModules() {
	IterateTypes<Modules,Instantiator> gendata;
	vector<int> v;
	gendata(back_inserter(v));
}
