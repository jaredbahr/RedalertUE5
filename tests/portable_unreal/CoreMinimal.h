#pragma once

// Test-only UE subset. This permits the unmodified production model/planner and
// native regression bodies to run under a portable compiler. It is not Unreal.
#include <algorithm>
#include <cmath>
#include <cstdarg>
#include <cstdint>
#include <cstdio>
#include <limits>
#include <string>
#include <type_traits>
#include <utility>
#include <vector>

using int32 = std::int32_t;
using int64 = std::int64_t;
using uint8 = std::uint8_t;
using TCHAR = char;
constexpr int32 INDEX_NONE = -1;
constexpr int32 MAX_int32 = std::numeric_limits<int32>::max();
constexpr double UE_SMALL_NUMBER = 1.e-8;
#define TEXT(Value) Value
#define UENUM(...)
#define USTRUCT(...)
#define UPROPERTY(...)
#define GENERATED_BODY(...)
#define REDALERTPROOF_API

class FString
{
    std::string Value;
public:
    FString() = default;
    FString(const char* In) : Value(In) {}
    explicit FString(std::string In) : Value(std::move(In)) {}
    const char* operator*() const { return Value.c_str(); }
    FString& operator+=(const FString& Other) { Value += Other.Value; return *this; }
    bool operator==(const FString& Other) const { return Value == Other.Value; }
    static FString Printf(const char* Format, ...)
    {
        va_list Args, Copy;
        va_start(Args, Format);
        va_copy(Copy, Args);
        const int Count = std::vsnprintf(nullptr, 0, Format, Copy);
        va_end(Copy);
        std::vector<char> Buffer(static_cast<std::size_t>(Count) + 1);
        std::vsnprintf(Buffer.data(), Buffer.size(), Format, Args);
        va_end(Args);
        return FString(Buffer.data());
    }
};

struct FVector
{
    double X = 0, Y = 0, Z = 0;
    FVector() = default;
    FVector(double InX, double InY, double InZ) : X(InX), Y(InY), Z(InZ) {}
    static const FVector ZeroVector;
    FVector operator+(const FVector& B) const { return {X + B.X, Y + B.Y, Z + B.Z}; }
    FVector operator-(const FVector& B) const { return {X - B.X, Y - B.Y, Z - B.Z}; }
    FVector operator*(double Scale) const { return {X * Scale, Y * Scale, Z * Scale}; }
    bool operator==(const FVector& B) const { return X == B.X && Y == B.Y && Z == B.Z; }
    bool operator!=(const FVector& B) const { return !(*this == B); }
    double& operator[](int32 Axis) { return Axis == 0 ? X : (Axis == 1 ? Y : Z); }
    double SizeSquared() const { return X * X + Y * Y + Z * Z; }
    double Size() const { return std::sqrt(SizeSquared()); }
    bool Equals(const FVector& B, double Tolerance = 1.e-4) const
    { return std::abs(X - B.X) <= Tolerance && std::abs(Y - B.Y) <= Tolerance && std::abs(Z - B.Z) <= Tolerance; }
    FString ToString() const { return FString::Printf("X=%.9g Y=%.9g Z=%.9g", X, Y, Z); }
    static double DistSquared(const FVector& A, const FVector& B) { return (A - B).SizeSquared(); }
    static double Dist(const FVector& A, const FVector& B) { return (A - B).Size(); }
};
inline const FVector FVector::ZeroVector{};

struct FMath
{
    template<typename T> static bool IsFinite(T Value) { return std::isfinite(Value); }
    template<typename T> static T Abs(T Value) { return std::abs(Value); }
    template<typename T> static T Min(T A, T B) { return std::min(A, B); }
    template<typename T> static T Max(T A, T B) { return std::max(A, B); }
    template<typename T> static T Square(T Value) { return Value * Value; }
};

template<typename T> class TArray
{
    std::vector<T> Data;
public:
    int32 Num() const { return static_cast<int32>(Data.size()); }
    void Add(const T& Item) { Data.push_back(Item); }
    void Reset() { Data.clear(); }
    T& operator[](int32 Index) { return Data.at(static_cast<std::size_t>(Index)); }
    const T& operator[](int32 Index) const { return Data.at(static_cast<std::size_t>(Index)); }
    T& Last() { return Data.back(); }
    auto begin() { return Data.begin(); }
    auto end() { return Data.end(); }
    auto begin() const { return Data.begin(); }
    auto end() const { return Data.end(); }
    void Swap(int32 A, int32 B) { std::swap((*this)[A], (*this)[B]); }
    template<typename P> T* FindByPredicate(P Pred)
    { for (T& Item : Data) if (Pred(Item)) return &Item; return nullptr; }
    template<typename P> const T* FindByPredicate(P Pred) const
    { for (const T& Item : Data) if (Pred(Item)) return &Item; return nullptr; }
    template<typename P> bool ContainsByPredicate(P Pred) const { return FindByPredicate(Pred) != nullptr; }
    template<typename P> void RemoveAll(P Pred)
    { Data.erase(std::remove_if(Data.begin(), Data.end(), Pred), Data.end()); }
    template<typename P> void Sort(P Pred)
    {
        std::sort(Data.begin(), Data.end(), [&Pred](const T& A, const T& B)
        { if constexpr (std::is_pointer_v<T>) return Pred(*A, *B); else return Pred(A, B); });
    }
};

template<typename K, typename V> struct TPair { K Key; V Value; };
template<typename K, typename V> class TMap
{
    std::vector<TPair<K, V>> Data;
public:
    V* Find(K Key) { for (auto& Item : Data) if (Item.Key == Key) return &Item.Value; return nullptr; }
    const V* Find(K Key) const { for (const auto& Item : Data) if (Item.Key == Key) return &Item.Value; return nullptr; }
    bool Contains(K Key) const { return Find(Key) != nullptr; }
    V& FindChecked(K Key) { return *Find(Key); }
    V& FindOrAdd(K Key) { if (V* Value = Find(Key)) return *Value; Data.push_back({Key, V{}}); return Data.back().Value; }
    void Add(K Key, const V& Value) { FindOrAdd(Key) = Value; }
    void Remove(K Key) { Data.erase(std::remove_if(Data.begin(), Data.end(), [Key](const auto& I) { return I.Key == Key; }), Data.end()); }
    void Reset() { Data.clear(); }
    auto begin() { return Data.begin(); }
    auto end() { return Data.end(); }
    class Iterator
    {
        TMap& Map;
        std::size_t Index = 0;
        bool Removed = false;
    public:
        explicit Iterator(TMap& In) : Map(In) {}
        explicit operator bool() const { return Index < Map.Data.size(); }
        void operator++() { if (!Removed) ++Index; Removed = false; }
        K Key() const { return Map.Data[Index].Key; }
        V& Value() { return Map.Data[Index].Value; }
        void RemoveCurrent() { Map.Data.erase(Map.Data.begin() + static_cast<std::ptrdiff_t>(Index)); Removed = true; }
    };
    Iterator CreateIterator() { return Iterator(*this); }
};

template<typename T> class TSet
{
    std::vector<T> Data;
public:
    void Add(T Value) { if (!Contains(Value)) Data.push_back(Value); }
    bool Contains(T Value) const { return std::find(Data.begin(), Data.end(), Value) != Data.end(); }
    int32 Num() const { return static_cast<int32>(Data.size()); }
};
