#include "interfaces.h"
#ifndef NDEBUG

#include "modules/root.h"
#include "modules/squarePixels.h"
#include "modules/stdEncoder.h"
#include "modules/quadTree.h"
#include "modules/stdDomains.h"
#include "modules/saupePredictor.h"

#include <QBoxLayout>
#include <QDialog>
#include <QLabel>
#include <QTabWidget>
#include <QTableWidget>

using namespace std;

int pos;

QWidget* MRoot::debugModule(QPixmap &pixmap,const QPoint &click) {
//	create a new modeless dialog window	
	QDialog *dlg= new QDialog;
	dlg->setModal(false);
//	add a laid-out single widget created by IShapeTransformer
	( new QVBoxLayout(dlg) )->addWidget( moduleShape()->debugModule(pixmap,click) );	
	return dlg;
}

namespace NOSPACE {
	struct PlaneFinder {
		typedef const IColorTransformer::PlaneSettings CPlSet;
		
		CPlSet* const plSet;
		
		PlaneFinder(CPlSet *plSet2find): plSet(plSet2find) {}
		bool operator()(const IColorTransformer::Plane &plane) 
			{ return plane.settings==plSet; }
	};
}
QWidget* MSquarePixels::debugModule(QPixmap &pixmap,const QPoint &click) {
	ASSERT( !planeList.empty() );
//	create a tab widget
	QTabWidget *tabs= new QTabWidget;
//	fill one tab for every job by three sub-tabs with the three modules
	for (int i=0; i<(int)jobs.size(); ++i) {
		PlaneBlock &job= jobs[i];
		if ( job.ranges->getRangeList().size() == 1 )
			continue;
	//	find the plane which is the job from
		PlaneList::const_iterator plane=
			find_if( planeList.begin(), planeList.end(), PlaneFinder(job.settings) );
		ASSERT( plane != planeList.end() );
	//	find out the position of the job in the plane
		int xShift=-1, yShift;
		plane->pixels.getPosition(job.pixels.start,xShift,yShift);
		ASSERT(xShift>=0);
		QPoint jobClick= click-QPoint(xShift,yShift);
		if ( jobClick.x()<0 || jobClick.y()<0 
		|| jobClick.x()>=job.width || jobClick.y()>=job.height )
			continue;
	//	make children do the work	
		QTabWidget *tabs2= new QTabWidget;
		tabs2->addTab( job.encoder->debugModule(pixmap,jobClick) , "Encoder" );
		tabs2->addTab( job.ranges->debugModule(pixmap,jobClick), "Ranges" );
		tabs2->addTab( job.domains->debugModule(pixmap,jobClick), "Domains" );
		
		tabs->addTab( tabs2, QString("Job %1").arg(i+1) );
	}
	return tabs;
}


namespace NOSPACE {
	struct RangeInfoAccumulator {
		const ISquareDomains::PoolList *pools;
		int rotCounts[9];
		vector<int> poolCounts, levelCounts;
		
		RangeInfoAccumulator(const ISquareDomains::PoolList &poolList)
		: pools( &poolList ), poolCounts( pools->size(), 0 ), levelCounts( 10, 0 ) {
			for (int i=0; i<9; ++i)
				rotCounts[i]= 0;
		}
		
		void operator()(const ISquareRanges::RangeNode *range) {
			const MStandardEncoder::RangeInfo &info
			= static_cast<MStandardEncoder::RangeInfo&>( *range->encoderData );
		//	increment the counts of ranges on this rotation and level
			++rotCounts[info.rotation+1];
			++levelCounts.at(range->level);
		//	find and increment the pool of the range's domain (if exists)
			if ( info.rotation>=0 ) {
				int index= info.decAccel.pool - &*pools->begin() ;
				ASSERT( index>=0 && index<(int)pools->size() );
				++poolCounts[index];
			}
		}
	}; // RangeAccumulator struct
	
	const ISquareRanges::RangeNode* findRangeOnPoint
	( const ISquareRanges::RangeList &ranges, const QPoint &click ) {
		int x= click.x(), y= click.y();
		ISquareRanges::RangeList::const_iterator it;
		for (it=ranges.begin(); it!=ranges.end(); ++it) {
			const Block &b= **it;
			if ( b.x0<=x && b.y0<=y && b.xend>x && b.yend>y )
				return *it;
		}
		ASSERT(false); return 0;
	}
}

namespace NOSPACE {
	static void addFramedImage(QLayout *layout,const QImage &image) {
		QLabel *label= new QLabel;
		label->setPixmap( QPixmap::fromImage(image) );
		label->setFrameStyle( QFrame::Plain | QFrame::Box ); // to have a thin frame
		label->setSizePolicy( QSizePolicy() ); // to have the frame exatly around the image
		layout->addWidget(label);
	}
	template<class Assigner>
	QImage imageFromMatrix(CSMatrix matrix,Block block,int rotation,Assigner assigner) {
		QImage image(block.width(),block.height(),QImage::Format_RGB32);
				
		using namespace MatrixWalkers;
	//	changing rotation because of transposed CheckedImage
		rotation= Rotation::compose(1,rotation);

		walkOperateCheckRotate( CheckedImage<QImage,QRgb>(image), assigner
		, matrix, block, rotation );
		
		return image;
	}
	struct GrayImageAssigner {
		void operator()(QRgb &rgb,const SReal &pixel) {
			int gray= Float2int<8,Real>::convertCheck(pixel);
			rgb= qRgb(gray,gray,255); // not gray colour (intentionally)
		}
		void innerEnd() const {}
	};
	struct GrayImageMulAddAssigner: public GrayImageAssigner {
		Real mul, add;
		
		GrayImageMulAddAssigner(Real toMul,Real toAdd)
		: mul(toMul), add(toAdd) {}
		void operator()(QRgb &rgb,const SReal &pixel) 
			{ GrayImageAssigner::operator()( rgb, pixel*mul+add ); }
	};
}
QWidget* MStandardEncoder::debugModule(QPixmap &pixmap,const QPoint &click) {
	const ISquareRanges::RangeList &ranges= planeBlock->ranges->getRangeList();
	
	QWidget *widget= new QWidget;
	QBoxLayout *layout= new QVBoxLayout(widget);
	
	if ( pixmap.rect().contains(click) ) { // info about range clicked on
		const RangeNode &range= *findRangeOnPoint( ranges, click );
		const RangeInfo &info= static_cast<RangeInfo&>(*range.encoderData);
		
		QString msg= QString("Quantized average: %1\nQuantized deviation: %2\n\n")
			.arg((double)info.qrAvg) .arg((double)sqrt(info.qrDev2));
		
		msg+= QString("Encoding SE: %1\n") .arg(info.bestSE);
		
		if (info.qrDev2) {
			int poolIndex= info.decAccel.pool - &*planeBlock->domains->getPools().begin();
			const Block &domBlock= info.decAccel.domBlock;
			msg+= QString("Domain pool: %1\nDomain block top-left corner coordinates:\n"
				"\t %2 %3\n") .arg(poolIndex) .arg(domBlock.x0) .arg(domBlock.y0);
			
			msg+= QString("Rotation: %1\nInversion: %2")
				.arg((int)info.rotation) .arg(info.inverted);
		} else
			msg+= "Only using solid color";
			
		layout->addWidget( new QLabel(msg) );
		
		if (info.qrDev2) {
			layout->addWidget( new QLabel("Domain block:") );
			addFramedImage( layout
			, imageFromMatrix( info.decAccel.pool->pixels, info.decAccel.domBlock
				, 0, GrayImageAssigner() )
			);
			
			layout->addWidget( new QLabel("Domain block, transformed (encode-mode-only):") );
			addFramedImage( layout
			, imageFromMatrix( info.decAccel.pool->pixels, info.decAccel.domBlock
				, info.rotation, GrayImageMulAddAssigner(info.exact.linCoeff,info.exact.constCoeff) )
			);
		}
		
		layout->addWidget( new QLabel("Range block:") );
		addFramedImage( layout
		, imageFromMatrix( planeBlock->pixels, range, 0, GrayImageAssigner() )
		);
		
	} else { // provide general info
		RangeInfoAccumulator info( planeBlock->domains->getPools() );
		info= for_each( ranges.begin(), ranges.end(), info );
		
		{// create a label with various counts info
			QString msg= "Range count: %1\nRotation counts: %2\nDomain pool counts: %3";
		//	fill in the total range count
			msg= msg.arg( ranges.size() );
		//	fill in the rotation counts
			QString rots= QString::number(info.rotCounts[0]);
			for (int i=1; i<9; ++i) 
				(rots+= ", ")+= QString::number(info.rotCounts[i]);		
			msg= msg.arg(rots);
		//	fill in the domain counts
			QString doms= QString::number(info.rotCounts[0]);
			vector<int>::const_iterator it;
			for (it= info.poolCounts.begin(); it!=info.poolCounts.end(); ++it)
				(doms+= ", ")+= QString::number(*it);
			msg= msg.arg(doms);
		//	create the label and add it to the layout
			layout->addWidget( new QLabel(msg) );
		}
		
		int maxLevel= 1 +log2ceil( max(planeBlock->width,planeBlock->height) );
		
		QTableWidget *table= new QTableWidget( maxLevel-2, 3 );
		table->setHorizontalHeaderLabels
		( QStringList() << "Level" << "Ranges" << "Domain count" );
		for (int level=2; level<maxLevel; ++level) {
		//	the number of the level
			table->setItem( level-2, 0, new QTableWidgetItem(QString::number(level)) );
		//	the count of the ranges on the level
			table->setItem( level-2, 1, new QTableWidgetItem(
				QString::number( info.levelCounts[level] )
			) );
		//	the maximum allowed SE for the level
			//float maxSE= planeBlock->moduleQ2SE->rangeSE( planeBlock->quality, powers[2*level] );
			//table->setItem( level-2, 1.5, new QTableWidgetItem(QString::number(maxSE)) );
		//	the domain count for the level
			int domCount= levelPoolInfos[level].empty() ? -1
				: levelPoolInfos[level].back().indexBegin;
			table->setItem( level-2, 2, new QTableWidgetItem(
				QString("2 ^ %1").arg( log2(domCount) )
			) );
		}
	//	resize the label and add it to the layout
		table->resizeColumnsToContents();
		table->resizeRowsToContents();
		layout->addWidget(table);
		
		if ( modulePredictor() )
			layout->addWidget( modulePredictor()->debugModule(pixmap,click) );
	} // if-then-else
	
	return widget;
} // MStandardEncoder::debugModule method


QWidget* MQuadTree::debugModule(QPixmap &pixmap,const QPoint &click) {
	
	if ( pixmap.rect().contains(click) ) { // info about range clicked on
		const ISquareRanges::RangeNode &range= *findRangeOnPoint( fringe, click );
		
		QString msg= QString("Level: %1\nRegular: %2\n"
			"Top-left corner: %3 %4\nWidth: %5\nHeight: %6")
			.arg(range.level) .arg(range.isRegular())
			.arg(range.x0) .arg(range.y0)
			.arg(range.width()) .arg(range.height());
		return new QLabel(msg);
		
	} else { // provide general info
		QString msg;
	//	add info about heuristic dividing (if it's allowed)	
		if ( settingsInt(HeuristicAllowed) ) {
			msg+= "Heuristic dividing info:\n"
					"\tBad tries (forced to divide): %1\n"
					"\tBad divides (successful merges): %2\n"
					"\tUnsuccessful merges: %3\n";
			msg= msg.arg(badTries).arg(badDivides).arg(triedMerges);
		}
		return new QLabel(msg);
	} // if-then-else
	
} // MQuadTree::debugModule method


QWidget* MStdDomains::debugModule(QPixmap &pixmap,const QPoint &click) {
	
	if ( pixmap.rect().contains(click) ) { // info about range clicked on
		
		
	} else { // provide general info
		
		
	} // if-then-else
	return 0;
}

QWidget* MSaupePredictor::debugModule(QPixmap &pixmap,const QPoint &click) {
	if ( pixmap.rect().contains(click) )
		return 0;
	return new QLabel( QString("Predicted %1/%2 (%3%)") 
		.arg(predicted) .arg(maxpred) .arg(double(100)*predicted/(double)maxpred) );
}

#endif
