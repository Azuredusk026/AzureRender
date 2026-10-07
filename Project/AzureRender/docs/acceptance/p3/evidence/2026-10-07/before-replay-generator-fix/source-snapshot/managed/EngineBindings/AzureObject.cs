using System.Text.Json.Nodes;
namespace Azure.Engine;
public sealed partial class AzureObject {
    internal NativeHost Host { get; }
    internal JsonObject Handle { get; }
    internal AzureObject(NativeHost host,JsonObject handle) {Host=host;Handle=(JsonObject)handle.DeepClone();}
    internal AzureObject? ObjectResult(JsonNode? value) => value is JsonObject identity?new AzureObject(Host,identity):null;
    internal static JsonArray ArrayValue(float[] values) {
        if(values.Length>1024)throw new ArgumentException("Script array budget exceeded");
        var result=new JsonArray();foreach(float v in values)result.Add((JsonNode?)JsonValue.Create(v));return result;
    }
}
public abstract class ScriptBehaviour {
    public AzureObject Self { get; internal set; }=null!;
    public virtual void Init() {}
    public virtual void Update(double delta) {}
    public virtual void Trigger(string other,bool entered) {}
    public virtual void Interact(string actor) {}
    public virtual void Shutdown() {}
}
