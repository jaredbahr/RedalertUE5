// PR72-only owned-job primitive; derived from reviewed source SHA256
// 0ffb82e568a1ae7bce90e751621a2f994fa92a6e7cd13442f4d65aaada1092c6.
// The original PR71 source is unchanged. No PID lookup is used for signalling.
// A positive outcome is only OS supervision, never engine or input acceptance.
using System;
using System.IO;
using System.Text;
using System.Collections;
using System.Collections.Generic;
using System.ComponentModel;
using System.Diagnostics;
using System.Runtime.InteropServices;
using System.Threading;
using System.Threading.Tasks;
using System.Text.Json;
using System.Security.Cryptography;
using Microsoft.Win32.SafeHandles;

public static class RA2CommandCardStageSupervisor
{
    static double Elapsed(long start) { return (Stopwatch.GetTimestamp()-start)/(double)Stopwatch.Frequency; }
    const uint WAIT_OBJECT_0=0, WAIT_TIMEOUT=258, INFINITE=0xffffffff;
    const uint KILL_ON_JOB_CLOSE=0x2000, STARTF_USESTDHANDLES=0x100;
    const uint CREATE_SUSPENDED=4, CREATE_NO_WINDOW=0x08000000;
    const uint EXTENDED_STARTUPINFO_PRESENT=0x80000, CREATE_UNICODE_ENVIRONMENT=0x400;
    const uint NORMAL_PRIORITY_CLASS=0x20;
    const int OUTPUT_CAP=1048576;
    [StructLayout(LayoutKind.Sequential)] struct SA { public int length; public IntPtr descriptor; public int inherit; }
    [StructLayout(LayoutKind.Sequential,CharSet=CharSet.Unicode)] struct SI {
        public int cb; public string reserved,desktop,title;
        public int x,y,xs,ys,xc,yc,fill; public uint flags;
        public short show,reserved2; public IntPtr reservedPtr,stdin,stdout,stderr;
    }
    [StructLayout(LayoutKind.Sequential)] struct SIX { public SI startup; public IntPtr attributes; }
    [StructLayout(LayoutKind.Sequential)] struct PI { public IntPtr process,thread; public uint pid,tid; }
    [StructLayout(LayoutKind.Sequential)] struct BASIC_LIMIT {
        public long processTime,jobTime; public uint flags;
        public UIntPtr minWorking,maxWorking; public uint activeLimit;
        public UIntPtr affinity; public uint priority,scheduling;
    }
    [StructLayout(LayoutKind.Sequential)] struct IO_COUNTERS { public ulong read,write,other,readBytes,writeBytes,otherBytes; }
    [StructLayout(LayoutKind.Sequential)] struct LIMIT {
        public BASIC_LIMIT basic; public IO_COUNTERS io;
        public UIntPtr processMemory,jobMemory,peakProcessMemory,peakJobMemory;
    }
    [StructLayout(LayoutKind.Sequential)] struct ACCOUNTING {
        public long user,kernel,periodUser,periodKernel;
        public uint faults,totalProcesses,activeProcesses,terminatedProcesses;
    }
    [StructLayout(LayoutKind.Sequential)] struct FT { public uint low,high; public ulong Value { get { return ((ulong)high<<32)|low; } } }
    [DllImport("kernel32.dll",SetLastError=true,CharSet=CharSet.Unicode)] static extern IntPtr CreateJobObjectW(IntPtr a,string n);
    [DllImport("kernel32.dll",SetLastError=true)] static extern bool SetInformationJobObject(IntPtr h,int c,ref LIMIT i,uint l);
    [DllImport("kernel32.dll",SetLastError=true)] static extern bool QueryInformationJobObject(IntPtr h,int c,out ACCOUNTING i,uint l,IntPtr r);
    [DllImport("kernel32.dll",SetLastError=true)] static extern bool AssignProcessToJobObject(IntPtr j,IntPtr p);
    [DllImport("kernel32.dll",SetLastError=true)] static extern bool TerminateJobObject(IntPtr j,uint c);
    [DllImport("kernel32.dll",SetLastError=true)] static extern bool TerminateProcess(IntPtr p,uint c);
    [DllImport("kernel32.dll",SetLastError=true)] static extern bool CloseHandle(IntPtr h);
    [DllImport("kernel32.dll",SetLastError=true)] static extern bool CreatePipe(out IntPtr r,out IntPtr w,ref SA s,uint n);
    [DllImport("kernel32.dll",SetLastError=true)] static extern bool SetHandleInformation(IntPtr h,uint m,uint f);
    [DllImport("kernel32.dll",SetLastError=true,CharSet=CharSet.Unicode)] static extern IntPtr CreateFileW(string n,uint a,uint s,ref SA sa,uint c,uint f,IntPtr t);
    [DllImport("kernel32.dll",SetLastError=true,CharSet=CharSet.Unicode)] static extern bool CreateProcessW(string a,StringBuilder c,IntPtr pa,IntPtr ta,bool inherit,uint flags,IntPtr env,string wd,ref SIX si,out PI pi);
    [DllImport("kernel32.dll",SetLastError=true)] static extern bool InitializeProcThreadAttributeList(IntPtr p,int n,uint f,ref IntPtr s);
    [DllImport("kernel32.dll",SetLastError=true)] static extern bool UpdateProcThreadAttribute(IntPtr p,uint f,IntPtr a,IntPtr v,IntPtr s,IntPtr prev,IntPtr ret);
    [DllImport("kernel32.dll")] static extern void DeleteProcThreadAttributeList(IntPtr p);
    [DllImport("kernel32.dll",SetLastError=true)] static extern bool SetProcessAffinityMask(IntPtr p,UIntPtr m);
    [DllImport("kernel32.dll",SetLastError=true)] static extern bool GetProcessAffinityMask(IntPtr p,out UIntPtr m,out UIntPtr s);
    [DllImport("kernel32.dll",SetLastError=true)] static extern uint GetPriorityClass(IntPtr p);
    [DllImport("kernel32.dll",SetLastError=true)] static extern uint ResumeThread(IntPtr t);
    [DllImport("kernel32.dll",SetLastError=true)] static extern uint WaitForSingleObject(IntPtr h,uint ms);
    [DllImport("kernel32.dll",SetLastError=true)] static extern bool GetExitCodeProcess(IntPtr p,out uint c);
    [DllImport("kernel32.dll",SetLastError=true,EntryPoint="QueryInformationJobObject")] static extern bool QueryJobPids(IntPtr j,int c,IntPtr buffer,uint size,IntPtr returned);
    [DllImport("kernel32.dll",SetLastError=true)] static extern IntPtr OpenProcess(uint access,bool inherit,uint pid);
    [DllImport("kernel32.dll",SetLastError=true)] static extern bool IsProcessInJob(IntPtr process,IntPtr job,out bool member);
    [DllImport("kernel32.dll",SetLastError=true)] static extern bool GetProcessTimes(IntPtr p,out FT c,out FT e,out FT k,out FT u);
    [DllImport("kernel32.dll",SetLastError=true,CharSet=CharSet.Unicode)] static extern bool QueryFullProcessImageNameW(IntPtr p,uint f,StringBuilder b,ref uint n);
    static void Check(bool ok,string what) { if(!ok) throw new Win32Exception(Marshal.GetLastWin32Error(),what); }
    static void Close(ref IntPtr h) { if(h!=IntPtr.Zero && h!=new IntPtr(-1)) CloseHandle(h); h=IntPtr.Zero; }
    static string Quote(string s) {
        if(s.IndexOf('\0')>=0) throw new ArgumentException("NUL in argument");
        var b=new StringBuilder("\""); int slashes=0;
        foreach(char c in s) {
            if(c=='\\') { slashes++; continue; }
            if(c=='\"') b.Append('\\',slashes*2+1).Append(c);
            else b.Append('\\',slashes).Append(c);
            slashes=0;
        }
        return b.Append('\\',slashes*2).Append('"').ToString();
    }
    public static void DurableNew(string path,string text) {
        byte[] raw=new UTF8Encoding(false).GetBytes(text);
        if(raw.Length>262144) throw new IOException("Metadata receipt exceeded 256 KiB cap");
        using(var f=new FileStream(path,FileMode.CreateNew,FileAccess.Write,FileShare.Read,4096,FileOptions.WriteThrough)) { f.Write(raw,0,raw.Length); f.Flush(true); }
    }
    static void Save(string path,object value) { DurableNew(path,JsonSerializer.Serialize(value,new JsonSerializerOptions { WriteIndented=true })); }
    static byte[] Metadata(string path) {
        var info=new FileInfo(path); if(!info.Exists || info.Length<=0 || info.Length>262144) throw new IOException("Complete bounded controller metadata required");
        using(var file=File.OpenRead(path)) { var raw=new byte[(int)info.Length]; int used=0,n;
            while(used<raw.Length && (n=file.Read(raw,used,raw.Length-used))>0) used+=n;
            if(used!=raw.Length || file.ReadByte()!=-1) throw new IOException("Controller metadata changed while reading"); return raw; }
    }
    static string Digest(byte[] raw) { return Convert.ToHexString(SHA256.HashData(raw)).ToLowerInvariant(); }
    static JsonDocument Object(byte[] raw) {
        var value=JsonDocument.Parse(raw,new JsonDocumentOptions { MaxDepth=16 });
        if(value.RootElement.ValueKind!=JsonValueKind.Object) { value.Dispose(); throw new IOException("Controller metadata object required"); }
        var keys=new HashSet<string>(); foreach(var p in value.RootElement.EnumerateObject()) if(!keys.Add(p.Name)) { value.Dispose(); throw new IOException("Duplicate controller property"); } return value;
    }
    static void Exact(JsonElement value,string name,string expected) {
        JsonElement field; if(!value.TryGetProperty(name,out field) || field.ValueKind!=JsonValueKind.String || field.GetString()!=expected) throw new IOException("Controller identity mismatch: "+name);
    }
    static void Complete(JsonElement value) {
        JsonElement passed,error; if(!value.TryGetProperty("passed",out passed) || passed.ValueKind!=JsonValueKind.True || !value.TryGetProperty("error",out error) || error.ValueKind!=JsonValueKind.Null) throw new IOException("Controller did not complete successfully");
    }
    static void BindController(Outcome result,string dir,string head,string binding,string cppTree) {
        byte[] proof=Metadata(Path.Combine(dir,"PAYLOAD_RESULT.json")),inner=Metadata(Path.Combine(dir,"INNER_RESULT.json")),complete=Metadata(Path.Combine(dir,"CONTROLLER_COMPLETE.json"));
        string proofSha=Digest(proof),innerSha=Digest(inner);
        using(var p=Object(proof)) using(var i=Object(inner)) using(var c=Object(complete)) {
            foreach(var value in new [] {p.RootElement,i.RootElement,c.RootElement}) { Exact(value,"head",head); Exact(value,"bindingsSha256",binding); Exact(value,"cppTree",cppTree); }
            JsonElement validated,error; if(!p.RootElement.TryGetProperty("automatedStageValidated",out validated) || validated.ValueKind!=JsonValueKind.True || !p.RootElement.TryGetProperty("error",out error) || error.ValueKind!=JsonValueKind.Null) throw new IOException("Incomplete payload proof");
            Complete(i.RootElement); Complete(c.RootElement);
            Exact(i.RootElement,"proofSha256",proofSha); Exact(c.RootElement,"proofSha256",proofSha); Exact(c.RootElement,"innerOutcomeSha256",innerSha);
        }
        result.wholeController=true; result.head=head; result.bindingsSha256=binding; result.cppTree=cppTree;
        result.payloadProofSha256=proofSha; result.innerOutcomeSha256=innerSha; result.controllerCompletionSha256=Digest(complete);
    }
    // The dispatcher exports only the returned, terminal whole-controller outcome.
    // It performs no later acceptance checks that could invalidate this receipt.
    public static void ExportReturnedOutcome(string path,Outcome result) { Save(path,result); }
    // Failure-only telemetry, after the unchanged survivor decision. Query handles
    // never grant termination authority and are checked for current job membership.
    static object[] FailureMembers(IntPtr job,out string error) {
        error=null; var rows=new List<object>(); IntPtr buffer=Marshal.AllocHGlobal(8+64*IntPtr.Size);
        try {
            Check(QueryJobPids(job,3,buffer,(uint)(8+64*IntPtr.Size),IntPtr.Zero),"Read failure members");
            int count=Marshal.ReadInt32(buffer,4); if(count<0 || count>64) throw new IOException("Failure member cap");
            for(int i=0;i<Math.Min(count,16);i++) {
                uint pid=checked((uint)Marshal.ReadIntPtr(buffer,8+i*IntPtr.Size).ToInt64());
                IntPtr h=OpenProcess(0x1000|0x100000,false,pid);
                if(h==IntPtr.Zero) { rows.Add(new {pid,error="Query handle unavailable"}); continue; }
                try {
                    bool member; Check(IsProcessInJob(h,job,out member),"Check failure membership");
                    if(!member) { rows.Add(new {pid,error="No longer a member"}); continue; }
                    FT c,e,k,u; Check(GetProcessTimes(h,out c,out e,out k,out u),"Read failure creation");
                    uint length=1024; var name=new StringBuilder((int)length);
                    Check(QueryFullProcessImageNameW(h,0,name,ref length),"Read failure executable");
                    rows.Add(new {pid,creationFileTime=c.Value,executable=name.ToString(),nativeSignaled=WaitForSingleObject(h,0)==WAIT_OBJECT_0});
                } catch(Exception ex) { rows.Add(new {pid,error=ex.Message}); }
                finally { Close(ref h); }
            }
        } catch(Exception ex) { error=ex.Message; }
        finally { Marshal.FreeHGlobal(buffer); }
        return rows.ToArray();
    }
    sealed class Capture {
        public Thread thread; public volatile bool fault; public string error; public long bytes;
        public Capture(IntPtr pipe,string path) {
            thread=new Thread(()=> {
                try {
                    using(var input=new FileStream(new SafeFileHandle(pipe,true),FileAccess.Read,4096,false))
                    using(var output=new FileStream(path,FileMode.CreateNew,FileAccess.Write,FileShare.Read,4096,FileOptions.WriteThrough)) {
                        var buffer=new byte[4096]; int n;
                        while((n=input.Read(buffer,0,buffer.Length))>0) {
                            if(bytes+n>OUTPUT_CAP) throw new IOException("Captured output exceeded 1 MiB; diagnostic must stop.");
                            output.Write(buffer,0,n); output.Flush(true); bytes+=n;
                        }
                    }
                } catch(Exception e) { error=e.Message; fault=true; }
            });
            thread.IsBackground=true; thread.Start();
        }
    }
    public sealed class Outcome {
        public string stem {get;set;} public string executable {get;set;}
        public uint pid {get;set;} public ulong creationFileTime {get;set;}
        public int boundSeconds {get;set;} public long frequency {get;set;}
        public long deadlineStartQpc {get;set;} public long observedExitQpc {get;set;}
        public string startedUtc {get;set;} public string finishedUtc {get;set;}
        public double elapsedSeconds {get;set;} public double? cpuSeconds {get;set;}
        public double cleanupElapsedSeconds {get;set;}
        public uint? exitCode {get;set;} public bool timedOut {get;set;}
        public string capacityExpiresUtc {get;set;} public bool reservationLost {get;set;} public bool leaseCancelled {get;set;} public long completedQpc {get;set;}
        public bool resumed {get;set;} public bool jobAssigned {get;set;}
        public bool terminationRequested {get;set;} public bool cleanupSucceeded {get;set;}
        public uint? activeOwnedProcessesAfterCleanup {get;set;}
        public bool outputComplete {get;set;} public long stdoutBytes {get;set;} public long stderrBytes {get;set;}
        public string error {get;set;} public string cleanupError {get;set;}
        public bool originalSurvivorDecision {get;set;} public object[] failureMembers {get;set;} public string failureSnapshotError {get;set;}
        public bool wholeController {get;set;} public string head {get;set;} public string bindingsSha256 {get;set;} public string cppTree {get;set;}
        public string payloadProofSha256 {get;set;} public string innerOutcomeSha256 {get;set;} public string controllerCompletionSha256 {get;set;}
        public bool acceptanceEvidence { get { return false; } }
        public bool passed { get { return resumed && jobAssigned && exitCode==0 && !timedOut && !reservationLost && !leaseCancelled && !terminationRequested && activeOwnedProcessesAfterCleanup==0 && error==null && cleanupSucceeded && outputComplete; } }
    }
    public static Outcome Run(string exe,string[] args,string wd,int seconds,string dir,string stem,string temp,string capacityExpiresUtc,long stageStartQpc,string cancellationPath) {
        return Run(exe,args,wd,seconds,dir,stem,temp,capacityExpiresUtc,stageStartQpc,cancellationPath,null,null,null);
    }
    public static Outcome Run(string exe,string[] args,string wd,int seconds,string dir,string stem,string temp,string capacityExpiresUtc,long stageStartQpc,string cancellationPath,string expectedHead,string expectedBindingsSha256,string expectedCppTree) {
        if(expectedBindingsSha256!=null) {
            if(Directory.Exists(dir)) throw new IOException("Fresh whole-controller output directory required");
            Directory.CreateDirectory(temp);
        }
        if(!((stem=="admission" && seconds==20) || (stem=="build" && seconds==900) || (stem=="native" && seconds==120) || (stem=="qualification" && seconds==650) || (stem=="render" && seconds==220) || (stem=="interactive" && seconds==220)) || !Directory.Exists(dir) || !Directory.Exists(temp)) throw new ArgumentException("Invalid planned acceptance stage bound");
        if(stem!="admission" && stem!="build" && stem!="native" && stem!="qualification" && stem!="render" && stem!="interactive") throw new ArgumentException("Invalid acceptance stage identity");
        if(stageStartQpc<=0 || stageStartQpc>Stopwatch.GetTimestamp() || String.IsNullOrWhiteSpace(cancellationPath)) throw new ArgumentException("Exact monotonic case origin and cancellation path required");
        var reservationEnd=DateTimeOffset.Parse(capacityExpiresUtc,System.Globalization.CultureInfo.InvariantCulture);
        var r=new Outcome { stem=stem,executable=Path.GetFullPath(exe),boundSeconds=seconds,frequency=Stopwatch.Frequency,startedUtc=DateTime.UtcNow.ToString("o"),capacityExpiresUtc=reservationEnd.ToString("o") };
        IntPtr job=IntPtr.Zero,readOut=IntPtr.Zero,writeOut=IntPtr.Zero,readErr=IntPtr.Zero,writeErr=IntPtr.Zero,stdin=IntPtr.Zero,attrs=IntPtr.Zero,handles=IntPtr.Zero,environment=IntPtr.Zero;
        PI pi=new PI(); Capture stdout=null,stderr=null; Stopwatch clock=null; bool attributesInitialized=false;
        try {
            job=CreateJobObjectW(IntPtr.Zero,null); Check(job!=IntPtr.Zero,"Create owned job");
            var limit=new LIMIT(); limit.basic.flags=KILL_ON_JOB_CLOSE;
            Check(SetInformationJobObject(job,9,ref limit,(uint)Marshal.SizeOf<LIMIT>()),"Set owned job close cleanup");
            var sa=new SA { length=Marshal.SizeOf<SA>(),inherit=1 };
            Check(CreatePipe(out readOut,out writeOut,ref sa,0),"stdout pipe");
            Check(CreatePipe(out readErr,out writeErr,ref sa,0),"stderr pipe");
            Check(SetHandleInformation(readOut,1,0),"stdout reader noninheritance");
            Check(SetHandleInformation(readErr,1,0),"stderr reader noninheritance");
            stdin=CreateFileW("NUL",0x80000000,3,ref sa,3,0,IntPtr.Zero);
            Check(stdin!=new IntPtr(-1),"NUL stdin");
            IntPtr size=IntPtr.Zero;
            InitializeProcThreadAttributeList(IntPtr.Zero,1,0,ref size);
            attrs=Marshal.AllocHGlobal(size);
            Check(InitializeProcThreadAttributeList(attrs,1,0,ref size),"Initialize exact inherited handle list"); attributesInitialized=true;
            handles=Marshal.AllocHGlobal(IntPtr.Size*3);
            Marshal.WriteIntPtr(handles,0,stdin); Marshal.WriteIntPtr(handles,IntPtr.Size,writeOut); Marshal.WriteIntPtr(handles,IntPtr.Size*2,writeErr);
            Check(UpdateProcThreadAttribute(attrs,0,new IntPtr(0x20002),handles,new IntPtr(IntPtr.Size*3),IntPtr.Zero,IntPtr.Zero),"Restrict inherited handles");
            var env=new SortedDictionary<string,string>(StringComparer.OrdinalIgnoreCase);
            foreach(DictionaryEntry e in Environment.GetEnvironmentVariables()) env[(string)e.Key]=(string)e.Value;
            env["TEMP"]=temp; env["TMP"]=temp;
            var block=new StringBuilder(); foreach(var e in env) block.Append(e.Key).Append('=').Append(e.Value).Append('\0'); block.Append('\0');
            environment=Marshal.StringToHGlobalUni(block.ToString());
            var si=new SIX { startup=new SI { cb=Marshal.SizeOf<SIX>(),flags=STARTF_USESTDHANDLES,stdin=stdin,stdout=writeOut,stderr=writeErr },attributes=attrs };
            var command=new StringBuilder(Quote(exe)); foreach(string a in args) command.Append(' ').Append(Quote(a));
            if((reservationEnd-DateTimeOffset.UtcNow).TotalSeconds<seconds+5) { r.reservationLost=true; throw new InvalidOperationException("Reservation cannot cover child creation, full functional limit and five-second cleanup"); }
            clock=Stopwatch.StartNew(); r.deadlineStartQpc=stageStartQpc;
            Check(CreateProcessW(exe,command,IntPtr.Zero,IntPtr.Zero,true,CREATE_SUSPENDED|CREATE_NO_WINDOW|NORMAL_PRIORITY_CLASS|EXTENDED_STARTUPINFO_PRESENT|CREATE_UNICODE_ENVIRONMENT,environment,wd,ref si,out pi),"Create suspended exact child");
            r.pid=pi.pid;
            Check(AssignProcessToJobObject(job,pi.process),"Assign suspended child to owned job"); r.jobAssigned=true;
            Check(SetProcessAffinityMask(pi.process,new UIntPtr(68)),"Set 0x44 before resume");
            UIntPtr affinity,system; Check(GetProcessAffinityMask(pi.process,out affinity,out system),"Read child affinity");
            if(affinity.ToUInt64()!=68 || GetPriorityClass(pi.process)!=NORMAL_PRIORITY_CLASS) throw new Exception("Child resource policy mismatch");
            FT created,exited,kernel,user; Check(GetProcessTimes(pi.process,out created,out exited,out kernel,out user),"Bind process creation time"); r.creationFileTime=created.Value;
            uint length=32768; var image=new StringBuilder((int)length); Check(QueryFullProcessImageNameW(pi.process,0,image,ref length),"Bind executable");
            if(!String.Equals(image.ToString(),r.executable,StringComparison.OrdinalIgnoreCase)) throw new Exception("Child executable identity mismatch");
            Save(Path.Combine(dir,stem+".identity.json"),new { pid=r.pid,creationFileTime=r.creationFileTime,executable=r.executable,arguments=args,boundSeconds=seconds,affinityMask="0x44",priority="Normal",jobAssignedBeforeResume=true,deadlineStartQpc=r.deadlineStartQpc,frequency=r.frequency });
            stdout=new Capture(readOut,Path.Combine(dir,stem+".stdout.log")); readOut=IntPtr.Zero;
            stderr=new Capture(readErr,Path.Combine(dir,stem+".stderr.log")); readErr=IntPtr.Zero;
            Close(ref writeOut); Close(ref writeErr); Close(ref stdin);
            if(Elapsed(stageStartQpc)>=seconds) { r.timedOut=true; throw new TimeoutException("Deadline expired before child resume"); }
            if((reservationEnd-DateTimeOffset.UtcNow).TotalSeconds<seconds-Elapsed(stageStartQpc)+5) { r.reservationLost=true; throw new InvalidOperationException("Reservation coverage lost before child resume"); }
            if(File.Exists(cancellationPath)) { r.leaseCancelled=true; throw new InvalidOperationException("Lease cancelled before resume"); }
            if(ResumeThread(pi.thread)!=1) throw new Win32Exception(Marshal.GetLastWin32Error(),"Resume owned child"); r.resumed=true;
            while(true) {
                if(File.Exists(cancellationPath)) { r.leaseCancelled=true; r.error="Coordinator cancelled the owned lease"; break; }
                if(DateTimeOffset.UtcNow>=reservationEnd.AddSeconds(-5)) { r.reservationLost=true; r.error="Reservation reached owned-cleanup boundary"; break; }
                uint wait=WaitForSingleObject(pi.process,0);
                if(wait==WAIT_OBJECT_0) { r.observedExitQpc=Stopwatch.GetTimestamp(); if(Elapsed(stageStartQpc)>=seconds) r.timedOut=true; break; }
                if(wait!=WAIT_TIMEOUT) throw new Win32Exception(Marshal.GetLastWin32Error(),"Wait owned child");
                if(stdout.fault || stderr.fault) throw new IOException("Output capture failure: "+stdout.error+" "+stderr.error);
                if(Elapsed(stageStartQpc)>=seconds) { r.timedOut=true; break; }
                Thread.Sleep((int)Math.Min(20,Math.Max(1,(seconds-Elapsed(stageStartQpc))*1000)));
            }
        } catch(Exception e) { r.error=e.Message; }
        finally {
            var cleanup=Stopwatch.StartNew();
            try {
                if(pi.process!=IntPtr.Zero) {
                    uint wait=WaitForSingleObject(pi.process,0);
                    if(wait!=WAIT_OBJECT_0) {
                        r.terminationRequested=true;
                        if(r.jobAssigned) Check(TerminateJobObject(job,0xdead),"Terminate owned job");
                        else Check(TerminateProcess(pi.process,0xdead),"Terminate own suspended unassigned child");
                    }
                }
                if(job!=IntPtr.Zero) {
                    ACCOUNTING a; Check(QueryInformationJobObject(job,1,out a,(uint)Marshal.SizeOf<ACCOUNTING>(),IntPtr.Zero),"Inspect owned job");
                    if(a.activeProcesses>0 && !r.terminationRequested) {
                        r.terminationRequested=true; r.originalSurvivorDecision=true;
                        Check(TerminateJobObject(job,0xdead),"Stop owned descendants after root exit"); if(r.error==null) r.error="Owned descendants outlived root";
                        // Termination always precedes optional diagnostics, within
                        // the same aggregate cleanup budget. No diagnostic grace.
                        r.failureSnapshotError="Member query omitted; owned termination and exit confirmation take priority";
                    }
                    do {
                        Check(QueryInformationJobObject(job,1,out a,(uint)Marshal.SizeOf<ACCOUNTING>(),IntPtr.Zero),"Confirm owned job empty");
                        r.activeOwnedProcessesAfterCleanup=a.activeProcesses;
                        if(a.activeProcesses==0) break;
                        Thread.Sleep(10);
                    } while(cleanup.Elapsed.TotalSeconds<5);
                }
                // An empty job can precede the retained root handle becoming signaled.
                // Confirm its exit using only the remaining original cleanup budget.
                if(pi.process!=IntPtr.Zero && WaitForSingleObject(pi.process,(uint)Math.Max(0,(5-cleanup.Elapsed.TotalSeconds)*1000))==WAIT_OBJECT_0) {
                    uint code; Check(GetExitCodeProcess(pi.process,out code),"Capture owned exit code"); r.exitCode=code;
                    FT c,e,k,u; Check(GetProcessTimes(pi.process,out c,out e,out k,out u),"Capture final CPU"); r.cpuSeconds=(k.Value+u.Value)/10000000.0;
                    if(r.observedExitQpc==0) r.observedExitQpc=Stopwatch.GetTimestamp();
                }
                Close(ref writeOut); Close(ref writeErr); Close(ref stdin);
                if(stdout!=null) stdout.thread.Join((int)Math.Max(0,(5-cleanup.Elapsed.TotalSeconds)*1000));
                if(stderr!=null) stderr.thread.Join((int)Math.Max(0,(5-cleanup.Elapsed.TotalSeconds)*1000));
                r.outputComplete=stdout!=null && stderr!=null && !stdout.thread.IsAlive && !stderr.thread.IsAlive && !stdout.fault && !stderr.fault;
                r.stdoutBytes=stdout==null?0:stdout.bytes; r.stderrBytes=stderr==null?0:stderr.bytes;
                r.cleanupSucceeded=(pi.process==IntPtr.Zero || r.exitCode.HasValue) && (!r.jobAssigned || r.activeOwnedProcessesAfterCleanup==0) && (stdout==null || !stdout.thread.IsAlive) && (stderr==null || !stderr.thread.IsAlive) && cleanup.Elapsed.TotalSeconds<=5;
                if(!r.outputComplete && r.error==null) r.error="Output not complete: "+(stdout==null?"no stdout":stdout.error)+" "+(stderr==null?"no stderr":stderr.error);
                if(!r.cleanupSucceeded) r.cleanupError="Owned process/reader cleanup did not meet separate five-second bound";
            } catch(Exception e) { r.cleanupSucceeded=false; r.cleanupError=e.Message; }
            // Closing this retained job handle kills only its assigned descendants,
            // including on failure paths. Telemetry handles have query/sync access only.
            Close(ref job); Close(ref pi.thread); Close(ref pi.process);
            Close(ref writeOut); Close(ref writeErr); Close(ref readOut); Close(ref readErr); Close(ref stdin);
            if(attributesInitialized) DeleteProcThreadAttributeList(attrs);
            if(attrs!=IntPtr.Zero) Marshal.FreeHGlobal(attrs);
            if(handles!=IntPtr.Zero) Marshal.FreeHGlobal(handles);
            if(environment!=IntPtr.Zero) Marshal.FreeHGlobal(environment);
            r.cleanupElapsedSeconds=cleanup.Elapsed.TotalSeconds;
            if(r.cleanupElapsedSeconds>5) { r.cleanupSucceeded=false; r.cleanupError="Aggregate owned cleanup exceeded five seconds"; }
            if(expectedBindingsSha256!=null && r.passed) {
                try { BindController(r,dir,expectedHead,expectedBindingsSha256,expectedCppTree); }
                catch(Exception e) { r.error=e.Message; }
            }
            r.elapsedSeconds=clock==null?0:Elapsed(stageStartQpc); r.finishedUtc=DateTime.UtcNow.ToString("o");
            r.completedQpc=Stopwatch.GetTimestamp();
            if(Elapsed(stageStartQpc)>=seconds) r.timedOut=true;
            if(File.Exists(cancellationPath)) r.leaseCancelled=true;
            Save(Path.Combine(dir,stem+".exit.json"),r);
            // A synchronous receipt-write overrun fails on return. Never infer a
            // complete case from the earlier raw file or child exit alone.
            r.completedQpc=Stopwatch.GetTimestamp();
            r.elapsedSeconds=Elapsed(stageStartQpc);
            if(r.elapsedSeconds>=seconds) r.timedOut=true;
        }
        return r;
    }
}
