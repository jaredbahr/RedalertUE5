#include "Presentation/ProofContextOrderPolicy.h"
#include <cstdio>
#include <cstdint>
#include <limits>

int main()
{
    struct Case { bool Skirmish; std::int32_t Owner, Target, Team, OtherTeam; bool Expected; };
    const auto Max = std::numeric_limits<std::int32_t>::max();
    const Case Cases[] = {
        {true,1,1,0,0,false}, {true,1,2,0,0,false}, {true,1,100,0,1,true},
        {true,2,1,0,0,false}, {true,100,1,1,0,true}, {true,100,100,1,1,false},
        {true,-1,2,0,1,false}, {true,1,-1,0,1,false}, {true,1,2,-1,0,false},
        {true,1,2,0,-1,false}, {true,1,2,-1,-1,false}, {true,1,2,7,7,false},
        {true,1,2,7,8,true}, {true,0,1,0,1,true}, {true,1,0,1,0,true},
        {false,1,2,0,0,true}, {false,1,2,1,1,true}, {false,1,1,0,1,false},
        {false,-1,2,0,0,false}, {false,1,-1,0,0,false},
        {false,1,2,-1,0,false}, {false,1,2,0,-1,false},
        {false,0,100,0,0,true}, {false,100,0,0,0,true},
        {true,Max,1,Max,0,true}, {true,1,Max,0,Max,true},
        {true,1,2,Max,Max,false}, {true,Max,Max,0,1,false}
    };
    int Count=0;
    for (const Case& C : Cases)
    {
        ++Count;
        if (FProofContextOrderPolicy::IsHostile(C.Skirmish,C.Owner,C.Target,C.Team,C.OtherTeam) != C.Expected)
        { std::printf("FAIL context relationship case %d\n",Count);return 1; }
    }
    std::printf("PASS %d compiled production relationship cases\n",Count);
    return 0;
}
