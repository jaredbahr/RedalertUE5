#!/usr/bin/env python3
"""Compile source-bound focus-recording regressions with explicit Unreal mocks.

No engine, OS focus event, input binding, or physical-input claim is made.
Requires Python 3 and a C++17 compiler (CXX, default g++); installs nothing.
"""
import argparse
import os
from pathlib import Path
import re
import shlex
import subprocess
import tempfile


def method(source, name):
    start = source.index("void AProofRTSPawn::" + name + "(")
    end = source.index("{", start) + 1
    depth = 1
    while depth:
        depth += (source[end] == "{") - (source[end] == "}")
        end += 1
    return source[start:end]


ADAPTERS = r'''
#include <cstdarg>
#include <cstdio>
#include <cstdint>
#include <iostream>
#include <limits>
#include <string>
#include <vector>
using int32 = std::int32_t;
using TCHAR = char;
constexpr int32 MAX_int32 = std::numeric_limits<int32>::max();
#define TEXT(x) x
struct FString {
    std::string V;
    FString(const char* S = "") : V(S) {}
    explicit FString(std::string S) : V(S) {}
    const char* operator*() const { return V.c_str(); }
    FString Left(int N) const { return FString(V.substr(0, N)); }
    static FString Printf(const char* F, ...) {
        va_list A; va_start(A, F); char B[2048];
        vsnprintf(B, sizeof(B), F, A); va_end(A); return FString(B);
    }
};
std::vector<std::string> Logs;
void Log(const char* F, ...) {
    va_list A; va_start(A, F); char B[2048];
    vsnprintf(B, sizeof(B), F, A); va_end(A); Logs.emplace_back(B);
}
#define UE_LOG(Category, Level, Format, ...) Log(Format, ##__VA_ARGS__)
struct FViewport {
    bool Focus = false, Foreground = false;
    bool HasFocus() const { return Focus; }
    bool IsForegroundWindow() const { return Foreground; }
};
struct UGameViewportClient { FViewport* Viewport = nullptr; };
struct World {
    UGameViewportClient* Client = nullptr;
    UGameViewportClient* GetGameViewport() { return Client; }
};
struct AProofRTSPawn {
    World* W = nullptr;
    World* GetWorld() { return W; }
    struct Gesture { int Canceled = 0; void Cancel() { ++Canceled; } } TypeGesture;
    bool bViewportFocused = false, bViewportForeground = false, bFocusObserved = false;
    int32 FocusTransitions = 0, ManualInputLogs = 0;
    FString LastManualAction;
    void ObserveViewportFocus();
    void RecordManualInput(const TCHAR*, int32&, const FString&);
};
int Checks = 0, Failures = 0;
void Check(bool Pass, const char* Name) {
    ++Checks;
    if (!Pass) { ++Failures; std::cerr << "FAIL " << Name << '\n'; }
}
int LogCount(const std::string& Needle) {
    int Count = 0;
    for (const auto& L : Logs) if (L.find(Needle) != std::string::npos) ++Count;
    return Count;
}
std::string InputLog() {
    for (auto I = Logs.rbegin(); I != Logs.rend(); ++I)
        if (I->find("PROOF_MANUAL_INPUT key=") == 0) return *I;
    return "";
}
struct Fixture {
    FViewport V;
    UGameViewportClient C{&V};
    World W{&C};
    AProofRTSPawn P;
    int32 Count = 0;
    Fixture() { P.W = &W; Logs.clear(); }
    void Set(int Bits) { V.Focus = (Bits & 1) != 0; V.Foreground = (Bits & 2) != 0; }
    void Record(const char* Key = "RMB") { P.RecordManualInput(Key, Count, FString("observed")); }
};
'''

TESTS = r'''
int main() {
    // Every cached/current flag combination, across all keys routed to this recorder.
    for (const char* Key : {"LMB", "RMB", "P", "S", "T", "Group", "WheelUp", "WheelDown", "N", "R", "F"}) {
        for (int Before = 0; Before < 4; ++Before) for (int After = 0; After < 4; ++After) {
            Fixture F; F.Set(Before); F.P.ObserveViewportFocus();
            const int Canceled = F.P.TypeGesture.Canceled;
            F.Set(After); F.Record(Key);
            const std::string Flags = "viewportFocus=" + std::to_string(After & 1)
                + " foreground=" + std::to_string((After >> 1) & 1);
            Check(InputLog().find(Flags) != std::string::npos, "input row samples current viewport flags");
            Check(F.P.bViewportFocused == F.V.Focus && F.P.bViewportForeground == F.V.Foreground,
                "cached observation matches current viewport");
            Check(F.P.bFocusObserved, "valid viewport is observed");
            Check(F.P.FocusTransitions == (Before == After ? 1 : 2), "only changed observation increments transitions");
            Check(F.P.TypeGesture.Canceled == Canceled + (After == 3 ? 0 : 1), "existing focus cancellation policy reused");
            Check(F.Count == 1 && F.P.ManualInputLogs == 1, "exactly one native counter and one input row");
            Check(InputLog().find("physicalInput=unverified") != std::string::npos, "no physical verification claim");
        }
    }
    for (int Bits = 0; Bits < 4; ++Bits) {
        Fixture F; F.Set(Bits); F.Record();
        Check(F.P.bFocusObserved && F.P.FocusTransitions == 1, "first record observes viewport before first tick");
        Check(F.P.bViewportFocused == F.V.Focus && F.P.bViewportForeground == F.V.Foreground, "first record current flags");
    }
    for (int Missing = 0; Missing < 3; ++Missing) {
        Fixture F; F.Set(3); F.P.ObserveViewportFocus();
        if (Missing == 0) F.P.W = nullptr;
        if (Missing == 1) F.W.Client = nullptr;
        if (Missing == 2) F.C.Viewport = nullptr;
        F.Record();
        Check(!F.P.bFocusObserved && !F.P.bViewportFocused && !F.P.bViewportForeground, "missing viewport clears stale focus");
        Check(InputLog().find("viewportFocus=0 foreground=0") != std::string::npos, "missing viewport logs false flags");
        Check(F.P.FocusTransitions == 1 && F.P.TypeGesture.Canceled == 1, "missing viewport cancels without a false transition");
        Check(F.Count == 1 && F.P.ManualInputLogs == 1, "missing viewport still records input counter");
        F.P.W = &F.W; F.W.Client = &F.C; F.C.Viewport = &F.V; F.Record();
        Check(F.P.bFocusObserved && F.P.FocusTransitions == 2, "viewport restoration is observed");
        Check(InputLog().find("viewportFocus=1 foreground=1") != std::string::npos, "restored viewport logs current flags");
    }
    {
        Fixture F; F.Set(3); F.P.ObserveViewportFocus(); F.Record("T");
        Check(F.P.FocusTransitions == 1 && F.P.TypeGesture.Canceled == 0, "existing T pre-refresh remains idempotent");
        Check(LogCount("PROOF_MANUAL_FOCUS transition=") == 1, "T double observation adds no duplicate transition");
    }
    {
        Fixture F;
        for (int N = 0; N < 40; ++N) { F.Set(N % 2 ? 3 : 0); F.Record(); }
        Check(F.Count == 40 && F.P.FocusTransitions == 40, "counters continue after detail limits");
        Check(F.P.ManualInputLogs == 24, "input detail cap remains 24");
        Check(LogCount("PROOF_MANUAL_INPUT key=") == 24, "exactly 24 input details");
        Check(LogCount("furtherEvents=suppressed countersRemainVisible=1") == 1, "one input suppression marker");
        Check(LogCount("PROOF_MANUAL_FOCUS transition=") == 8, "exactly eight focus details");
        Check(LogCount("furtherTransitions=suppressed") == 1, "one focus suppression marker");
        Check(F.P.bViewportFocused && F.P.bViewportForeground, "focus stays fresh after log suppression");
        F.Set(0); F.Record();
        Check(!F.P.bViewportFocused && !F.P.bViewportForeground && F.Count == 41,
            "suppressed input still updates observation and counter");
    }
    {
        Fixture F; F.Count = MAX_int32 - 1; F.P.FocusTransitions = MAX_int32 - 1;
        F.Set(3); F.Record(); F.Set(0); F.Record();
        Check(F.Count == MAX_int32 && F.P.FocusTransitions == MAX_int32, "native and focus counters saturate safely");
        Check(!F.P.bViewportFocused && !F.P.bViewportForeground, "saturated counter does not freeze focus flags");
        Check(LogCount("PROOF_MANUAL_FOCUS") == 0, "saturation does not reopen focus logging");
    }
    {
        Fixture F; F.Set(3); const std::string Long(100, 'x');
        F.P.RecordManualInput("RMB", F.Count, FString(Long));
        Check(F.P.LastManualAction.V == "RMB " + Long.substr(0, 56), "HUD outcome bound remains 56");
        Check(InputLog().find("outcome=" + Long.substr(0, 80) + " viewportFocus=") != std::string::npos,
            "input outcome bound remains 80");
        Check(F.Count == 1 && F.P.ManualInputLogs == 1, "long outcome is still one input");
    }
    std::cout << "source-bound manual focus checks=" << Checks << " failures=" << Failures
        << " Unreal=0 OSFocus=0 InputBinding=0 PhysicalInput=0\n";
    return Failures ? 1 : 0;
}
'''


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--source", type=Path, default=Path(__file__).resolve().parents[1]
                        / "Source/RedalertProof/Presentation/ProofRTSPawn.cpp")
    parser.add_argument("--mode", choices=("normal", "optimized", "sanitized"), default="normal")
    args = parser.parse_args()
    source = args.source.read_text(encoding="utf-8")
    source_keys = set(re.findall(r'RecordManualInput\(TEXT\("([^"]+)"\)', source))
    tested_keys = set(re.findall(r'"([^"]+)"',
                               re.search(r'for \(const char\* Key : \{([^}]+)\}', TESTS).group(1)))
    if source_keys != tested_keys:
        raise RuntimeError("Recorder key coverage changed: source=" + str(sorted(source_keys))
                           + " tested=" + str(sorted(tested_keys)))
    cpp = ADAPTERS + "\n" + method(source, "ObserveViewportFocus") + "\n" \
        + method(source, "RecordManualInput") + "\n" + TESTS
    flags = {"normal": [], "optimized": ["-O2", "-DNDEBUG"],
             "sanitized": ["-g", "-fsanitize=address,undefined", "-fno-omit-frame-pointer"]}[args.mode]
    with tempfile.TemporaryDirectory(prefix="proof-manual-focus-") as tmp:
        path = Path(tmp)
        (path / "test.cpp").write_text(cpp, encoding="utf-8")
        exe = path / ("test.exe" if os.name == "nt" else "test")
        subprocess.run(shlex.split(os.environ.get("CXX", "g++"))
                       + ["-std=c++17", "-Wall", "-Wextra", "-Werror", *flags,
                          str(path / "test.cpp"), "-o", str(exe)], check=True, timeout=60)
        result = subprocess.run([str(exe)], timeout=30)
        return result.returncode


if __name__ == "__main__":
    raise SystemExit(main())
