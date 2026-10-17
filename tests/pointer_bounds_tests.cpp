#include "Presentation/ProofPointerBounds.h"
#include <cstdio>
#include <limits>

int main()
{
    struct Case { double X, Y, L, T, R, B; bool Expected; };
    const double Nan = std::numeric_limits<double>::quiet_NaN();
    const double Inf = std::numeric_limits<double>::infinity();
    const Case Cases[] = {
        {0,0,0,0,1280,720,true}, {1279.999,719.999,0,0,1280,720,true},
        {1280,400,0,0,1280,720,false}, {600,720,0,0,1280,720,false},
        {-0.001,400,0,0,1280,720,false}, {600,-0.001,0,0,1280,720,false},
        {1e12,400,0,0,1280,720,false}, {600,1e12,0,0,1280,720,false},
        {Nan,400,0,0,1280,720,false}, {600,Nan,0,0,1280,720,false},
        {Inf,400,0,0,1280,720,false}, {600,-Inf,0,0,1280,720,false},
        {10,10,0,0,0,720,false}, {10,10,0,0,1280,0,false},
        {10,10,100,100,50,50,false}, {10,10,0,0,Inf,720,false},
        {10,10,Nan,0,1280,720,false}, {10,10,0,Nan,1280,720,false},
        {10,10,0,0,1280,Nan,false}, {0.25,0.75,0,0,1,1,true},
        {959.999,539.999,0,0,960,540,true}, {960,540,0,0,960,540,false},
        {100,50,100,50,1180,670,true}, {99.999,400,100,50,1180,670,false},
        {600,49.999,100,50,1180,670,false}, {1180,400,100,50,1180,670,false},
        {600,670,100,50,1180,670,false}, {1179.999,669.999,100,50,1180,670,true}
    };
    int Count = 0;
    for (const Case& C : Cases)
    {
        ++Count;
        if (FProofPointerBounds::Contains(C.X,C.Y,C.L,C.T,C.R,C.B) != C.Expected)
        { std::printf("FAIL pointer case %d\n",Count); return 1; }
    }
    std::printf("PASS %d pointer boundary cases (production header)\n", Count);
    return 0;
}
