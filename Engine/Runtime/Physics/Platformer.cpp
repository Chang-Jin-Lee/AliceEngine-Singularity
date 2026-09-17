// SPDX-License-Identifier: MIT
// Static terrain permits a compact immutable BVH and allocation-free motion queries.
#include "Runtime/Physics/Platformer.h"
#include "Foundation/Profiler.h"
#include <algorithm>
#include <cmath>
#include <limits>
#include <numeric>

namespace alice::runtime::physics {
namespace {
constexpr double kContact = 1e-8;
constexpr double kLimit = 1e6;
bool ValidNumber(double value) { return std::isfinite(value) && std::abs(value) <= kLimit; }
bool Valid(const Box& box) {
    return ValidNumber(box.center.x) && ValidNumber(box.center.y) &&
           ValidNumber(box.half.x) && ValidNumber(box.half.y) &&
           box.half.x >= 1e-6 && box.half.y >= 1e-6;
}
Diagnostic Error(const char* code, const char* message, const char* hint, const SourceLocation& source) {
    auto error = MakeError(code,message,hint);
    error.file=source.file.empty() ? "<physics>" : source.file;
    error.path=source.path.empty() ? "bounds" : source.path;
    error.mark=source.mark.Valid() ? source.mark : Mark{1,1,0}; return error;
}
Box Union(const Box& a,const Box& b) {
    const double loX=std::min(a.center.x-a.half.x,b.center.x-b.half.x);
    const double loY=std::min(a.center.y-a.half.y,b.center.y-b.half.y);
    const double hiX=std::max(a.center.x+a.half.x,b.center.x+b.half.x);
    const double hiY=std::max(a.center.y+a.half.y,b.center.y+b.half.y);
    return {{(loX+hiX)*0.5,(loY+hiY)*0.5},{(hiX-loX)*0.5,(hiY-loY)*0.5}};
}
bool Intersects(const Box& a,const Box& b) {
    return std::abs(a.center.x-b.center.x)<=a.half.x+b.half.x+kContact &&
           std::abs(a.center.y-b.center.y)<=a.half.y+b.half.y+kContact;
}
struct Hit { double time=2; bool x=false,y=false; double snapX=0,snapY=0; };
Hit Sweep(const Box& a,Vec2 d,const Box& b) {
    const double hx=a.half.x+b.half.x,hy=a.half.y+b.half.y;
    double px=a.center.x-b.center.x,py=a.center.y-b.center.y;
    if(std::abs(std::abs(px)-hx)<=kContact) px=std::copysign(hx,px);
    if(std::abs(std::abs(py)-hy)<=kContact) py=std::copysign(hy,py);
    double nx=-std::numeric_limits<double>::infinity(),ny=nx;
    double fx=std::numeric_limits<double>::infinity(),fy=fx;
    if(d.x==0) { if(std::abs(px)>=hx) return {}; }
    else { nx=(-hx-px)/d.x; fx=(hx-px)/d.x; if(nx>fx) std::swap(nx,fx); }
    if(d.y==0) { if(std::abs(py)>=hy) return {}; }
    else { ny=(-hy-py)/d.y; fy=(hy-py)/d.y; if(ny>fy) std::swap(ny,fy); }
    const double enter=std::max(nx,ny),leave=std::min(fx,fy);
    // Equal entry/exit only grazes a corner; there is no interval inside the obstacle.
    if(enter<0 || enter>1 || enter>=leave || leave<0) return {};
    Hit hit; hit.time=enter; hit.x=nx>=ny; hit.y=ny>=nx;
    hit.snapX=b.center.x+(d.x>0?-hx:hx);
    hit.snapY=b.center.y+(d.y>0?-hy:hy);
    return hit;
}
}
Status CheckBox(const Box& box, const SourceLocation& source) {
    if (!Valid(box))
        return Error("physics.bounds.invalid","invalid static collision bounds",
                     "use finite centers within +/-1e6 and half sizes from 1e-6 to 1e6",source);
    return Status::Ok();
}
Status StaticWorld::Build(std::span<const Box> terrain,const SourceLocation& source) {
    for(const Box& box:terrain) { ALICE_TRY(CheckBox(box,source)); }
    StaticWorld next(m_indexed);
    next.m_boxes.assign(terrain.begin(),terrain.end());
    next.m_order.resize(terrain.size());
    std::iota(next.m_order.begin(),next.m_order.end(),usize{0});
    next.m_nodes.reserve(terrain.size()*2);
    if(!terrain.empty() && m_indexed) next.BuildNode(0,terrain.size());
    m_boxes.swap(next.m_boxes); m_order.swap(next.m_order); m_nodes.swap(next.m_nodes);
    return {};
}
usize StaticWorld::BuildNode(usize begin,usize end) {
    Box bounds=m_boxes[m_order[begin]];
    for(usize i=begin+1;i<end;++i) bounds=Union(bounds,m_boxes[m_order[i]]);
    const usize index=m_nodes.size();
    m_nodes.push_back({bounds,begin,end,0,0});
    if(end-begin>4) {
        const bool x=bounds.half.x>=bounds.half.y;
        const usize middle=begin+(end-begin)/2;
        std::nth_element(m_order.begin()+static_cast<std::ptrdiff_t>(begin),
                         m_order.begin()+static_cast<std::ptrdiff_t>(middle),
                         m_order.begin()+static_cast<std::ptrdiff_t>(end),[&](usize a,usize b) {
            const double av=x?m_boxes[a].center.x:m_boxes[a].center.y;
            const double bv=x?m_boxes[b].center.x:m_boxes[b].center.y;
            return av==bv?a<b:av<bv;
        });
        const usize left=BuildNode(begin,middle),right=BuildNode(middle,end);
        m_nodes[index].left=left; m_nodes[index].right=right;
    }
    return index;
}
void StaticWorld::Query(usize index,const Box& bounds,void (*visit)(usize,void*),void* context) const {
    const Node& node=m_nodes[index];
    if(!Intersects(node.bounds,bounds)) return;
    if(node.left!=0) { Query(node.left,bounds,visit,context); Query(node.right,bounds,visit,context); }
    else for(usize i=node.begin;i<node.end;++i)
        if(Intersects(m_boxes[m_order[i]],bounds)) visit(m_order[i],context);
}
void StaticWorld::Visit(const Box& bounds,void (*visit)(usize,void*),void* context) const {
    ALICE_PROFILE_ZONE("Physics.Query");
    if(m_indexed) { if(!m_nodes.empty()) Query(0,bounds,visit,context); }
    else for(usize i=0;i<m_boxes.size();++i) visit(i,context);
}
Result<Motion> StaticWorld::Move(const Box& box,Vec2 displacement,const SourceLocation& source) const {
    ALICE_PROFILE_ZONE("Physics.Move");
    if(!Valid(box) || !ValidNumber(displacement.x) || !ValidNumber(displacement.y) ||
       !ValidNumber(box.center.x+displacement.x) || !ValidNumber(box.center.y+displacement.y))
        return Error("physics.motion.invalid","invalid character bounds or displacement",
                     "use finite centers/displacements within +/-1e6 and half sizes from 1e-6 to 1e6",source);
    Motion result{box};
    // Resolve authored overlaps before sweeping; bounded failure rejects trapped spawns.
    for(int iteration=0;iteration<=16;++iteration) {
        struct Context { const std::vector<Box>* boxes; Motion* motion; double distance=kLimit*8;
                         Vec2 correction; usize id=std::numeric_limits<usize>::max(); } context{&m_boxes,&result};
        Visit(result.box,[](usize id,void* opaque) {
            auto& c=*static_cast<Context*>(opaque); ++c.motion->candidates; ++c.motion->tests;
            const Box& a=c.motion->box; const Box& b=(*c.boxes)[id];
            const double dx=a.center.x-b.center.x,dy=a.center.y-b.center.y;
            const double ox=a.half.x+b.half.x-std::abs(dx),oy=a.half.y+b.half.y-std::abs(dy);
            if(ox<=kContact || oy<=kContact) return;
            const bool useY=oy<=ox; const double distance=useY?oy:ox;
            if(distance<c.distance || (distance==c.distance && id<c.id)) {
                c.distance=distance; c.id=id;
                c.correction=useY?Vec2{0,dy>=0?oy:-oy}:Vec2{dx>=0?ox:-ox,0};
            }
        },&context);
        if(context.id==std::numeric_limits<usize>::max()) break;
        if(iteration==16) return Error("physics.overlap.unresolved","character remains inside static terrain",
                                     "move the spawn into free space or separate overlapping terrain",source);
        result.box.center.x+=context.correction.x; result.box.center.y+=context.correction.y;
    }
    Vec2 remaining=displacement;
    for(int iteration=0;iteration<4 && (remaining.x!=0 || remaining.y!=0);++iteration) {
        struct Context { const std::vector<Box>* boxes; Motion* motion; Vec2 delta; Hit hit;
                         usize xId=std::numeric_limits<usize>::max(),yId=std::numeric_limits<usize>::max();
        } context{&m_boxes,&result,remaining,{}};
        Box end=result.box; end.center.x+=remaining.x; end.center.y+=remaining.y;
        Visit(Union(result.box,end),[](usize id,void* opaque) {
            auto& c=*static_cast<Context*>(opaque); ++c.motion->candidates; ++c.motion->tests;
            const Hit hit=Sweep(c.motion->box,c.delta,(*c.boxes)[id]);
            if(hit.time>1) return;
            if(hit.time<c.hit.time) { c.hit=hit; c.xId=hit.x?id:std::numeric_limits<usize>::max();
                                    c.yId=hit.y?id:std::numeric_limits<usize>::max(); }
            else if(hit.time==c.hit.time) {
                if(hit.x && id<c.xId) { c.hit.x=true; c.hit.snapX=hit.snapX; c.xId=id; }
                if(hit.y && id<c.yId) { c.hit.y=true; c.hit.snapY=hit.snapY; c.yId=id; }
            }
        },&context);
        const Hit& hit=context.hit;
        const double fraction=std::min(hit.time,1.0);
        result.box.center.x+=remaining.x*fraction; result.box.center.y+=remaining.y*fraction;
        if(hit.time>1) break;
        remaining.x*=1-fraction; remaining.y*=1-fraction;
        if(hit.x) { result.box.center.x=hit.snapX; remaining.x=0; result.blockedX=true; }
        if(hit.y) { result.box.center.y=hit.snapY; remaining.y=0; result.blockedY=true; }
    }
    struct GroundContext { const std::vector<Box>* boxes; Motion* motion; } ground{&m_boxes,&result};
    Box foot=result.box; foot.half.y+=kContact;
    Visit(foot,[](usize id,void* opaque) {
        auto& c=*static_cast<GroundContext*>(opaque); ++c.motion->candidates; ++c.motion->tests;
        const Box& a=c.motion->box; const Box& b=(*c.boxes)[id];
        if(a.half.x+b.half.x-std::abs(a.center.x-b.center.x)>kContact &&
           std::abs((a.center.y-a.half.y)-(b.center.y+b.half.y))<=kContact) c.motion->grounded=true;
    },&ground);
    if(!Valid(result.box)) return Error("physics.motion.invalid","resolved character is outside supported bounds",
                                       "move the spawn and terrain within +/-1e6",source);
    return result;
}
} // namespace alice::runtime::physics
