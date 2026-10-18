#include "Commands/ProofCommandModel.h"
#include "Misc/SecureHash.h"

#include <openssl/evp.h>

#include <algorithm>
#include <clocale>
#include <iostream>
#include <stdexcept>
#include <string>

namespace
{
int Checks = 0;

void Require(bool Condition, const std::string& Message)
{
    ++Checks;
    if (!Condition)
        throw std::runtime_error(Message);
}

void RequireDigest(const FString& Actual, const char* Expected, const std::string& Label)
{
    const std::string Text(*Actual);
    Require(Text == Expected, Label + ": expected " + Expected + ", got " + Text);
    Require(Text.size() == 32, Label + ": digest must have 32 characters");
    Require(std::all_of(Text.begin(), Text.end(), [](char C)
        { return (C >= '0' && C <= '9') || (C >= 'a' && C <= 'f'); }),
        Label + ": digest must use lowercase hexadecimal");
}

void TestKnownVectors()
{
    // Fixed public answers: RFC 1321, Appendix A.5. These are never derived
    // from this bridge or the production digest implementation at runtime.
    struct FVectorCase { const char* Input; const char* Expected; };
    const FVectorCase Cases[] = {
        {"", "d41d8cd98f00b204e9800998ecf8427e"},
        {"a", "0cc175b9c0f1b6a831c399e269772661"},
        {"abc", "900150983cd24fb0d6963f7d28e17f72"},
        {"message digest", "f96b697d7cb7938d525a2f31aaf161d0"},
        {"abcdefghijklmnopqrstuvwxyz", "c3fcd3d76192e4007dfb496cca67e13b"},
        {"ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789",
            "d174ab98d277d9f5a5611c2c9f419d9f"},
        {"12345678901234567890123456789012345678901234567890123456789012345678901234567890",
            "57edf4a22be3c955ac49da2e2107b67a"}
    };
    for (const auto& Case : Cases)
        RequireDigest(FMD5::HashAnsiString(Case.Input), Case.Expected, "RFC 1321 vector");

    const char EmbeddedNul[] = {'a', 'b', 'c', '\0', 'x', static_cast<char>(0xff), '\0'};
    RequireDigest(FMD5::HashAnsiString(EmbeddedNul), Cases[2].Expected,
        "C-string terminator excludes suffix, including non-ASCII bytes");
    const char InitialNul[] = {'\0', 'a', '\0'};
    RequireDigest(FMD5::HashAnsiString(InitialNul), Cases[0].Expected,
        "initial NUL hashes the empty input");

    for (int I = 0; I < 32; ++I)
    {
        const auto& Case = Cases[I % 7];
        RequireDigest(FMD5::HashAnsiString(Case.Input), Case.Expected,
            "repeated interleaved bridge call");
    }
}

void TestRejectedInputs()
{
    const auto Reject = [](const char* Input, const char* Message)
    {
        bool Rejected = false;
        try { (void)FMD5::HashAnsiString(Input); }
        catch (const std::invalid_argument& Error)
        {
            Rejected = std::string(Error.what()).find(Message) != std::string::npos;
        }
        Require(Rejected, std::string("input was not clearly rejected: ") + Message);
    };
    Reject(nullptr, "null input");
    const char HighBit[] = {static_cast<char>(0x80), '\0'};
    const char Utf8[] = {'a', static_cast<char>(0xc3), static_cast<char>(0xa9), '\0'};
    Reject(HighBit, "non-ASCII input");
    Reject(Utf8, "non-ASCII input");
}

FProofSnapshot MixedSnapshot()
{
    FProofSnapshot Snapshot;
    Snapshot.Revision = 7;
    Snapshot.MatchEpoch = 3;
    Snapshot.bSkirmish = true;
    Snapshot.MatchPhase = EProofMatchPhase::Playing;
    Snapshot.WinnerTeam = INDEX_NONE;

    FProofPlayerRecord Player2;
    Player2.OwnerId = 2;
    Player2.Credits = 125;
    Player2.TotalGathered = 50;
    FProofPlayerRecord Player1;
    Player1.OwnerId = 1;
    Player1.Credits = 200;
    Snapshot.Players = {Player2, Player1};

    FProofUnitRecord Harvester;
    Harvester.UnitId = 9;
    Harvester.OwnerId = 2;
    Harvester.Position = FVector(1.25, -2.5, 0);
    Harvester.HitPoints = 75.f;
    Harvester.Kind = EProofUnitKind::Harvester;
    Harvester.Cargo = 7;
    FProofUnitRecord Factory;
    Factory.UnitId = 4;
    Factory.OwnerId = 1;
    Factory.Position = FVector(-3, 4, 0.5);
    Factory.Kind = EProofUnitKind::Factory;
    Factory.bHasRallyPoint = true;
    Factory.RallyPoint = FVector(10, 20, 0);
    Snapshot.Units = {Harvester, Factory};

    FProofResourceRecord Resource8;
    Resource8.ResourceId = 8;
    Resource8.OwnerId = 2;
    Resource8.Position = FVector(5, 6, 0);
    Resource8.Remaining = 125;
    FProofResourceRecord Resource3;
    Resource3.ResourceId = 3;
    Resource3.OwnerId = 1;
    Resource3.Position = FVector(-7, 8, 0);
    Snapshot.Resources = {Resource8, Resource3};
    return Snapshot;
}

constexpr char EmptyDigest[] = "aa1d59044f1a11b0547336759a1cefb2";
constexpr char MixedDigest[] = "431a40e1809cacc8fff55d85e13312b7";

void TestSnapshotGoldensAndOrder()
{
    // Manually specified canonical inputs and precomputed Python hashlib MD5
    // answers are documented in README.snapshot_digest_bridge.md. The expected
    // values below are fixed and never call the implementation under test.
    const FProofSnapshot Empty;
    RequireDigest(Empty.ComputeDigest(), EmptyDigest, "snapshot golden empty");
    const FProofSnapshot Mixed = MixedSnapshot();
    RequireDigest(Mixed.ComputeDigest(), MixedDigest, "snapshot golden mixed");

    for (int Mask = 0; Mask < 8; ++Mask)
    {
        FProofSnapshot Reordered = Mixed;
        if ((Mask & 1) != 0) Reordered.Players.Swap(0, 1);
        if ((Mask & 2) != 0) Reordered.Units.Swap(0, 1);
        if ((Mask & 4) != 0) Reordered.Resources.Swap(0, 1);
        const int32 FirstPlayer = Reordered.Players[0].OwnerId;
        const int32 FirstUnit = Reordered.Units[0].UnitId;
        const int32 FirstResource = Reordered.Resources[0].ResourceId;
        RequireDigest(Reordered.ComputeDigest(), MixedDigest, "canonical array ordering");
        Require(Reordered.Players[0].OwnerId == FirstPlayer
            && Reordered.Units[0].UnitId == FirstUnit
            && Reordered.Resources[0].ResourceId == FirstResource,
            "ComputeDigest must not reorder the original arrays");
    }
    for (int I = 0; I < 32; ++I)
    {
        RequireDigest(Empty.ComputeDigest(), EmptyDigest, "repeated empty snapshot");
        RequireDigest(Mixed.ComputeDigest(), MixedDigest, "repeated mixed snapshot");
    }
}

void TestFieldSensitivity()
{
    const FProofSnapshot Base = MixedSnapshot();
    struct FMutation { const char* Name; void (*Apply)(FProofSnapshot&); };
    const FMutation Cases[] = {
        {"revision", [](FProofSnapshot& S) { ++S.Revision; }},
        {"epoch", [](FProofSnapshot& S) { ++S.MatchEpoch; }},
        {"skirmish", [](FProofSnapshot& S) { S.bSkirmish = false; }},
        {"phase", [](FProofSnapshot& S) { S.MatchPhase = EProofMatchPhase::Finished; }},
        {"winner", [](FProofSnapshot& S) { S.WinnerTeam = 0; }},
        {"player owner", [](FProofSnapshot& S) { S.Players[0].OwnerId = 3; }},
        {"credits", [](FProofSnapshot& S) { ++S.Players[0].Credits; }},
        {"team", [](FProofSnapshot& S) { ++S.Players[0].TeamId; }},
        {"total gathered", [](FProofSnapshot& S) { ++S.Players[0].TotalGathered; }},
        {"unit ID", [](FProofSnapshot& S) { ++S.Units[0].UnitId; }},
        {"unit owner", [](FProofSnapshot& S) { ++S.Units[0].OwnerId; }},
        {"unit X", [](FProofSnapshot& S) { S.Units[0].Position.X += 0.125; }},
        {"unit Y", [](FProofSnapshot& S) { S.Units[0].Position.Y += 0.125; }},
        {"unit Z", [](FProofSnapshot& S) { S.Units[0].Position.Z += 0.125; }},
        {"hit points", [](FProofSnapshot& S) { S.Units[0].HitPoints -= 0.25f; }},
        {"kind", [](FProofSnapshot& S) { S.Units[0].Kind = EProofUnitKind::Combat; }},
        {"cargo", [](FProofSnapshot& S) { ++S.Units[0].Cargo; }},
        {"rally enabled flag", [](FProofSnapshot& S) { S.Units[1].bHasRallyPoint = false; }},
        {"rally X", [](FProofSnapshot& S) { S.Units[1].RallyPoint.X += 0.125; }},
        {"rally Y", [](FProofSnapshot& S) { S.Units[1].RallyPoint.Y += 0.125; }},
        {"rally Z", [](FProofSnapshot& S) { S.Units[1].RallyPoint.Z += 0.125; }},
        {"resource ID", [](FProofSnapshot& S) { ++S.Resources[0].ResourceId; }},
        {"resource owner", [](FProofSnapshot& S) { ++S.Resources[0].OwnerId; }},
        {"resource X", [](FProofSnapshot& S) { S.Resources[0].Position.X += 0.125; }},
        {"resource Y", [](FProofSnapshot& S) { S.Resources[0].Position.Y += 0.125; }},
        {"resource Z", [](FProofSnapshot& S) { S.Resources[0].Position.Z += 0.125; }},
        {"resource remaining", [](FProofSnapshot& S) { ++S.Resources[0].Remaining; }},
        {"remove player", [](FProofSnapshot& S) { S.Players.RemoveAt(0); }},
        {"remove unit", [](FProofSnapshot& S) { S.Units.RemoveAt(0); }},
        {"remove resource", [](FProofSnapshot& S) { S.Resources.RemoveAt(0); }}
    };
    for (const auto& Case : Cases)
    {
        FProofSnapshot Changed = Base;
        Case.Apply(Changed);
        Require(Changed.ComputeDigest() != FString(MixedDigest),
            std::string("digest omitted mutation: ") + Case.Name);
        RequireDigest(Base.ComputeDigest(), MixedDigest, "mutations leave original intact");
    }

    FProofSnapshot NoRally = Base;
    NoRally.Units[1].bHasRallyPoint = false;
    NoRally.Units[1].RallyPoint = FVector::ZeroVector;
    const FString NoRallyDigest = NoRally.ComputeDigest();
    FProofSnapshot EnabledZero = NoRally;
    EnabledZero.Units[1].bHasRallyPoint = true;
    Require(EnabledZero.ComputeDigest() != NoRallyDigest,
        "enabled zero rally must be committed");
    for (int32 Axis = 0; Axis < 3; ++Axis)
    {
        FProofSnapshot DisabledNonzero = NoRally;
        DisabledNonzero.Units[1].RallyPoint[Axis] = 0.125;
        Require(DisabledNonzero.ComputeDigest() != NoRallyDigest,
            "disabled nonzero rally coordinate must be committed");
    }
}
} // namespace

int main(int Argc, char** Argv)
{
    try
    {
        // The portable FString uses printf. Pin the test's numeric formatting
        // rather than claim locale/Unicode equivalence with native Unreal.
        Require(std::setlocale(LC_NUMERIC, "C") != nullptr, "cannot set C numeric locale");
        const std::string Mode = Argc == 2 ? Argv[1] : "";
        if (Argc > 2 || (Mode != "" && Mode != "--snapshots-only" && Mode != "--disable-md5"))
            throw std::invalid_argument("usage: snapshot_digest_bridge_tests [--snapshots-only|--disable-md5]");
        if (Mode == "--disable-md5")
        {
            // This fresh process changes only its own default context. No
            // provider install/unload, system configuration, or persistent state.
            Require(EVP_set_default_properties(nullptr,
                "provider=redalert_test_provider_does_not_exist") == 1,
                "cannot set test-local MD5-unavailable policy");
            (void)FProofSnapshot{}.ComputeDigest();
            std::cerr << "FAIL: unavailable MD5 unexpectedly produced a digest\n";
            return 2;
        }
        if (Mode != "--snapshots-only")
        {
            TestKnownVectors();
            TestRejectedInputs();
        }
        TestSnapshotGoldensAndOrder();
        TestFieldSensitivity();
        std::cout << "PASS: " << Checks << " checks; real snapshot digest bridge\n";
        return 0;
    }
    catch (const std::exception& Error)
    {
        std::cerr << "FAIL: " << Error.what() << '\n';
        return 1;
    }
}
