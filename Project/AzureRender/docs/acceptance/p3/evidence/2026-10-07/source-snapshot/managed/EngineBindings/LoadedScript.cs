using System.Runtime.CompilerServices;
#if !AZURE_NATIVEAOT
using System.Reflection;
using System.Runtime.Loader;
#endif
namespace Azure.Engine;
internal sealed class LoadedScript {
    private ScriptBehaviour? behaviour;
    internal ScriptBehaviour Behaviour=>behaviour??throw new InvalidOperationException("Script object closed");
#if !AZURE_NATIVEAOT
    private sealed class ScriptContext:AssemblyLoadContext {
        internal ScriptContext():base(isCollectible:true) {}
        protected override Assembly? Load(AssemblyName name) => name.Name==typeof(ScriptBehaviour).Assembly.GetName().Name?typeof(ScriptBehaviour).Assembly:null;
    }
    private ScriptContext? context;
    private static readonly List<WeakReference> Contexts=new();
#endif
    private LoadedScript() {}
    [MethodImpl(MethodImplOptions.NoInlining)]
    internal static LoadedScript Create(string path,string type) {
#if AZURE_NATIVEAOT
        return new LoadedScript {behaviour=NativeTypes.Create(type)};
#else
        var context=new ScriptContext();Contexts.Add(new WeakReference(context));
        try {
            // Memory loading releases the source DLL immediately after the read.
            using var stream=new MemoryStream(File.ReadAllBytes(path));var assembly=context.LoadFromStream(stream);
            var definition=assembly.GetType(type,true)!;
            if(!typeof(ScriptBehaviour).IsAssignableFrom(definition))throw new ArgumentException("Managed script must implement ScriptBehaviour");
            return new LoadedScript { context=context,behaviour=(ScriptBehaviour)Activator.CreateInstance(definition)! };
        }catch {context.Unload();throw;}
#endif
    }
    [MethodImpl(MethodImplOptions.NoInlining)]
    internal void Close() {
        behaviour=null;
#if !AZURE_NATIVEAOT
        var previous=context;context=null;previous?.Unload();
#endif
    }
    internal static void Collect() {
#if !AZURE_NATIVEAOT
        for(int index=0;index<3;index++){GC.Collect();GC.WaitForPendingFinalizers();GC.Collect();}
        Contexts.RemoveAll(item=>!item.IsAlive);
#endif
    }
    internal static int LiveContexts {
        get {
#if AZURE_NATIVEAOT
            return 0;
#else
            return Contexts.Count(item=>item.IsAlive);
#endif
        }
    }
}
