// Test-only definition for the deliberately undefined portable FMD5 declaration.
// This is an ASCII/C-string bridge, not Unreal's implementation or a crypto API.
#include "Misc/SecureHash.h"

#include <openssl/evp.h>

#include <array>
#include <memory>
#include <stdexcept>
#include <string>

FString FMD5::HashAnsiString(const TCHAR* Input)
{
    static_assert(sizeof(TCHAR) == 1, "This test bridge requires the portable char TCHAR.");
    if (Input == nullptr)
        throw std::invalid_argument("portable MD5 bridge: null input is unsupported");

    std::size_t Length = 0;
    while (Input[Length] != '\0')
    {
        if (static_cast<unsigned char>(Input[Length]) > 0x7f)
            throw std::invalid_argument("portable MD5 bridge: non-ASCII input is unsupported");
        ++Length;
    }

    // Respect the process's provider policy. Never load an alternate provider,
    // change its properties, bypass FIPS policy, or substitute a dummy digest.
    const std::unique_ptr<EVP_MD, decltype(&EVP_MD_free)> Algorithm(
        EVP_MD_fetch(nullptr, "MD5", nullptr), &EVP_MD_free);
    if (!Algorithm)
        throw std::runtime_error(
            "portable MD5 bridge: EVP_MD_fetch(MD5) failed; MD5 unavailable or disabled");
    if (EVP_MD_get_size(Algorithm.get()) != 16)
        throw std::runtime_error("portable MD5 bridge: unexpected MD5 digest size");

    const std::unique_ptr<EVP_MD_CTX, decltype(&EVP_MD_CTX_free)> Context(
        EVP_MD_CTX_new(), &EVP_MD_CTX_free);
    if (!Context)
        throw std::runtime_error("portable MD5 bridge: EVP_MD_CTX_new failed");
    if (EVP_DigestInit_ex2(Context.get(), Algorithm.get(), nullptr) != 1)
        throw std::runtime_error("portable MD5 bridge: EVP_DigestInit_ex2 failed");
    if (EVP_DigestUpdate(Context.get(), Input, Length) != 1)
        throw std::runtime_error("portable MD5 bridge: EVP_DigestUpdate failed");

    std::array<unsigned char, EVP_MAX_MD_SIZE> Bytes{};
    unsigned int Size = 0;
    if (EVP_DigestFinal_ex(Context.get(), Bytes.data(), &Size) != 1)
        throw std::runtime_error("portable MD5 bridge: EVP_DigestFinal_ex failed");
    if (Size != 16)
        throw std::runtime_error("portable MD5 bridge: unexpected final MD5 digest size");

    constexpr char Hex[] = "0123456789abcdef";
    std::string Result(32, '0');
    for (std::size_t I = 0; I < 16; ++I)
    {
        Result[I * 2] = Hex[Bytes[I] >> 4];
        Result[I * 2 + 1] = Hex[Bytes[I] & 0x0f];
    }
    return FString(Result);
}
