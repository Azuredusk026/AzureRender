using Azure.Engine;
using System.Text.Json.Nodes;
namespace Azure.Samples;
public sealed class Counter:ScriptBehaviour {
    private int count;
    public override void Init() {Self.Set("azure.transform","translation",new JsonArray(0,0,0));}
    public override void Update(double delta) {
        count++;Self.Set("azure.transform","translation",new JsonArray(count,0,0));Self.UiText("status","脚本状态："+count);
    }
    public override void Trigger(string other,bool entered) {if(entered)Self.UiText("event","触发："+other);}
    public override void Interact(string actor) {Self.UiText("event","交互："+actor);}
    public override void Shutdown() {Self.UiText("status","脚本关闭");}
}
public sealed class BrokenInit:ScriptBehaviour {
    public override void Init() {Self.Set("azure.transform","translation",new JsonArray(900,0,0));throw new InvalidOperationException("候选初始化失败");}
}
public sealed class BrokenUpdate:ScriptBehaviour {
    public override void Update(double delta) {throw new InvalidOperationException("运行回调失败");}
}
public sealed class SceneAnnotator:ScriptBehaviour {
    public override void Init() {Self.UiText("annotation",Self.Id+"：场景标注");}
    public override void Update(double delta) {Self.Set("azure.transform","translation",new JsonArray(4,5,6));}
}
public sealed class ExplorationController:ScriptBehaviour {
    public override void Update(double delta) {
        float x=(Self.Action("move-right")?1:0)-(Self.Action("move-left")?1:0);
        float z=(Self.Action("move-back")?1:0)-(Self.Action("move-forward")?1:0);
        Self.Move(x,z,Self.Pressed("jump"));
    }
}
