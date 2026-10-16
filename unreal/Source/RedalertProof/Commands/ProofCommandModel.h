#pragma once

#include "CoreMinimal.h"
#include "ProofCommandModel.generated.h"

UENUM()
enum class EProofCommandType : uint8
{
    Move,
    Attack,
    Produce
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
};

USTRUCT()
struct REDALERTPROOF_API FProofUnitRecord
{
    GENERATED_BODY()
    UPROPERTY() int32 UnitId = INDEX_NONE;
    UPROPERTY() int32 OwnerId = INDEX_NONE;
    UPROPERTY() FVector Position = FVector::ZeroVector;
    UPROPERTY() float HitPoints = 100.f;
};

USTRUCT()
struct REDALERTPROOF_API FProofSnapshot
{
    GENERATED_BODY()
    UPROPERTY() int32 Revision = 0;
    UPROPERTY() TArray<FProofPlayerRecord> Players;
    UPROPERTY() TArray<FProofUnitRecord> Units;

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

// Tiny authoritative rules fixture: no collision, pathfinding, economy balance,
// final unit budget, or network transport. Human and bot adapters both Submit.
class REDALERTPROOF_API FProofCommandModel
{
public:
    static constexpr int32 ProductionCost = 50;
    static constexpr float MovementSpeed = 150.f;
    static constexpr float AttackRange = 300.f;
    static constexpr float DamagePerSecond = 25.f;
    static constexpr float WorldExtent = 10000.f;

    explicit FProofCommandModel(int32 InCommandsPerAdvance = 32);
    void AddPlayer(int32 OwnerId, int32 StartingCredits = 200);
    int32 SeedUnit(int32 OwnerId, FVector Position);
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
    };

    FProofSnapshot State;
    TMap<int32, FOrder> Orders;
    TMap<int32, int32> LastSequences;
    TMap<int32, int32> CommandCounts;
    int32 NextUnitId = 1;
    int32 CommandsPerAdvance = 32;

    void BumpRevision();
    static bool IsFinitePosition(const FVector& Position);
    static bool IsInWorld(const FVector& Position);
};
