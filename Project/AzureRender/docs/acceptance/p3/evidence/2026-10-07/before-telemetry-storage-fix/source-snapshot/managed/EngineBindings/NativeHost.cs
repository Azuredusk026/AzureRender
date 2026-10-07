using System.Runtime.InteropServices;
using System.Text;
using System.Text.Json;
using System.Text.Json.Nodes;
namespace Azure.Engine;
[StructLayout(LayoutKind.Sequential)]
public unsafe struct ScriptHostApi {
    public uint Version,Size;
    public ulong Session;
    public delegate* unmanaged[Cdecl]<ulong,byte*,uint,byte*,uint,uint*,int> Invoke;
}
internal sealed unsafe class NativeHost {
    private readonly ScriptHostApi api;
    private readonly byte[] buffer=new byte[1024*1024];
    private bool closed;
    private readonly int owner=Environment.CurrentManagedThreadId;
    internal NativeHost(ScriptHostApi value) {api=value;}
    internal void Close() {closed=true;}
    internal JsonNode? Invoke(JsonObject handle,string method,JsonArray arguments) {
        if(closed || owner!=Environment.CurrentManagedThreadId)throw new InvalidOperationException("Managed host session is closed or on a foreign thread");
        var request=new JsonObject { ["apiVersion"]=1,["object"]=handle.DeepClone(),["method"]=method,["arguments"]=arguments };
        byte[] bytes=JsonBytes.Write(request);if(bytes.Length>buffer.Length)throw new ArgumentException("Script request byte budget exceeded");
        uint written=0;int status;
        fixed(byte* input=bytes)fixed(byte* output=buffer)status=api.Invoke(api.Session,input,(uint)bytes.Length,output,(uint)buffer.Length,&written);
        if(written==0 || written>buffer.Length)throw new InvalidOperationException("Native response ownership violation");
        var response=JsonNode.Parse(buffer.AsSpan(0,(int)written))!.AsObject();
        if(status!=0 || response["ok"]!.GetValue<bool>()!=true)throw new InvalidOperationException(response["error"]!.GetValue<string>());
        return response["value"]?.DeepClone();
    }
}
internal static class JsonBytes {
    internal static byte[] Write(JsonNode node) {
        using var stream=new MemoryStream();using(var writer=new Utf8JsonWriter(stream))node.WriteTo(writer);
        return stream.ToArray();
    }
}
