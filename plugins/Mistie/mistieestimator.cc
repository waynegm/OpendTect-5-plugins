#include "mistieestimator.h"

#include "geom2dintersections.h"
#include "survgeom.h"
#include "survgeom2d.h"
#include "seisread.h"
#include "seisioobjinfo.h"
#include "seistrc.h"
#include "seistype.h"
#include "bufstring.h"
#include "seisselectionimpl.h"
#include "ctxtioobj.h"
#include "ioman.h"
#include "seistrctr.h"
#include "iodir.h"
#include "survinfo.h"

#include "trcanalysis.h"

MistieEstimator::MistieEstimator(const IOObj* ioobj, Line2DInterSectionSet& intset, ZGate window, float maxshift, bool allEst)
    : window_(window)
    , maxshift_(maxshift)
    , ioobj_(ioobj)
    , allest_(allEst)
{
    BufferString lineA, lineB;
    int trcA, trcB;
    float spnr;
    Coord pos;
    for (int idx=0; idx<intset.size(); idx++) {
        Pos::GeomID geomidA = intset[idx]->geomID();
        mDynamicCastGet( const Survey::Geometry2D*, geom2d, Survey::GM().getGeometry(geomidA) );
        lineA = Survey::GM().getName(geomidA);
        for (int idp=0; idp<intset[idx]->size(); idp++) {
            Line2DInterSection::Point pint = intset[idx]->getPoint(idp);
            lineB = Survey::GM().getName(pint.line);
            trcA = pint.mytrcnr;
            trcB = pint.linetrcnr;
            geom2d->getPosByTrcNr(trcA, pos, spnr);
            misties_.add(lineA, trcA, lineB, trcB, pos);
        }
    }
}

MistieEstimator::~MistieEstimator()
{}

od_int64 MistieEstimator::nrIterations() const
{
    return misties_.size();
}

uiString MistieEstimator::uiMessage() const
{
    return tr("Estimating misties");
}

uiString MistieEstimator::uiNrDoneText() const
{
    return tr("Intersections done");
}

bool MistieEstimator::doWork( od_int64 start, od_int64 stop, int threadid )
{
    BufferString lineA, lineB;
    int trcnrA, trcnrB;
    SeisTrc trcA, trcB;
    
    for (int idx=mCast(int,start); idx<=stop && shouldContinue(); idx++, addToNrDone(1)) {
        float zdiff = 0.0;
        float phasediff = 0.0;
        float ampdiff = 1.0;
        float quality = 0.0;
        
        if (!misties_.get(idx, lineA, trcnrA, lineB, trcnrB)) {
            BufferString tmp("MistieEstimator::doWork - could not get intersection details at index: ");
            tmp += idx;
            ErrMsg(tmp);
            continue;
        }
        if (get2DTrc(lineA, trcnrA, trcA) && get2DTrc(lineB, trcnrB, trcB)) {
            if (trcA.info().sampling.step > trcB.info().sampling.step) {
                trcA.info().pick = 0.0;
                SeisTrc* tmp = trcA.getRelTrc( window_, trcB.info().sampling.step );
                trcA = *tmp;
            } else if (trcA.info().sampling.step < trcB.info().sampling.step) {
                trcB.info().pick = 0.0;
                SeisTrc* tmp = trcB.getRelTrc( window_, trcA.info().sampling.step );
                trcB = *tmp;
            }
            if (allest_)
		computeMistie( trcA, trcB, maxshift_, zdiff, phasediff, ampdiff, quality );
	    else
		computeMistie( trcA, trcB, maxshift_, zdiff, quality );

        } else {
            BufferString tmp("MistieEstimator::doWork - could not get trace data for: ");
            tmp += lineA; tmp+= " "; tmp += lineB;
            ErrMsg(tmp);
        }
        misties_.set(idx, zdiff, phasediff, ampdiff, quality);
    }
    return true;
}

bool MistieEstimator::get2DTrc( BufferString line, int trcnr, SeisTrc& trc )
{
    Pos::GeomID geomid = Survey::GM().getGeomID(line);
    
    Seis::RangeSelData range;
    range.setZRange(window_);
    range.cubeSampling().hsamp_.setInlRange(Interval<int>(0,0));
    range.cubeSampling().hsamp_.setCrlRange(Interval<int>(trcnr, trcnr));
    range.setGeomID(geomid);
    
    SeisTrcReader rdr(ioobj_);
    rdr.setSelData(range.clone());
    rdr.prepareWork();
    rdr.get(trc);

    return (!trc.isNull());
}

bool MistieEstimator::doFinish( bool success )
{
    return true;
}

