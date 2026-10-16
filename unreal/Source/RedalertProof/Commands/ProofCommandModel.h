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
    Gather
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
    UPROPERTY() float HitPoints = 100.f;
    UPROPERTY() EProofUnitKind Kind = EProofUnitKind::Combat;
    UPROPERTY() int32 Cargo = 0;
};

USTRUCT()
struct REDALERTPROOF_API FProofResourceRecord
{
    GENERATED_BODY()
    UPROPERTY() int32 ResourceId = INDEX_NONE;
    UPROPERTY() FVector Position = FVector::ZeroVector;
    UPROPERTY() int32 Remaining = 0;
};

USTRUCT()
struct REDALERTPROOF_API FProofSnapshot
{
    GENERATED_BODY()
    UPROPERTY() int32 Revision = 0;
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
    FProofCommandResult Submit(int32 TrustedOwnerId, const FProofCommand& Command);
    // Finite nonnegative advances reset the test command allowance; zero does
    // not simulate. Positive advances compute movement then simultaneous damage.
    void Advance(float DeltaSeconds);
    FProofSnapshot GetSnapshot() const;
    void Reset();

private:
    struct FOrder
    {
        EProofCommandType Type = EProofCommandType::Move;
        FVector Destination = FVector::ZeroVector;
        int32 TargetUnitId = INDEX_NONE;
        bool bReturning = false;
    };

    FProofSnapshot State;
    TMap<int32, FOrder> Orders;
    TMap<int32, int32> LastSequences;
    TMap<int32, int32> CommandCounts;
    int32 NextUnitId = 1;
    int32 NextResourceId = 1;
    int32 CommandsPerAdvance = 32;

    void BumpRevision();
    static bool IsFinitePosition(const FVector& Position);
    static bool IsInWorld(const FVector& Position);
    int32 AddUnit(int32 OwnerId, const FVector& Position, EProofUnitKind Kind, float HitPoints);
    int32 CountLiveOwnerUnits(int32 OwnerId) const;
    int32 TeamForOwner(int32 OwnerId) const;
    const FProofUnitRecord* FindLiveRefinery(int32 OwnerId, const FVector& From) const;
};
