using System;
using System.Collections;
using System.Collections.Generic;
using System.Globalization;
using System.IO;
using System.Reflection;
using UnityEngine;
using System.Linq;
using Newtonsoft.Json;
using Newtonsoft.Json.Serialization;
using UnityEngine.Rendering;
using UnityEngine.SceneManagement;

public static class ReferenceSkyDiagnostic
{
    private static RoomFrameCapture driver;
    public static object ActiveSphere;
    public static void Capture(object sphere)
    {
        ActiveSphere = sphere;
        if (driver != null) return;
        var root = new GameObject("AzureRender reference frames");
        UnityEngine.Object.DontDestroyOnLoad(root);
        driver = root.AddComponent<RoomFrameCapture>();
    }

    public static object Read(object source, string name)
    {
        if (source == null) return null;
        var type = source.GetType();
        var flags = BindingFlags.Public | BindingFlags.NonPublic | BindingFlags.Instance | BindingFlags.Static;
        var field = type.GetField(name, flags);
        if (field != null) return field.GetValue(source);
        var property = type.GetProperty(name, flags);
        return property == null ? null : property.GetValue(source, null);
    }
}

[Serializable]
public class RoomFrameRecord
{
    public string weather;
    public string color;
    public string depth;
    public int index;
    public int frame;
    public float simulationTime;
    public float realtime;
    public int width;
    public int height;
    public string camera;
    public Vector3 position;
    public Quaternion rotation;
    public Matrix4x4 worldToCamera;
    public Matrix4x4 projection;
    public Matrix4x4 gpuProjection;
    public float near;
    public float far;
    public bool reversedZ;
    public string depthGraphicsFormat;
    public string graphicsDevice;
    public string atmosphereProfile;
    public string weatherClock;
    public float clockHours;
    public float clockMinutes;
    public float clockSeconds;
    public float clockMilliseconds;
    public float modifiedDayPercentage;
    public CapturedShaderValue[] shaderValues;
    public CapturedTransform[] transforms;
    public CapturedSkin[] skins;
    public string[] activeObjects;
}


[Serializable]
public class CapturedTransform
{
    public string path;
    public int id;
    public int parentId;
    public Vector3 position;
    public Quaternion rotation;
    public Vector3 scale;
}

[Serializable]
public class CapturedSkin
{
    public string path;
    public string mesh;
    public string root;
    public string[] bones;
    public float[] blendWeights;
}

[Serializable]
public class CapturedShaderValue
{
    public string name;
    public Vector4 value;
}

public class CaptureFieldsResolver : DefaultContractResolver
{
    protected override IList<JsonProperty> CreateProperties(Type type, MemberSerialization serialization)
    {
        return type.GetFields(BindingFlags.Instance | BindingFlags.Public)
            .Select(field => CreateProperty(field, MemberSerialization.Fields)).ToList();
    }
}

public class RoomFrameCapture : MonoBehaviour
{
    private Camera renderedCamera;
    private Texture depthTexture;
    private int renderedFrame = -1;
    private int outstanding;
    private int readbackFailures;
    private string output;

    private void OnEnable()
    {
        RenderPipelineManager.endCameraRendering += RenderedCamera;
    }

    private void OnDisable()
    {
        RenderPipelineManager.endCameraRendering -= RenderedCamera;
    }

    private void RenderedCamera(ScriptableRenderContext context, Camera camera)
    {
        if (camera != Camera.main) return;
        renderedCamera = camera;
        depthTexture = Shader.GetGlobalTexture("_CameraDepthTexture");
        renderedFrame = Time.frameCount;
    }

    private static string Root()
    {
        return Directory.GetParent(Application.dataPath).FullName;
    }

    private static string OutputPath()
    {
        var arguments = Environment.GetCommandLineArgs();
        for (var index = 0; index + 1 < arguments.Length; index++)
        {
            if (arguments[index] == "-azure-frame-output")
                return Path.GetFullPath(arguments[index + 1]);
        }
        return Path.Combine(Root(), "closed-frames");
    }

    private void Log(string message)
    {
        File.AppendAllText(Path.Combine(output, "capture-status.log"), message + Environment.NewLine);
        Debug.Log("[ROOM_FRAMES] " + message);
    }

    private static string[] ActiveObjectPaths()
    {
        var paths = new List<string>();
        foreach (var transform in UnityEngine.Object.FindObjectsOfType<Transform>())
        {
            if (!transform.gameObject.activeInHierarchy) continue;
            var path = transform.name;
            var parent = transform.parent;
            while (parent != null)
            {
                path = parent.name + "/" + path;
                parent = parent.parent;
            }
            paths.Add(path);
        }
        paths.Sort(StringComparer.Ordinal);
        return paths.ToArray();
    }


    private static string ObjectPath(Transform transform)
    {
        if (transform == null) return "";
        var result = transform.name;
        for (var parent = transform.parent; parent != null; parent = parent.parent)
            result = parent.name + "/" + result;
        return result;
    }

    private static CapturedTransform[] CaptureTransforms()
    {
        var result = new List<CapturedTransform>();
        var visible = new Dictionary<int, Transform>();
        foreach (var child in UnityEngine.Object.FindObjectsOfType<Transform>())
        {
            if (!child.gameObject.activeInHierarchy) continue;
            for (var node = child; node != null; node = node.parent)
            {
                if (visible.ContainsKey(node.GetInstanceID())) break;
                visible.Add(node.GetInstanceID(), node);
            }
        }
        foreach (var transform in visible.Values)
        {
            result.Add(new CapturedTransform {
                path = ObjectPath(transform), id = transform.GetInstanceID(),
                parentId = transform.parent == null ? 0 : transform.parent.GetInstanceID(),
                position = transform.localPosition,
                rotation = transform.localRotation, scale = transform.localScale
            });
        }
        result.Sort((a, b) => StringComparer.Ordinal.Compare(a.path, b.path));
        return result.ToArray();
    }

    private static CapturedSkin[] CaptureSkins()
    {
        var result = new List<CapturedSkin>();
        foreach (var skin in UnityEngine.Object.FindObjectsOfType<SkinnedMeshRenderer>())
        {
            if (!skin.enabled || !skin.gameObject.activeInHierarchy || skin.sharedMesh == null) continue;
            var bones = new List<string>();
            foreach (var bone in skin.bones) bones.Add(ObjectPath(bone));
            var weights = new float[skin.sharedMesh.blendShapeCount];
            for (var index = 0; index < weights.Length; index++) weights[index] = skin.GetBlendShapeWeight(index);
            result.Add(new CapturedSkin {
                path = ObjectPath(skin.transform), mesh = skin.sharedMesh.name,
                root = ObjectPath(skin.rootBone), bones = bones.ToArray(), blendWeights = weights
            });
        }
        result.Sort((a, b) => StringComparer.Ordinal.Compare(a.path, b.path));
        return result.ToArray();
    }

    private static float Number(object source, string name)
    {
        var value = ReferenceSkyDiagnostic.Read(source, name);
        return value == null ? 0 : Convert.ToSingle(value, CultureInfo.InvariantCulture);
    }

    private static CapturedShaderValue[] CaptureShaderValues(object sphere)
    {
        var result = new List<CapturedShaderValue>();
        var ids = sphere.GetType().Assembly.GetType("DistantLands.Cozy.CozyShaderIDs");
        foreach (var field in ids.GetFields(BindingFlags.Public | BindingFlags.NonPublic | BindingFlags.Static))
        {
            if (field.FieldType != typeof(int)) continue;
            var name = field.Name;
            var id = (int)field.GetValue(null);
            if (name.IndexOf("Color", StringComparison.OrdinalIgnoreCase) >= 0)
                result.Add(new CapturedShaderValue { name = name, value = Shader.GetGlobalColor(id) });
            else if (name.IndexOf("Direction", StringComparison.OrdinalIgnoreCase) >= 0)
                result.Add(new CapturedShaderValue { name = name, value = Shader.GetGlobalVector(id) });
        }
        result.Sort((a, b) => StringComparer.Ordinal.Compare(a.name, b.name));
        return result.ToArray();
    }

    private IEnumerator Start()
    {
        output = OutputPath();
        if (Directory.Exists(output))
        {
            Debug.LogError("[ROOM_FRAMES] Capture output already exists: " + output);
            Application.Quit(6);
            yield break;
        }
        Directory.CreateDirectory(output);
        Log("Starting " + Application.version + " at " + Application.dataPath);
        var limit = Time.realtimeSinceStartup + 90f;
        Component windows = null;
        Type windowType = null;
        while (Time.realtimeSinceStartup < limit)
        {
            foreach (var assembly in AppDomain.CurrentDomain.GetAssemblies())
            {
                windowType = assembly.GetType("Bulbul.WindowViewService");
                if (windowType != null) break;
            }
            if (SceneManager.GetSceneByName("RoomScene").isLoaded && Camera.main != null && windowType != null)
            {
                var found = UnityEngine.Object.FindObjectsOfType(windowType);
                if (found.Length == 1) windows = found[0] as Component;
            }
            if (windows != null) break;
            yield return null;
        }
        if (windows == null)
        {
            Log("RoomScene or WindowViewService did not become ready");
            Application.Quit(3);
            yield break;
        }

        Screen.SetResolution(1280, 720, false);
        QualitySettings.vSyncCount = 0;
        Application.targetFrameRate = 60;
        Time.captureFramerate = 60;
        Camera.main.depthTextureMode |= DepthTextureMode.Depth;
        var pipeline = GraphicsSettings.currentRenderPipeline;
        if (pipeline != null)
        {
            var option = pipeline.GetType().GetProperty("supportsCameraDepthTexture");
            if (option != null && option.CanWrite) option.SetValue(pipeline, true, null);
        }
        foreach (var component in Camera.main.GetComponents<Component>())
        {
            if (component.GetType().Name != "UniversalAdditionalCameraData") continue;
            var property = component.GetType().GetProperty("requiresDepthOption");
            if (property != null) property.SetValue(component, Enum.Parse(property.PropertyType, "On"), null);
        }
        for (var settling = 0; settling < 120; settling++) yield return null;
        windowType.GetMethod("ResetOtherThanTimeOfDay").Invoke(windows, new object[0]);
        var change = windowType.GetMethod("ChangeWeatherAndTime");
        var modeType = change.GetParameters()[0].ParameterType;
        foreach (var mode in new[] {"Night", "Cloudy"})
        {
            change.Invoke(windows, new[] {Enum.Parse(modeType, mode)});
            for (var settling = 0; settling < 90; settling++) yield return null;
            Log("Sampling " + mode);
            for (var sample = 0; sample < 12; sample++)
            {
                for (var gap = 0; gap < 4; gap++) yield return new WaitForEndOfFrame();
                if (renderedFrame != Time.frameCount || renderedCamera == null || depthTexture == null)
                {
                    Log("Missing same-frame main camera depth at frame " + Time.frameCount
                        + " lastRendered=" + renderedFrame + " main=" + Camera.main
                        + " rendered=" + renderedCamera + " depth=" + depthTexture);
                    foreach(var camera in UnityEngine.Object.FindObjectsOfType<Camera>())
                        Log("Camera " + camera.name + " enabled=" + camera.enabled + " tag=" + camera.tag
                            + " depthMode=" + camera.depthTextureMode + " target=" + camera.targetTexture);
                    Log("Pipeline=" + GraphicsSettings.currentRenderPipeline);
                    Application.Quit(4);
                    yield break;
                }
                var stem = mode + "-" + sample.ToString("00", CultureInfo.InvariantCulture);
                var record = new RoomFrameRecord {
                    weather = mode, index = sample, frame = Time.frameCount,
                    simulationTime = Time.time, realtime = Time.realtimeSinceStartup,
                    width = Screen.width, height = Screen.height,
                    camera = renderedCamera.name,
                    position = renderedCamera.transform.position,
                    rotation = renderedCamera.transform.rotation,
                    worldToCamera = renderedCamera.worldToCameraMatrix,
                    projection = renderedCamera.projectionMatrix,
                    gpuProjection = GL.GetGPUProjectionMatrix(renderedCamera.projectionMatrix, false),
                    near = renderedCamera.nearClipPlane, far = renderedCamera.farClipPlane,
                    reversedZ = SystemInfo.usesReversedZBuffer, depthGraphicsFormat = depthTexture.graphicsFormat.ToString(),
                    graphicsDevice = SystemInfo.graphicsDeviceVersion,
                    color = stem + ".png", depth = stem + "-depth.f32",
                    activeObjects = ActiveObjectPaths()
                };
                var sphere = ReferenceSkyDiagnostic.ActiveSphere;
                var profile = ReferenceSkyDiagnostic.Read(ReferenceSkyDiagnostic.Read(sphere, "atmosphereModule"), "atmosphereProfile");
                record.atmosphereProfile = profile == null ? "" : profile.ToString();
                var clock = ReferenceSkyDiagnostic.Read(ReferenceSkyDiagnostic.Read(sphere, "timeModule"), "currentTime");
                record.weatherClock = clock == null ? "" : clock.ToString();
                record.clockHours = Number(clock, "hours");
                record.clockMinutes = Number(clock, "minutes");
                record.clockSeconds = Number(clock, "seconds");
                record.clockMilliseconds = Number(clock, "milliseconds");
                record.modifiedDayPercentage = Number(ReferenceSkyDiagnostic.Read(sphere, "timeModule"), "modifiedDayPercentage");
                record.shaderValues = CaptureShaderValues(sphere);
                record.transforms = CaptureTransforms();
                record.skins = CaptureSkins();

                var capture = ScreenCapture.CaptureScreenshotAsTexture();
                File.WriteAllBytes(Path.Combine(output, record.color), capture.EncodeToPNG());
                UnityEngine.Object.Destroy(capture);
                File.WriteAllText(Path.Combine(output, stem + ".json"), JsonConvert.SerializeObject(record, Formatting.Indented, new JsonSerializerSettings { ContractResolver = new CaptureFieldsResolver() }));
                outstanding++;
                AsyncGPUReadback.Request(depthTexture, 0, TextureFormat.RFloat, request => {
                    if (request.hasError)
                    {
                        readbackFailures++;
                        Log("Depth readback failed for " + stem);
                    }
                    else
                    {
                        var data = request.GetData<float>();
                        var floats = data.ToArray();
                        var bytes = new byte[floats.Length * sizeof(float)];
                        Buffer.BlockCopy(floats, 0, bytes, 0, bytes.Length);
                        File.WriteAllBytes(Path.Combine(output, record.depth), bytes);
                    }
                    outstanding--;
                });
            }
        }
        while (outstanding > 0) yield return null;
        Time.captureFramerate = 0;
        Log("Completed samples=24 depthFailures=" + readbackFailures);
        Application.Quit(readbackFailures == 0 ? 0 : 5);
    }
}
