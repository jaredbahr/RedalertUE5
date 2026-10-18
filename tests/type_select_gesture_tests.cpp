#include "../unreal/Source/RedalertProof/Presentation/ProofTypeSelectGesture.h"
#include <iostream>
#include <limits>
#include <cmath>
int main()
{
    using namespace ProofTypeSelectGesture;
    int checks=0, failed=0;
    const auto Check=[&](bool value){++checks;if(!value){++failed;std::cerr<<"failed "<<checks<<'\n';}};
    Context c; c.Owner=1;c.Epoch=1;c.Anchor=7;c.Kind=0;c.Mode=1;c.SelectionRevision=4;
    const auto Arm=[&](State& state,double now,Context context){Check(state.Press(now,context,true,true)==Scope::Visible);++context.SelectionRevision;Check(state.CommitVisible(now,context));state.Release();return context;};
    State state;
    Context post=Arm(state,0.0,c);
    Check(state.Press(0.3,post,true,true)==Scope::Battlefield);
    Check(!state.IsArmed());
    Check(state.Press(0.3,post,true,true)==Scope::Ignored);
    state.Release();Check(state.Press(0.31,post,true,true)==Scope::Visible); // Triple is a fresh pair.
    Check(state.CommitVisible(0.31,post));
    state.Release();Check(state.Press(0.4,post,true,true)==Scope::Battlefield);
    for(int field=0;field<6;++field){State s;Context p=Arm(s,1.0,c);if(field==0)++p.Owner;if(field==1)++p.Epoch;if(field==2)++p.Anchor;if(field==3)++p.Kind;if(field==4)++p.Mode;if(field==5)++p.SelectionRevision;Check(s.Press(1.1,p,true,true)==Scope::Visible);}
    for(double elapsed:{std::nextafter(PairSeconds,0.0),PairSeconds,std::nextafter(PairSeconds,1.0)}){State s;Context p=Arm(s,0.0,c);Check(s.Press(elapsed,p,true,true)==(elapsed<=PairSeconds?Scope::Battlefield:Scope::Visible));}
    for(double invalid:{-1.0,std::numeric_limits<double>::quiet_NaN(),std::numeric_limits<double>::infinity(),-std::numeric_limits<double>::infinity()}){State s;Context p=Arm(s,1.0,c);Check(s.Press(invalid,p,true,true)==Scope::Ignored);Check(!s.IsArmed());}
    {State s;Context p=Arm(s,1.0,c);Check(s.Press(0.9,p,true,true)==Scope::Ignored);Check(!s.IsArmed());s.Release();Check(s.Press(1.1,p,true,true)==Scope::Visible);}
    for(int mode=0;mode<2;++mode){State s;Context p=Arm(s,1.0,c);Check(s.Press(1.1,p,mode==0,mode!=0)==Scope::Ignored);Check(!s.IsArmed());s.Release();Check(s.Press(1.2,p,true,true)==Scope::Visible);}
    {State s;Check(!s.CommitVisible(1.0,c));Check(s.Press(1.0,c,true,true)==Scope::Visible);Check(!s.CommitVisible(1.1,c));Check(s.CommitVisible(1.0,c));Check(s.Press(1.2,c,true,true)==Scope::Ignored);s.Release();Check(s.Press(1.31,c,true,true)==Scope::Visible);}
    {State s;Context p=Arm(s,1.0,c);s.Cancel();Check(!s.IsArmed());Check(s.Press(1.1,p,true,true)==Scope::Visible);s.Release();Check(!s.CommitVisible(1.1,p));}
    static_assert(noexcept(state.Press(0,c,true,true)),"noexcept input policy");
    std::cout<<checks<<" checks, "<<failed<<" failures\n";
    return failed==0?0:1;
}
