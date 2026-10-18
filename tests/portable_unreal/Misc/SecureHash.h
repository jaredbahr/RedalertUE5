#pragma once
#include "CoreMinimal.h"
// Deliberately undefined. Link-time section removal drops the unused digest
// routine. If a portable test starts relying on MD5, linking must fail.
struct FMD5 { static FString HashAnsiString(const TCHAR*); };
