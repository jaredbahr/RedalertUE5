#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "HAL/PlatformTime.h"
#include "Commands/ProofCommandModel.h"

namespace
{
struct FStressMeasurement
{
	TArray<double> AdvanceMilliseconds;
	TArray<double> SnapshotDigestMilliseconds;
	FString FinalDigest;
	int32 FinalLiveUnits = 0;
	int32 AcceptedAttacks = 0;
	int32 FinalMovedUnits = 0;
	int32 FinalDamagedUnits = 0;
};

FProofCommand MakeAttack(int32 Sequence, int32 UnitId, int32 TargetUnitId)
{
	FProofCommand Command;
	Command.Sequence = Sequence;
	Command.Type = EProofCommandType::Attack;
	Command.UnitId = UnitId;
	Command.TargetUnitId = TargetUnitId;
	return Command;
}

double Percentile(TArray<double> Samples, double Fraction)
{
	Samples.Sort();
	const int32 Index = FMath::Clamp(FMath::CeilToInt(Fraction * Samples.Num()) - 1, 0, Samples.Num() - 1);
	return Samples[Index];
}

FStressMeasurement RunArmy(int32 ArmySize)
{
	constexpr int32 Steps = 100;
	constexpr float StepSeconds = 0.1f;
	constexpr int32 Rows = 16;
	constexpr double PairSpacing = 2800.0;
	FProofCommandModel Model(ArmySize);
	Model.AddPlayer(1);
	Model.AddPlayer(2);
	TArray<int32> FirstOwner;
	TArray<int32> SecondOwner;
	FirstOwner.Reserve(ArmySize / 2);
	SecondOwner.Reserve(ArmySize / 2);

	for (int32 Pair = 0; Pair < ArmySize / 2; ++Pair)
	{
		const int32 Row = Pair % Rows;
		const int32 Column = Pair / Rows;
		const FVector Offset(0.0, (Row - (Rows / 2)) * 120.0, Column * 120.0);
		FirstOwner.Add(Model.SeedUnit(1, FVector(-PairSpacing * 0.5, 0.0, 0.0) + Offset));
		SecondOwner.Add(Model.SeedUnit(2, FVector(PairSpacing * 0.5, 0.0, 0.0) + Offset));
	}

	FStressMeasurement Measurement;
	const FProofSnapshot InitialSnapshot = Model.GetSnapshot();
	for (int32 Pair = 0; Pair < FirstOwner.Num(); ++Pair)
	{
		if (FirstOwner[Pair] > 0 && SecondOwner[Pair] > 0
			&& Model.Submit(1, MakeAttack(Pair + 1, FirstOwner[Pair], SecondOwner[Pair])).bAccepted)
		{
			++Measurement.AcceptedAttacks;
		}
		if (FirstOwner[Pair] > 0 && SecondOwner[Pair] > 0
			&& Model.Submit(2, MakeAttack(Pair + 1, SecondOwner[Pair], FirstOwner[Pair])).bAccepted)
		{
			++Measurement.AcceptedAttacks;
		}
	}

	Measurement.AdvanceMilliseconds.Reserve(Steps);
	Measurement.SnapshotDigestMilliseconds.Reserve(Steps);
	for (int32 Step = 0; Step < Steps; ++Step)
	{
		const double AdvanceStart = FPlatformTime::Seconds();
		Model.Advance(StepSeconds);
		Measurement.AdvanceMilliseconds.Add((FPlatformTime::Seconds() - AdvanceStart) * 1000.0);

		const double SnapshotStart = FPlatformTime::Seconds();
		const FProofSnapshot Snapshot = Model.GetSnapshot();
		const FString Digest = Snapshot.ComputeDigest();
		Measurement.SnapshotDigestMilliseconds.Add((FPlatformTime::Seconds() - SnapshotStart) * 1000.0);
		Measurement.FinalDigest = Digest;
		Measurement.FinalLiveUnits = 0;
		Measurement.FinalMovedUnits = 0;
		Measurement.FinalDamagedUnits = 0;
		for (const FProofUnitRecord& Unit : Snapshot.Units)
		{
			if (Unit.HitPoints > 0.0f) { ++Measurement.FinalLiveUnits; }
			const FProofUnitRecord* InitialUnit = InitialSnapshot.Units.FindByPredicate([&Unit](const FProofUnitRecord& Initial) { return Initial.UnitId == Unit.UnitId; });
			if (InitialUnit && !Unit.Position.Equals(InitialUnit->Position, 0.01)) { ++Measurement.FinalMovedUnits; }
			if (Unit.HitPoints < 100.0f) { ++Measurement.FinalDamagedUnits; }
		}
	}
	return Measurement;
}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FProofStressBaselineTest, "RedalertProof.Stress.ModelBaseline", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FProofStressBaselineTest::RunTest(const FString&)
{
	constexpr int32 Steps = 100;
	for (const int32 ArmySize : { 64, 128, 256 })
	{
		const FStressMeasurement First = RunArmy(ArmySize);
		const FStressMeasurement Repeat = RunArmy(ArmySize);
		const bool bRepeatDigestMatched = Repeat.FinalDigest == First.FinalDigest;
		TestEqual(FString::Printf(TEXT("%d-unit all attack commands accepted"), ArmySize), First.AcceptedAttacks, ArmySize);
		TestTrue(FString::Printf(TEXT("%d-unit repeated final digest"), ArmySize), bRepeatDigestMatched);
		TestEqual(FString::Printf(TEXT("%d-unit all units remain active at final sample"), ArmySize), First.FinalLiveUnits, ArmySize);
		TestEqual(FString::Printf(TEXT("%d-unit all units moved"), ArmySize), First.FinalMovedUnits, ArmySize);
		TestEqual(FString::Printf(TEXT("%d-unit all units took damage"), ArmySize), First.FinalDamagedUnits, ArmySize);

		const double AdvanceMedian = Percentile(First.AdvanceMilliseconds, 0.50);
		const double AdvanceP95 = Percentile(First.AdvanceMilliseconds, 0.95);
		const double AdvanceMaximum = Percentile(First.AdvanceMilliseconds, 1.0);
		const double SnapshotMedian = Percentile(First.SnapshotDigestMilliseconds, 0.50);
		const double SnapshotP95 = Percentile(First.SnapshotDigestMilliseconds, 0.95);
		const double SnapshotMaximum = Percentile(First.SnapshotDigestMilliseconds, 1.0);
		AddInfo(FString::Printf(
			TEXT("STRESS_BASELINE {\"fixture\":\"legacy_proof_command_model\",\"rendered\":false,\"networked\":false,\"armySize\":%d,\"owners\":2,\"acceptedAttackCommands\":%d,\"activeCombatUnits\":%d,\"movedUnits\":%d,\"damagedUnits\":%d,\"steps\":%d,\"stepSeconds\":%.1f,\"repeatDigestMatched\":%s,\"finalDigest\":\"%s\",\"advanceMs\":{\"median\":%.6f,\"p95\":%.6f,\"max\":%.6f},\"snapshotAndComputeDigestMs\":{\"median\":%.6f,\"p95\":%.6f,\"max\":%.6f}}"),
			ArmySize, First.AcceptedAttacks, First.FinalLiveUnits, First.FinalMovedUnits, First.FinalDamagedUnits, Steps, 0.1,
			bRepeatDigestMatched ? TEXT("true") : TEXT("false"), *First.FinalDigest,
			AdvanceMedian, AdvanceP95, AdvanceMaximum, SnapshotMedian, SnapshotP95, SnapshotMaximum));
	}
	return true;
}
#endif
