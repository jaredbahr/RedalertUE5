#include "Runtime/ProofProductionAccounting.h"
#include <iostream>
#include <stdexcept>
#include <cmath>

namespace PA = ProofProductionAccounting;
static int Checks = 0;
static void Require(bool Value, const char* Message)
{ ++Checks; if (!Value) throw std::runtime_error(Message); }
struct Fixture
{
    FProofCommandModel M;
    int Seq[3] = {0, 0, 0};
    Fixture()
    {
        M.StartSkirmish();
        Require(M.SeedSkirmishPlayer(1, 0, FVector(-3000,0,0)), "seed owner one");
        Require(M.SeedSkirmishPlayer(2, 1, FVector(3000,0,0)), "seed owner two");
    }
    int Id(int Owner, EProofUnitKind Kind) const
    { for (const auto& U : M.GetSnapshot().Units) if (U.OwnerId == Owner && U.Kind == Kind && U.HitPoints > 0) return U.UnitId; return INDEX_NONE; }
    FProofCommandResult Send(int Owner, EProofCommandType Type, int Unit = INDEX_NONE, int Target = INDEX_NONE, FVector Point = FVector::ZeroVector)
    { FProofCommand C; C.Sequence = ++Seq[Owner]; C.Type = Type; C.UnitId = Unit; C.TargetUnitId = Target; C.Destination = Point; return M.Submit(Owner,C); }
    void Factory(int Owner = 1)
    { Require(Send(Owner,EProofCommandType::BuildFactory,INDEX_NONE,INDEX_NONE,FVector(Owner == 1 ? -2650 : 3350,0,0)).bAccepted,"build factory"); }
    void Start(int Owner = 1)
    { Require(Send(Owner,EProofCommandType::Produce,Id(Owner,EProofUnitKind::Factory)).bAccepted,"start production"); }
    void Control(EProofCommandType Type)
    { const auto J = *PA::Job(M.GetSnapshot(),1); Require(Send(1,Type,J.FactoryId,J.StartSequence).bAccepted,"job control"); }
    FProofSnapshot Step(int& Payment, int& Refund)
    {
        const auto Before=M.GetSnapshot(); M.Advance(.1f); const auto After=M.GetSnapshot();
        Require(PA::ValidateAdvance(Before,After,1,Payment,Refund),"actual owner one Advance rejected");
        int P2=0,R2=0;Require(PA::ValidateAdvance(Before,After,2,P2,R2),"actual owner two Advance rejected");
        return After;
    }
};
static void Reject(const FProofSnapshot& Before, const FProofSnapshot& Bad, const char* Message)
{ int P=0,R=0; Require(!PA::ValidateAdvance(Before,Bad,1,P,R),Message); }
int main()
{
    try
    {
        Fixture F; F.Factory(); F.Start();
        const auto Start=F.M.GetSnapshot(); int P=0,R=0;
        auto Paid=F.Step(P,R); Require(P == 1 && R == 0,"one credit per tenth");
        std::cout << "CONTEXT " << *PA::ContextJson(Paid) << '\n';
        auto Bad=Paid; Bad.ProductionJobs[0].PaidCredits=0;Reject(Start,Bad,"unpaid progress accepted");
        Bad=Paid;Bad.ProductionJobs[0].WorkMicroseconds=0;Reject(Start,Bad,"paid without work accepted");
        Bad=Paid;Bad.ProductionJobs[0].StartSequence++;Reject(Start,Bad,"reidentified job accepted");
        Bad=Paid;Bad.ProductionJobs[0].FactoryId++;Reject(Start,Bad,"wrong factory accepted");
        Bad=Paid;Bad.ProductionJobs[0].bHeld=true;Reject(Start,Bad,"hold command inside Advance accepted");
        Bad=Paid;Bad.Players[0].Credits--;Reject(Start,Bad,"extra debit accepted");
        Bad=Paid;Bad.ProductionJobs.Reset();Reject(Start,Bad,"disappeared unpaid job accepted");
        Bad=Paid;Bad.Units.Add(Bad.Units[0]);Reject(Start,Bad,"duplicate unit accepted");
        Bad=Paid;Bad.Players.Add(Bad.Players[0]);Reject(Start,Bad,"duplicate owner accepted");
        F.Control(EProofCommandType::HoldProduction); auto Held=F.M.GetSnapshot();Paid=F.Step(P,R);
        Require(P == 0 && R == 0 && PA::SameJob(*PA::Job(Held,1),*PA::Job(Paid,1)),"held debit/work");
        Bad=Paid;Bad.ProductionJobs[0].PaidCredits++;Bad.ProductionJobs[0].WorkMicroseconds+=100000;Bad.Players[0].Credits--;
        Reject(Held,Bad,"held progress accepted");
        F.Control(EProofCommandType::ResumeProduction);
        for(int I=0;I<48;++I) F.Step(P,R);
        const auto CompleteBefore=F.M.GetSnapshot();const auto Complete=F.Step(P,R);
        Require(!PA::Job(Complete,1) && P == 1 && R == 0,"completion payment");
        std::cout << "CONTEXT " << *PA::ContextJson(Complete) << '\n';
        Bad=Complete;Bad.Units[Bad.Units.Num()-1].Position.X+=0.00000001;Reject(CompleteBefore,Bad,"incorrect completion spawn accepted");
        Bad=Complete;Bad.Units[Bad.Units.Num()-1].LastTravelDirection=FVector(1,0,0);Reject(CompleteBefore,Bad,"already moved completion accepted");
        Bad=Complete;Bad.Units[Bad.Units.Num()-1].bAreaGuard=true;Reject(CompleteBefore,Bad,"already guarding completion accepted");
        Bad=Complete;Bad.Units[Bad.Units.Num()-1].AreaGuardTargetId=1;Reject(CompleteBefore,Bad,"noncanonical guard target accepted");
        F.Step(P,R);Require(P == 0 && R == 0,"double debit after completion");
        F.Start();F.Step(P,R);F.Step(P,R);const auto CancelBefore=F.M.GetSnapshot();F.Control(EProofCommandType::CancelProduction);
        const auto Cancel=F.M.GetSnapshot();Require(!PA::Job(Cancel,1) && PA::Player(Cancel,1)->Credits-PA::Player(CancelBefore,1)->Credits == 2,"exact public cancellation refund");
        Reject(CancelBefore,Cancel,"command cancellation misclassified as Advance refund");

        Fixture Shortage;Shortage.Factory();
        Require(Shortage.Send(1,EProofCommandType::BuildRefinery,INDEX_NONE,INDEX_NONE,FVector(-3350,0,0)).bAccepted,"first shortage refinery");
        Require(Shortage.Send(1,EProofCommandType::BuildRefinery,INDEX_NONE,INDEX_NONE,FVector(-3000,350,0)).bAccepted,"second shortage refinery");
        for(const auto& U:Shortage.M.GetSnapshot().Units)
            if(U.OwnerId==1 && U.Kind==EProofUnitKind::Harvester) Require(Shortage.Send(1,EProofCommandType::Stop,U.UnitId).bAccepted,"stop shortage miner");
        Shortage.Start();auto Paused=Shortage.Step(P,R);
        Require(P==0 && R==0 && PA::Job(Paused,1)->WorkMicroseconds==0 && PA::Player(Paused,1)->Credits==0,"actual cash-shortage boundary");
        const int ShortMiner=Shortage.Id(1,EProofUnitKind::Harvester);
        Require(Shortage.Send(1,EProofCommandType::Gather,ShortMiner,Shortage.M.GetSnapshot().Resources[0].ResourceId).bAccepted,"resume shortage cash flow");
        bool Refilled=false;
        for(int I=0;I<500 && !Refilled;++I)
        {
            const auto Before=Shortage.M.GetSnapshot();const auto After=Shortage.Step(P,R);
            if(PA::Player(After,1)->TotalGathered>PA::Player(Before,1)->TotalGathered)
            {
                Require(P==0 && PA::Job(After,1)->WorkMicroseconds==0,"same-step deposit funded production too early");
                const auto Next=Shortage.Step(P,R);Require(P==1 && PA::Job(Next,1)->WorkMicroseconds==100000,"cash did not resume next step");Refilled=true;
            }
        }
        Require(Refilled,"shortage income never deposited");

        Fixture Deposit;Deposit.Factory();Require(Deposit.Send(1,EProofCommandType::BuildRefinery,INDEX_NONE,INDEX_NONE,FVector(-3350,0,0)).bAccepted,"build depot");
        const int Miner=Deposit.Id(1,EProofUnitKind::Harvester), Depot=Deposit.Id(1,EProofUnitKind::Refinery);
        const int Ore=Deposit.M.GetSnapshot().Resources[0].ResourceId;
        Require(Deposit.Send(1,EProofCommandType::Gather,Miner,Ore).bAccepted,"gather starter");
        bool Combined=false;
        for(int I=0;I<500 && !Combined;++I)
        {
            const auto S=Deposit.M.GetSnapshot();const auto* U=PA::Unit(S,Miner);const auto* D=PA::Unit(S,Depot);
            if(U->Cargo>0 && FVector::DistSquared(U->Position,D->Position)<=225.0)
            {
                Deposit.Start();const auto Before=Deposit.M.GetSnapshot();const auto After=Deposit.Step(P,R);
                Require(P==1 && R==0 && PA::Player(After,1)->TotalGathered>PA::Player(Before,1)->TotalGathered,"same-step deposit plus payment");
                Require(PA::Player(After,1)->Credits-PA::Player(Before,1)->Credits==49,"deposit less payment credit identity");
                Bad=After;Bad.Players[0].Credits++;Reject(Before,Bad,"deposit ignored production payment");
                Bad=After;
                Bad.Units.FindByPredicate([Miner](const FProofUnitRecord& U){return U.UnitId==Miner;})->Position = PA::Unit(After,Depot)->Position + FVector(1.00000001,0,0);
                Reject(Before,Bad,"deposit just outside exact boundary accepted");
                Combined=true;
            }
            else Deposit.Step(P,R);
        }
        Require(Combined,"public simultaneous deposit/payment setup did not occur");

        Fixture Loss;Loss.Factory();Loss.Start();for(int I=0;I<5;++I)Loss.Step(P,R);Loss.Control(EProofCommandType::HoldProduction);
        Loss.Factory(2);Loss.Start(2);for(int I=0;I<50;++I)Loss.Step(P,R);
        const int Factory=Loss.Id(1,EProofUnitKind::Factory);Require(Loss.Send(2,EProofCommandType::Attack,Loss.Id(2,EProofUnitKind::Combat),Factory).bAccepted,"attack held factory");
        bool Refunded=false;
        for(int I=0;I<1000 && !Refunded;++I)
        {
            const auto Before=Loss.M.GetSnapshot();const auto After=Loss.Step(P,R);
            if(R>0)
            {
                Require(R==5 && P==0 && PA::Unit(After,Factory)->HitPoints==0 && !PA::Job(After,1),"loss precedence exact refund");
                Bad=After;Bad.Units.FindByPredicate([Factory](const FProofUnitRecord& U){return U.UnitId==Factory;})->HitPoints=200;
                Reject(Before,Bad,"live factory refund accepted");
                Bad=After;Bad.Players[0].Credits++;Reject(Before,Bad,"over-refund accepted");
                std::cout << "CONTEXT " << *PA::ContextJson(After) << '\n';Refunded=true;
            }
        }
        Require(Refunded,"public factory loss refund never occurred");
        std::cout << "PASS production accounting actual-header/public-model checks=" << Checks << " (portable only)\n";
        return 0;
    }
    catch(const std::exception& E){std::cerr << "FAIL " << E.what() << '\n';return 1;}
}
