// Test-only ASCII snapshot-digest bridge for the Windows portable UE subset.
// Use the OS default provider; fail if MD5 is unavailable, without a fallback
// provider or any system/provider configuration changes. This is not Unreal.
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include "Misc/SecureHash.h"
// Windows also defines TEXT; this translation unit uses the portable char type.
#undef TEXT
#include <windows.h>
#include <bcrypt.h>
#include <array>
#include <stdexcept>

namespace
{
void RequireHashStatus(NTSTATUS Status)
{
    if (Status < 0) { throw std::runtime_error("Windows portable MD5 provider failed"); }
}

struct FPortableHashHandles
{
    BCRYPT_ALG_HANDLE Algorithm = nullptr;
    BCRYPT_HASH_HANDLE Hash = nullptr;
    ~FPortableHashHandles()
    {
        if (Hash) { BCryptDestroyHash(Hash); }
        if (Algorithm) { BCryptCloseAlgorithmProvider(Algorithm, 0); }
    }
};
}

FString FMD5::HashAnsiString(const TCHAR* Input)
{
    static_assert(sizeof(TCHAR) == 1, "Requires the portable ASCII char subset");
    if (!Input) { throw std::invalid_argument("Null portable MD5 input"); }
    ULONG Length = 0;
    while (Input[Length])
    {
        if (static_cast<unsigned char>(Input[Length]) > 0x7f || Length >= 1024 * 1024)
        {
            throw std::invalid_argument("Portable MD5 requires bounded ASCII input");
        }
        ++Length;
    }
    // The hash object must outlive its handle, including failure unwinding.
    std::vector<UCHAR> Object;
    FPortableHashHandles Handles;
    RequireHashStatus(BCryptOpenAlgorithmProvider(&Handles.Algorithm, BCRYPT_MD5_ALGORITHM, nullptr, 0));
    ULONG ObjectBytes = 0, ResultBytes = 0, DigestBytes = 0;
    RequireHashStatus(BCryptGetProperty(Handles.Algorithm, BCRYPT_OBJECT_LENGTH,
        reinterpret_cast<PUCHAR>(&ObjectBytes), sizeof(ObjectBytes), &ResultBytes, 0));
    if (ResultBytes != sizeof(ObjectBytes) || ObjectBytes == 0 || ObjectBytes > 1024 * 1024)
    {
        throw std::runtime_error("Invalid portable MD5 object size");
    }
    RequireHashStatus(BCryptGetProperty(Handles.Algorithm, BCRYPT_HASH_LENGTH,
        reinterpret_cast<PUCHAR>(&DigestBytes), sizeof(DigestBytes), &ResultBytes, 0));
    if (ResultBytes != sizeof(DigestBytes) || DigestBytes != 16)
    {
        throw std::runtime_error("Invalid portable MD5 digest size");
    }
    Object.resize(ObjectBytes);
    RequireHashStatus(BCryptCreateHash(Handles.Algorithm, &Handles.Hash,
        Object.data(), ObjectBytes, nullptr, 0, 0));
    RequireHashStatus(BCryptHashData(Handles.Hash,
        reinterpret_cast<PUCHAR>(const_cast<TCHAR*>(Input)), Length, 0));
    std::array<UCHAR, 16> Digest{};
    RequireHashStatus(BCryptFinishHash(Handles.Hash, Digest.data(), static_cast<ULONG>(Digest.size()), 0));
    constexpr char Hex[] = "0123456789abcdef";
    std::string Text;
    Text.reserve(32);
    for (UCHAR Byte : Digest)
    {
        Text.push_back(Hex[Byte >> 4]);
        Text.push_back(Hex[Byte & 15]);
    }
    return FString(Text);
}
