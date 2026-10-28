# Inert unit tests: compile only the exact pure helper bytes, never the watchdog,
# native declarations, controller, capability preparation or any stage launcher.
[CmdletBinding()]
param([string]$BaselineSource)
$ErrorActionPreference='Stop'
$sourcePath=if($BaselineSource){$BaselineSource}else{Join-Path $PSScriptRoot '../unreal/Scripts/CommandCardValidation/CommandCardStageSupervisor.cs'}
$source=[IO.File]::ReadAllText($sourcePath)
$using='using System; using System.IO; using System.Diagnostics; using System.Text.Json; using System.Security.Cryptography;'
if($BaselineSource){
    $method=[regex]::Match($source,'public static void ExportReturnedOutcome\(string path,Outcome result\) \{ Save\(path,result\); \}').Value
    if(-not $method){throw 'Exact original export method not found.'}
    $harness=@'
public static class LegacyExportRegression {
    public sealed class Outcome { public bool passed {get {return true;}} }
    static long clock=100; static string receipt;
    static void Save(string path,object value) { receipt=JsonSerializer.Serialize(value); clock=2300; }
    METHOD
    public static void Check() {
        ExportReturnedOutcome("inert",new Outcome());
        if(clock>=2300 && JsonDocument.Parse(receipt).RootElement.GetProperty("passed").GetBoolean())
            throw new Exception("REGRESSION: delayed terminal export retained reusable success at the deadline");
    }
}
'@
    Add-Type -TypeDefinition ($using+$harness.Replace('METHOD',$method))
    [LegacyExportRegression]::Check()
    throw 'Baseline unexpectedly passed the delayed-export regression.'
}
$helper=[regex]::Match($source,'(?s)// BEGIN PURE TERMINAL EXPORT HELPER\r?\n(.*?)// END PURE TERMINAL EXPORT HELPER').Groups[1].Value
if(-not $helper -or $helper.Contains('DllImport')){throw 'Exact inert helper section required.'}
$harness=@'
public static class TerminalExportRegression {
    static int checks;
    static void Check(bool ok,string message) { if(!ok) throw new Exception(message); checks++; }
    public static int Run() {
        string json="{\"passed\":true,\"wholeController\":true}";
        var start=new DateTimeOffset(2026,1,1,0,0,0,TimeSpan.Zero);
        string expiry=start.AddMinutes(20).ToString("o");
        foreach(string mode in new [] {"success","at-deadline","past-deadline","cancelled","lease-lost","late-cancel-probe","write-error"}) {
            long clock=100; bool cancel=false; int writes=0; byte[] saved=null; var now=start;
            bool threw=false; RA2CommandCardTerminalExport.Attestation result=null;
            try {
                result=RA2CommandCardTerminalExport.ExportObserved("inert",json,100,220,10,expiry,"cancel",
                    (path,raw)=> { writes++; saved=raw; clock=200;
                        if(mode=="at-deadline") clock=2300;
                        if(mode=="past-deadline") clock=2301;
                        if(mode=="cancelled") cancel=true;
                        if(mode=="lease-lost") now=start.AddMinutes(20).AddSeconds(-5);
                        if(mode=="write-error") throw new IOException("injected inert write error");
                    },()=>clock,()=>now,path=> { if(mode=="late-cancel-probe" && writes>0) clock=2300; return cancel; });
            } catch(IOException) { threw=true; }
            Check(writes==1,"Expected exactly one provisional write: "+mode);
            using(var document=JsonDocument.Parse(saved)) {
                JsonElement ignored;
                Check(document.RootElement.GetProperty("provisionalTerminal").GetBoolean() && !document.RootElement.TryGetProperty("passed",out ignored),"No on-disk success outcome: "+mode);
            }
            Check(mode=="write-error" ? threw && result==null : !threw && result.passed==(mode=="success"),"Delayed/cancelled/error export accepted: "+mode);
            if(mode=="success") {
                Check(result.completedQpc==200 && result.terminalSha256==Convert.ToHexString(SHA256.HashData(saved)).ToLowerInvariant(),"Post-write clock/exact bytes not attested");
            }
        }
        foreach(string mode in new [] {"expired-before-write","cancel-before-write","lease-before-write"}) {
            int writes=0;
            var result=RA2CommandCardTerminalExport.ExportObserved("inert",json,100,220,10,expiry,"cancel",
                (path,raw)=>writes++,()=>mode=="expired-before-write"?2300:100,
                ()=>mode=="lease-before-write"?start.AddMinutes(20).AddSeconds(-5):start,path=>mode=="cancel-before-write");
            Check(writes==0 && !result.passed && !result.completed,"Pre-export rejection must not write: "+mode);
        }
        return checks;
    }
}
'@
Add-Type -TypeDefinition ($using+$helper+$harness)
$count=[TerminalExportRegression]::Run()
[ordered]@{passed=$true;assertions=$count;helperSha256=[Convert]::ToHexString([Security.Cryptography.SHA256]::HashData([Text.Encoding]::UTF8.GetBytes($helper))).ToLowerInvariant();sourceSha256=(Get-FileHash -LiteralPath $sourcePath -Algorithm SHA256).Hash.ToLowerInvariant();nativeWatchdogLoaded=$false;stageLaunched=$false;engineLaunched=$false}|ConvertTo-Json
