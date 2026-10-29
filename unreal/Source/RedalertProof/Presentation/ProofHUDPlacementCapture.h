#pragma once

#include "CoreMinimal.h"
#include "Containers/StringConv.h"
#include "Dom/JsonObject.h"
#include "Presentation/ProofMarkerLayout.h"
#include "Presentation/ProofPrimaryLabelLayout.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"

/** Capture-only observer. Never participates in placement or drawing decisions. */
class FProofHUDPlacementCapture
{
public:
    static constexpr int32 MaxCaptureRows = 7;
    static constexpr int32 MaxEntries = 256;
    static constexpr int32 MaxUtf8Bytes = 65536;

    FProofHUDPlacementCapture(bool bCapture, int32 InCaptureOrdinal)
        : bEnabled(bCapture), CaptureOrdinal(InCaptureOrdinal) {}

    void Configure(const FVector2D& InViewport, double InMargin, double InGap, double InDisplacement)
    {
        if (!bEnabled) return;
        bConfigured = true;
        Viewport = InViewport; Margin = InMargin; Gap = InGap; Displacement = InDisplacement;
    }

    void Source(int32 Token, const FBox2D& Bounds, const TCHAR* Kind, int32 Id)
    {
        if (!AddEntry()) return;
        const TSharedRef<FJsonObject> Row = MakeShared<FJsonObject>();
        Row->SetNumberField(TEXT("token"), Token);
        Row->SetStringField(TEXT("kind"), Kind);
        Row->SetNumberField(TEXT("id"), Id);
        Row->SetObjectField(TEXT("bounds"), Box(Bounds));
        Sources.Add(MakeShared<FJsonValueObject>(Row));
    }

    void Unit(int32 Id, int32 Owner, int32 Team, int32 Kind, double HP, bool bSelected,
        int32 Priority, const FVector2D& Center, double Radius, const FBox2D& Bounds, int32 Token)
    {
        if (!AddEntry()) return;
        const TSharedRef<FJsonObject> Row = MakeShared<FJsonObject>();
        Row->SetStringField(TEXT("kind"), TEXT("unit"));
        Row->SetNumberField(TEXT("id"), Id); Row->SetNumberField(TEXT("owner"), Owner);
        Row->SetNumberField(TEXT("team"), Team); Row->SetNumberField(TEXT("unitKind"), Kind);
        Row->SetField(TEXT("hp"), Number(HP));
        Row->SetBoolField(TEXT("selected"), bSelected); Row->SetBoolField(TEXT("primary"), Priority == 0);
        Row->SetNumberField(TEXT("priority"), Priority); Row->SetNumberField(TEXT("sourceToken"), Token);
        Row->SetArrayField(TEXT("center"), Point(Center)); Row->SetField(TEXT("radius"), Number(Radius));
        Row->SetObjectField(TEXT("bounds"), Box(Bounds));
        Glyphs.Add(MakeShared<FJsonValueObject>(Row));
    }

    void Resource(int32 Id, int32 Remaining, const FVector2D& Center, const FBox2D& Bounds, int32 Token)
    {
        if (!AddEntry()) return;
        const TSharedRef<FJsonObject> Row = MakeShared<FJsonObject>();
        Row->SetStringField(TEXT("kind"), TEXT("resource")); Row->SetNumberField(TEXT("id"), Id);
        Row->SetNumberField(TEXT("remaining"), Remaining); Row->SetNumberField(TEXT("sourceToken"), Token);
        Row->SetArrayField(TEXT("center"), Point(Center)); Row->SetObjectField(TEXT("bounds"), Box(Bounds));
        Glyphs.Add(MakeShared<FJsonValueObject>(Row));
    }

    void Placement(const TCHAR* Kind, int32 Id, int32 Token, int32 Priority, const FString& Label,
        const FVector2D& Preferred, const FVector2D& Size, bool bPlaced, const FBox2D& Bounds,
        const FVector2D& LeaderStart, const FVector2D& LeaderEnd,
        int32 PassesAttempted = 1, bool bPrimaryAlternates = false)
    {
        if (!bEnabled) return;
        const int32 Ordinal = ++DrawOrdinal;
        if (!AddEntry()) return;
        const TSharedRef<FJsonObject> Row = MakeShared<FJsonObject>();
        Row->SetNumberField(TEXT("drawOrdinal"), Ordinal); Row->SetStringField(TEXT("kind"), Kind);
        Row->SetNumberField(TEXT("id"), Id); Row->SetNumberField(TEXT("sourceToken"), Token);
        Row->SetNumberField(TEXT("priority"), Priority); Row->SetStringField(TEXT("label"), Label.Left(64));
        Row->SetArrayField(TEXT("preferred"), Point(Preferred)); Row->SetArrayField(TEXT("size"), Point(Size));
        Row->SetBoolField(TEXT("placed"), bPlaced);
        Row->SetStringField(TEXT("anchorStrategy"), bPrimaryAlternates ? TEXT("above-then-below") : TEXT("single"));
        Row->SetNumberField(TEXT("passesAttempted"), PassesAttempted);
        Row->SetNumberField(TEXT("maxCandidatesPerPlacement"), bPrimaryAlternates
            ? FProofPrimaryLabelLayout::MaxCandidateCount : FProofMarkerLayout::MaxCandidateCount);
        if (bPlaced)
        {
            Row->SetObjectField(TEXT("bounds"), Box(Bounds));
            Row->SetArrayField(TEXT("leaderStart"), Point(LeaderStart));
            Row->SetArrayField(TEXT("leaderEnd"), Point(LeaderEnd));
        }
        else
        {
            Row->SetField(TEXT("bounds"), MakeShared<FJsonValueNull>());
            Row->SetField(TEXT("leaderStart"), MakeShared<FJsonValueNull>());
            Row->SetField(TEXT("leaderEnd"), MakeShared<FJsonValueNull>());
        }
        Placements.Add(MakeShared<FJsonValueObject>(Row));
    }

    bool BuildJson(const FString& CaptureName, int32 Epoch, int64 Revision, int32 Phase, FString& Out)
    {
        if (!bEnabled || !bConfigured) return false;
        TSharedRef<FJsonObject> Row = Metadata(CaptureName, Epoch, Revision, Phase);
        Row->SetArrayField(TEXT("viewport"), Point(Viewport));
        Row->SetField(TEXT("margin"), Number(Margin)); Row->SetField(TEXT("gap"), Number(Gap));
        Row->SetField(TEXT("maxDisplacement"), Number(Displacement));
        Row->SetStringField(TEXT("displacementOrigin"), TEXT("per-anchor-preferred"));
        Row->SetNumberField(TEXT("maxCandidatesPerPass"), FProofMarkerLayout::MaxCandidateCount);
        Row->SetNumberField(TEXT("maxCandidatesPerPlacement"), FProofPrimaryLabelLayout::MaxCandidateCount);
        Row->SetArrayField(TEXT("sources"), Sources); Row->SetArrayField(TEXT("glyphs"), Glyphs);
        Row->SetArrayField(TEXT("placements"), Placements);
        Row->SetNumberField(TEXT("entries"), EntryCount); Row->SetNumberField(TEXT("droppedEntries"), DroppedEntries);
        Row->SetNumberField(TEXT("nonFiniteValues"), NonFiniteValues);
        Row->SetBoolField(TEXT("complete"), DroppedEntries == 0 && NonFiniteValues == 0);
        if (!Serialize(Row, Out))
        {
            Row = Metadata(CaptureName, Epoch, Revision, Phase);
            Row->SetBoolField(TEXT("complete"), false); Row->SetStringField(TEXT("reason"), TEXT("serialization"));
            return Serialize(Row, Out);
        }
        const FTCHARToUTF8 Utf8(*Out);
        if (Utf8.Length() > MaxUtf8Bytes)
        {
            const int32 OriginalBytes = Utf8.Length();
            Row = Metadata(CaptureName, Epoch, Revision, Phase);
            Row->SetBoolField(TEXT("complete"), false); Row->SetStringField(TEXT("reason"), TEXT("byte-limit"));
            Row->SetNumberField(TEXT("originalUtf8Bytes"), OriginalBytes);
            return Serialize(Row, Out);
        }
        return true;
    }

private:
    bool AddEntry()
    {
        if (!bEnabled) return false;
        if (EntryCount >= MaxEntries) { ++DroppedEntries; return false; }
        ++EntryCount; return true;
    }

    TSharedPtr<FJsonValue> Number(double Value)
    {
        if (FMath::IsFinite(Value)) return MakeShared<FJsonValueNumber>(Value);
        ++NonFiniteValues; return MakeShared<FJsonValueNull>();
    }

    TArray<TSharedPtr<FJsonValue>> Point(const FVector2D& Value)
    {
        return {Number(Value.X), Number(Value.Y)};
    }

    TSharedRef<FJsonObject> Box(const FBox2D& Bounds)
    {
        const TSharedRef<FJsonObject> Row = MakeShared<FJsonObject>();
        Row->SetBoolField(TEXT("valid"), !!Bounds.bIsValid);
        Row->SetArrayField(TEXT("min"), Point(Bounds.Min)); Row->SetArrayField(TEXT("max"), Point(Bounds.Max));
        return Row;
    }

    TSharedRef<FJsonObject> Metadata(const FString& Name, int32 Epoch, int64 Revision, int32 Phase) const
    {
        const TSharedRef<FJsonObject> Row = MakeShared<FJsonObject>();
        Row->SetStringField(TEXT("schema"), TEXT("RedalertProof.HUDPlacement.v2"));
        Row->SetStringField(TEXT("glyphScope"), TEXT("reserved-visible-only"));
        Row->SetStringField(TEXT("name"), Name.Left(64)); Row->SetNumberField(TEXT("captureOrdinal"), CaptureOrdinal);
        Row->SetNumberField(TEXT("epoch"), Epoch);
        Row->SetStringField(TEXT("revision"), FString::Printf(TEXT("%lld"), Revision));
        Row->SetNumberField(TEXT("phase"), Phase); Row->SetNumberField(TEXT("rowLimit"), MaxCaptureRows);
        Row->SetNumberField(TEXT("entryLimit"), MaxEntries);
        Row->SetNumberField(TEXT("utf8ByteLimit"), MaxUtf8Bytes); Row->SetBoolField(TEXT("diagnosticOnly"), true);
        return Row;
    }

    static bool Serialize(const TSharedRef<FJsonObject>& Row, FString& Out)
    {
        Out.Reset();
        const TSharedRef<TJsonWriter<TCHAR, TCondensedJsonPrintPolicy<TCHAR>>> Writer =
            TJsonWriterFactory<TCHAR, TCondensedJsonPrintPolicy<TCHAR>>::Create(&Out);
        return FJsonSerializer::Serialize(Row, Writer);
    }

    bool bEnabled = false, bConfigured = false;
    int32 CaptureOrdinal = 0, EntryCount = 0, DroppedEntries = 0, NonFiniteValues = 0, DrawOrdinal = 0;
    FVector2D Viewport = FVector2D::ZeroVector;
    double Margin = 0.0, Gap = 0.0, Displacement = 0.0;
    TArray<TSharedPtr<FJsonValue>> Sources, Glyphs, Placements;
};
