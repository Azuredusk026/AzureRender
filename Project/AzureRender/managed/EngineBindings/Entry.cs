using System.Runtime.CompilerServices;
using System.Runtime.InteropServices;
using System.Text.Json.Nodes;
namespace Azure.Engine;
public static unsafe class Entry {
    private sealed class Session {
        internal readonly NativeHost Host;
        internal readonly Dictionary<string,LoadedScript> Active=new();
        internal readonly Dictionary<string,LoadedScript> Candidates=new();
        internal readonly int Owner=Environment.CurrentManagedThreadId;
        internal Session(ScriptHostApi api) {Host=new NativeHost(api);}
    }
    private static readonly Dictionary<ulong,Session> Sessions=new();
    private static readonly object Gate=new();
    [UnmanagedCallersOnly(EntryPoint="AzureScriptEntry",CallConvs=new[]{typeof(CallConvCdecl)})]
    public static int Invoke(ScriptHostApi* api,byte* input,uint inputSize,byte* output,uint capacity,uint* written) {
        if(written==null)return 1;*written=0;
        if(output==null || capacity<1024*1024)return 2;
        int status=0;JsonObject response;
        try {
            if(api==null || api->Version!=1 || api->Size!=(uint)sizeof(ScriptHostApi) || api->Session==0 || api->Invoke==null)
                throw new ArgumentException("Invalid managed host ABI version or size");
            if(input==null || inputSize==0 || inputSize>1024*1024)throw new ArgumentException("Invalid managed request byte length");
            var request=JsonNode.Parse(new ReadOnlySpan<byte>(input,(int)inputSize))!.AsObject();
            JsonNode value;lock(Gate)value=Execute(*api,request);
            response=new JsonObject { ["ok"]=true,["value"]=value };
        } catch(Exception error) {
            status=1;string message=error.GetBaseException().Message;
            response=new JsonObject { ["ok"]=false,["error"]=message[..Math.Min(message.Length,4096)] };
        }
        try {
            byte[] bytes=JsonBytes.Write(response);
            if(bytes.Length>capacity)return 3;
            bytes.CopyTo(new Span<byte>(output,(int)capacity));*written=(uint)bytes.Length;return status;
        }catch {return 4;}
    }
    private static JsonNode Execute(ScriptHostApi api,JsonObject request) {
        string operation=request["operation"]!.GetValue<string>();
        if(operation=="handshake") {
            if(request["apiVersion"]!.GetValue<int>()!=1)throw new ArgumentException("Managed binding version rejected");
            return new JsonObject { ["apiVersion"]=1,["bindingHash"]=BindingContract.Hash,["trustedCompiledExtension"]=true };
        }
        if(operation=="diagnostics") {
            LoadedScript.Collect();return new JsonObject { ["sessions"]=Sessions.Count,
                ["objects"]=Sessions.Values.Sum(s=>s.Active.Count+s.Candidates.Count),["liveContexts"]=LoadedScript.LiveContexts };
        }
        if(!Sessions.TryGetValue(api.Session,out Session? session)) {
            if(operation=="closeSession")return JsonValue.Create(true)!;
            if(operation!="prepare")throw new InvalidOperationException("Managed script session is unavailable");
            session=new Session(api);Sessions.Add(api.Session,session);
        }
        if(session.Owner!=Environment.CurrentManagedThreadId)throw new InvalidOperationException("Managed owner thread required");
        if(operation=="closeSession") {
            foreach(var item in session.Candidates.Values)item.Close();foreach(var item in session.Active.Values)item.Close();
            session.Candidates.Clear();session.Active.Clear();session.Host.Close();Sessions.Remove(api.Session);return JsonValue.Create(true)!;
        }
        string id=request["id"]!.GetValue<string>();
        if(operation=="prepare") {
            if(session.Candidates.Remove(id,out var previous))previous.Close();
            LoadedScript? candidate=null;
            try {
                candidate=LoadedScript.Create(request["assembly"]!.GetValue<string>(),request["type"]!.GetValue<string>());
                candidate.Behaviour.Self=new AzureObject(session.Host,request["object"]!.AsObject());
                if(!candidate.Behaviour.Self.Alive())throw new InvalidOperationException("Managed script object is stale");
                candidate.Behaviour.Init();
                session.Candidates.Add(id,candidate);return JsonValue.Create(true)!;
            }catch{
                candidate?.Close();
                if(session.Active.Count==0 && session.Candidates.Count==0){session.Host.Close();Sessions.Remove(api.Session);}
                throw;
            }
        }
        if(operation=="cancel") {if(session.Candidates.Remove(id,out var candidate))candidate.Close();return JsonValue.Create(true)!;}
        if(operation=="activate") {
            var candidate=session.Candidates[id];session.Candidates.Remove(id);
            if(session.Active.Remove(id,out var previous))previous.Close();session.Active[id]=candidate;return JsonValue.Create(true)!;
        }
        if(operation=="closeObject") {
            if(session.Candidates.Remove(id,out var candidate))candidate.Close();
            if(session.Active.Remove(id,out var active))active.Close();return JsonValue.Create(true)!;
        }
        if(operation=="callback") {
            ScriptBehaviour script=session.Active[id].Behaviour;
            switch(request["callback"]!.GetValue<string>()) {
                case "update": script.Update(request["delta"]!.GetValue<double>());break;
                case "trigger": script.Trigger(request["other"]!.GetValue<string>(),request["entered"]!.GetValue<bool>());break;
                case "interact": script.Interact(request["actor"]!.GetValue<string>());break;
                case "shutdown": script.Shutdown();break;
                default:throw new ArgumentException("Unknown managed callback");
            }
            return JsonValue.Create(true)!;
        }
        throw new ArgumentException("Unknown managed operation");
    }
}
