#pragma once

#include "CoreMinimal.h"
#include "ProofCommandModel.generated.h"

UENUM()
enum class EProofCommandType : uint8
{
    Move,
    Attack,
    Produce,
    BuildRefinery,
    BuildFactory,
    Gather,
    Stop,
    SetRally = 7,
    AttackMove = 8,
    ReturnCargo = 9
};

UENUM()
enum class EProofUnitKind : uint8
{
    Combat,
    Harvester,
    HQ,
    Refinery,
    Factory
};

UENUM()
enum class EProofMatchPhase : uint8
{
    Inactive,
    Playing,
    Finished
};

// Identity comes from the authoritative adapter, never from this client payload.
USTRUCT()
struct REDALERTPROOF_API FProofCommand
{
    GENERATED_BODY()
    UPROPERTY() int32 Sequence = 0;
    // The server's match generation, stamped by clients from the replicated snapshot.
    UPROPERTY() int32 MatchEpoch = 0;
    UPROPERTY() EProofCommandType Type = EProofCommandType::Move;
    UPROPERTY() int32 UnitId = INDEX_NONE;
    UPROPERTY() int32 TargetUnitId = INDEX_NONE;
    UPROPERTY() FVector Destination = FVector::ZeroVector;
};

USTRUCT()
struct REDALERTPROOF_API FProofPlayerRecord
{
    GENERATED_BODY()
    UPROPERTY() int32 OwnerId = INDEX_NONE;
    UPROPERTY() int32 Credits = 0;
    UPROPERTY() int32 TeamId = 0;
    UPROPERTY() int32 TotalGathered = 0;
};

USTRUCT()
struct REDALERTPROOF_API FProofUnitRecord
{
    GENERATED_BODY()
    UPROPERTY() int32 UnitId = INDEX_NONE;
    UPROPERTY() int32 OwnerId = INDEX_NONE;
    UPROPERTY() FVector Position = FVector::ZeroVector;
    // Authoritative display metadata only: last actual nonzero XY travel.
    // Zero means no observed travel; this is not hull/turret simulation state.
    UPROPERTY() FVector LastTravelDirection = FVector::ZeroVector;
    UPROPERTY() float HitPoints = 100.f;
    UPROPERTY() EProofUnitKind Kind = EProofUnitKind::Combat;
    UPROPERTY() int32 Cargo = 0;
    // Only live factories can own a rally. New records and resets have none.
    UPROPERTY() bool bHasRallyPoint = false;
    UPROPERTY() FVector RallyPoint = FVector::ZeroVector;
};

USTRUCT()
struct REDALERTPROOF_API FProofResourceRecord
{
    GENERATED_BODY()
    UPROPERTY() int32 ResourceId = INDEX_NONE;
    // The skirmish starter patch belongs to the seeded player for teardown.
    UPROPERTY() int32 OwnerId = INDEX_NONE;
    UPROPERTY() FVector Position = FVector::ZeroVector;
    UPROPERTY() int32 Remaining = 0;
};

USTRUCT()
struct REDALERTPROOF_API FProofSnapshot
{
    GENERATED_BODY()
    UPROPERTY() int32 Revision = 0;
    UPROPERTY() int32 MatchEpoch = 0;
    UPROPERTY() TArray<FProofPlayerRecord> Players;
    UPROPERTY() TArray<FProofUnitRecord> Units;
    UPROPERTY() bool bSkirmish = false;
    UPROPERTY() EProofMatchPhase MatchPhase = EProofMatchPhase::Inactive;
    UPROPERTY() int32 WinnerTeam = INDEX_NONE;
    UPROPERTY() TArray<FProofResourceRecord> Resources;

    // Diagnostic equality of visible state, not authentication or lockstep proof.
    FString ComputeDigest() const;
};

USTRUCT()
struct REDALERTPROOF_API FProofCommandResult
{
    GENERATED_BODY()
    UPROPERTY() bool bAccepted = false;
    UPROPERTY() FString Reason;
    UPROPERTY() int32 Sequence = 0;
    UPROPERTY() int32 MatchEpoch = 0;
};

// Tiny authoritative proof fixture: no collision, pathfinding, or network
// transport. Human and bot adapters both Submit through the same validator.
class REDALERTPROOF_API FProofCommandModel
{
public:
    static constexpr int32 ProductionCost = 50;
    static constexpr float MovementSpeed = 150.f;
    static constexpr float AttackRange = 300.f;
    static constexpr float DamagePerSecond = 25.f;
    static constexpr float WorldExtent = 10000.f;
    static constexpr int32 RefineryCost = 100;
    static constexpr int32 FactoryCost = 150;
    static constexpr int32 CargoCapacity = 50;
    static constexpr int32 SkirmishRecordCap = 12;
    static constexpr float BuildRadius = 800.f;
    static constexpr float StructureSeparation = 180.f;

    explicit FProofCommandModel(int32 InCommandsPerAdvance = 32);
    void AddPlayer(int32 OwnerId, int32 StartingCredits = 200);
    int32 SeedUnit(int32 OwnerId, FVector Position);
    void StartSkirmish();
    bool SeedSkirmishPlayer(int32 OwnerId, int32 TeamId, const FVector& HQPosition);
    // Vacate one skirmish seat without disturbing surviving players or the bot.
    bool RemoveSkirmishPlayer(int32 OwnerId);
    // Pure shared placement for production and refinery grants. Checks all live
    // centers in fixed order (300 cm steps, three rings, >=260 cm separation).
    // Anchor/candidates must be finite and in-world; failure clears OutSpawn.
    static bool FindSkirmishSpawn(const FProofSnapshot& Snapshot, const FVector& Anchor, FVector& OutSpawn);
    FProofCommandResult Submit(int32 TrustedOwnerId, const FProofCommand& Command);
    // Finite nonnegative advances reset the test command allowance; zero does
    // not simulate. Positive advances compute movement then simultaneous damage.
    void Advance(float DeltaSeconds);
    FProofSnapshot GetSnapshot() const;
    void Reset();

private:
#if WITH_DEV_AUTOMATION_TESTS
    // Bounded exhaustion coverage without exposing a production ID setter.
    friend class FProofRefineryMinerIdBoundaryTest;
    friend struct FProofOreReselectionFixture;
    friend struct FProofReturnCargoFixture;
    friend struct FProofLastTravelDirectionFixture;
#endif
    struct FOrder
    {
        EProofCommandType Type = EProofCommandType::Move;
        FVector Destination = FVector::ZeroVector;
        int32 TargetUnitId = INDEX_NONE;
        bool bReturning = false;
        // ReturnCargo targets one refinery. Only a displaced Gather supplies
        // this resource ID; ordinary Move/Stop/idle returns never auto-harvest.
        int32 ResumeResourceId = INDEX_NONE;
    };

    FProofSnapshot State;
    TMap<int32, FOrder> Orders;
    TMap<int32, int32> LastSequences;
    TMap<int32, int32> CommandCounts;
    int32 NextUnitId = 1;
    int32 NextResourceId = 1;
    int32 CommandsPerAdvance = 32;

    void BumpRevision();
    static void RecordLastTravelDirection(FProofUnitRecord& Unit, const FVector& PreviousPosition);
    static bool IsFinitePosition(const FVector& Position);
    static bool IsInWorld(const FVector& Position);
    int32 AddUnit(int32 OwnerId, const FVector& Position, EProofUnitKind Kind, float HitPoints);
    int32 CountLiveOwnerUnits(int32 OwnerId) const;
    int32 TeamForOwner(int32 OwnerId) const;
    const FProofUnitRecord* FindAttackMoveTarget(const TArray<FProofUnitRecord>& Units,
        const FProofUnitRecord& Unit, int32 PreferredTargetId) const;
    FProofResourceRecord* ResolveGatherResource(FOrder& Order, const FVector& From);
    const FProofUnitRecord* FindLiveRefinery(int32 OwnerId, const FVector& From) const;
    const FProofUnitRecord* FindReturnCargoRefinery(int32 OwnerId, int32 RefineryId) const;
};
