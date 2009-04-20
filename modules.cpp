#include "interfaces.h"

#include "modules/root.h"
#include "modules/colorModel.h"
#include "modules/squarePixels.h"
#include "modules/quadTree.h"
#include "modules/stdDomains.h"
#include "modules/quality2SE.h"
#include "modules/stdEncoder.h"
#include "modules/vliCodec.h"
#include "modules/saupePredictor.h"
#include "modules/noPredictor.h"

#include "util.h"
#include "fileUtil.h"

#include "FerrisLoki/DataGenerators.h"

using namespace std;

typedef Loki::TL::MakeTypelist< MRoot, MColorModel, MSquarePixels, MQuadTree, MStdDomains
, MQuality2SE_std, MStandardEncoder, MDifferentialVLICodec, MSaupePredictor, NoPredictor >
::Result Modules;

const int powers[31]= { 1, 2, 4, 8, 16, 32, 64, 128, 256, 512, 1024, 2*1024			/* 2^11 */
    , 4*1024, 8*1024, 16*1024, 32*1024, 64*1024, 128*1024, 256*1024, 512*1024		/* 2^19 */
    , 1024*1024, 2*1024*1024, 4*1024*1024, 8*1024*1024, 16*1024*1024, 32*1024*1024	/* 2^25 */
    , 64*1024*1024, 128*1024*1024, 256*1024*1024, 512*1024*1024, 1024*1024*1024 };	/* 2^30 */

const bool UpdateInfo::noTerminate;

////	Compatible<TypeList,Iface> struct template - leaves in the TypeList only derivates
////	of Iface class parameter - used by Inteface<Iface>, hidden for others
namespace NOSPACE {
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
template<class Iface> const vector<int>& Interface<Iface>::getCompMods() {
	using namespace Loki::TL;
	typedef typename Compatible<Modules,Iface>::Result CompList;
//	if compMods_ vector is empty, create it and fill it
	if ( compMods_.empty() && Length<CompList>::value ) {
		compMods_.reserve(Length<CompList>::value);
		IterateTypes<CompList,ExtractId> gendata;
		gendata( back_inserter(compMods_) );
	}
	return compMods_;
}
template <class Iface> std::vector<int> Interface<Iface>::compMods_;


////	Module class members
const Module::SettingTypeItem Module::SettingTypeItem
::stopper= { 0, 0, {Stop,{i:-1},{text:0}} };

Module::SettingItem* Module::copySettings(CloneMethod method) const {
	ASSERT( method==DeepCopy || method==ShallowCopy );
//	copy the settings array
	int length= info().setLength;
	if (!length)
		return 0;
	SettingItem *result= new SettingItem[length];
	copy( settings, settings+length, result );

	SettingItem *item= result, *itemEnd= result+length;
	if (method==DeepCopy)
	//	make child modules copy themselves (deeply again)
		while (item!=itemEnd) {
			if (item->m)
				item->m= item->m->abstractClone(DeepCopy);
			++item;
		}
	else // ShallowCopy
	//	null module links
		while (item!=itemEnd) {
			item->m= 0;
			++item;
		}

	return result;
}
void Module::initDefaultModuleLinks() {
//	iterate over all settings
	const SettingTypeItem *setType= info().setType;
	for (int i=0; setType[i].type.type!=Stop; ++i )
		if (setType[i].type.type==ModuleCombo) {
		//	it is a module link -> initialize it with the right prototype
			ASSERT(!settings[i].m);
			settings[i].m= constCast(&ModuleFactory::prototype(
				(*setType[i].type.data.compatIDs)[settings[i].val.i]
			));
		}
}
void Module::nullModuleLinks() {
//	iterate over all settings
	SettingItem *itEnd= settings+info().setLength;
	for (SettingItem *it=settings; it!=itEnd; ++it)
		it->m= 0;
}
template<class M> M* Module::concreteClone(CloneMethod method) const {
	ASSERT( this && info().id == ModuleFactory::getModuleID<M>() );
//	create a new instance of the same type and fill its settings with a copy of mine
	M *result= new M;
	result->settings= copySettings(method);
	return result;
}
	
void Module::createDefaultSettings() {
	ASSERT(!settings);
	const TypeInfo &inf= info();
	settings= new SettingItem[inf.setLength];
	copy( inf.setType, inf.setType+inf.setLength, settings );
}

void Module::file_saveModuleType( ostream &os, int which ) {
//	do some assertions - we expect to have the child module, etc.
	ASSERT( which>=0 && which<info().setLength );
	SettingItem &setItem= settings[which];
	ASSERT( info().setType[which].type.type==ModuleCombo && setItem.m );
//	put the module's identifier
	put<Uchar>( os, setItem.m->info().id );
}
void Module::file_loadModuleType( istream &is, int which ) {
//	do some assertions - we expect not to have the child module, etc.
	ASSERT( which>=0 && which<info().setLength );
	const SettingTypeItem &setType= info().setType[which];
	SettingItem &setItem= settings[which];
	ASSERT( setType.type.type==ModuleCombo && !setItem.m );
//	get module identifier and check its existence
	int newId= get<Uchar>(is);
	checkThrow( 0<=newId && newId<Loki::TL::Length<Modules>::value );
//	check module compatibility
	const vector<int> &v= *setType.type.data.compatIDs;
	settings[which].val.i= find(v.begin(),v.end(),newId) - v.begin();
	checkThrow( settings[which].val.i < (int)v.size() );
//	create a new correct empty module
	setItem.m= ModuleFactory::newModule(newId,ShallowCopy);
}

void Module::saveAllSettings(std::ostream &stream) {
	int setLength= info().setLength;
	if (!setLength)
		return;
	else
		ASSERT( settings && setLength>0 );
	
	const SettingTypeItem *setType= info().setType;
	for (int i=0; i<setLength; ++i)
		switch(setType[i].type.type) {
		case Int:
		case IntLog2:
		case Combo:
			put<Uint32>( stream, settings[i].val.i );
			break;
		case Float:
			put<float>( stream, settings[i].val.f );
			break;
		case ModuleCombo:
			file_saveModuleType( stream, i );
			settings[i].m->saveAllSettings(stream);
			break;
		default:
			ASSERT(false);
		} // switch
}

void Module::loadAllSettings(std::istream &stream) {
	int setLength= info().setLength;
	ASSERT(setLength>=0);
	if (!setLength)
		return;
	if (!settings)
		settings= new SettingItem[setLength];
	
	const SettingTypeItem *setType= info().setType;
	for (int i=0; i<setLength; ++i)
		switch(setType[i].type.type) {
		case Int:
		case IntLog2:
		case Combo:
			settings[i].val.i= get<Uint32>(stream);
			break;
		case Float:
			settings[i].val.f= get<float>(stream);
			break;
		case ModuleCombo:
			file_loadModuleType( stream, i );
			settings[i].m->loadAllSettings(stream);
			break;
		default:
			ASSERT(false);
		} // switch
}

////	ModuleFactory class members
ModuleFactory* ModuleFactory::instance=0;

template<class M> int ModuleFactory::getModuleID()
	{ return Loki::TL::IndexOf<Modules,M>::value; }
	
template int ModuleFactory::getModuleID<MRoot>();

void ModuleFactory::initialize() {
//	create one instance of each module-type
	ASSERT( prototypes.empty() );
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
	const Module::TypeInfo &mi= module.info();
	Module::SettingItem *protSet= prototype(mi.id).settings;
//	replace prototype's settings
	ASSERT( protSet && module.settings );
	copy( module.settings, module.settings+mi.setLength, protSet );
//	convert module-links to links to the correct prototypes
	for (const Module::SettingTypeItem *setType= mi.setType
	; setType->type.type!=Module::Stop; ++setType,++protSet)
		if ( setType->type.type==Module::ModuleCombo ) {
			ASSERT(protSet->m);
			protSet->m= constCast(&prototype( protSet->m->info().id ));
		}
}


template<class T> int ModuleFactory::Instantiator<T>::operator()() const {
	int i= getModuleID<T>();
	Module *m= T::newCompatibleModule();
	m= m->concreteClone<T>(Module::DeepCopy);
	i+= T::getCompMods().size();
	return i;
}
void ModuleFactory::instantiateModules() {
	IterateTypes<Modules,Instantiator> gendata;
	vector<int> v;
	gendata(back_inserter(v));
}
