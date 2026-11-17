using System;
using System.Collections.Generic;
using System.IO;
using System.IO.Compression;
using UnityEditor;
using UnityEditor.Animations;
using UnityEngine;
using UnityEngine.Animations;
using UnityEngine.Playables;

public static class StoryFaceControllerProbe
{
    [Serializable] public sealed class Settings
    {
        public string[] bodyClips, names;
        public float[] defaults;
    }

    [Serializable] public sealed class Frame
    {
        public int number, body, facialRequested, facialCurrent, faceNext, talkCurrent, talkNext;
        public float facialTime, faceNextTime, faceBlend, talkTime, talkNextTime, talkBlend;
        public bool talking;
        public float[] weights, baseWeights, faceWeights;
    }

    [Serializable] public sealed class Report
    {
        public string editor;
        public int sampleRate;
        public string[] targets;
        public Frame[] frames;
    }

    private sealed class Actor
    {
        public GameObject root;
        public SkinnedMeshRenderer face;
        public AnimatorControllerPlayable controller;
        public PlayableGraph graph;
    }

    private static Actor Create(StoryBodyControllerProbe.Rig rig, Settings config,
                                AnimatorController controller, float facial, float talk)
    {
        var nodes = new Transform[rig.nodes.Length];
        for (var i = 0; i < nodes.Length; i++)
        {
            var source = rig.nodes[i];
            var node = new GameObject(source.name).transform;
            if (source.parent >= 0) node.SetParent(nodes[source.parent], false);
            node.localPosition = source.position;
            node.localRotation = source.rotation;
            node.localScale = source.scale;
            nodes[i] = node;
        }
        var root = nodes[rig.animatorNode];
        var renderer = root.Find("Character/Face").gameObject.AddComponent<SkinnedMeshRenderer>();
        var mesh = new Mesh { vertices = new[] { Vector3.zero, Vector3.right, Vector3.up }, triangles = new[] { 0, 1, 2 } };
        var delta = new[] { Vector3.forward, Vector3.zero, Vector3.zero };
        for (var i = 0; i < config.names.Length; i++)
            mesh.AddBlendShapeFrame(config.names[i], 100, delta, null, null);
        renderer.sharedMesh = mesh;
        renderer.updateWhenOffscreen = true;
        for (var i = 0; i < config.defaults.Length; i++) renderer.SetBlendShapeWeight(i, config.defaults[i]);
        var animator = root.gameObject.AddComponent<Animator>();
        animator.avatar = AssetDatabase.LoadAssetAtPath<Avatar>("Assets/" + rig.avatar);
        animator.cullingMode = AnimatorCullingMode.AlwaysAnimate;
        animator.fireEvents = false;
        var graph = PlayableGraph.Create("Original face, body and voice layers");
        graph.SetTimeUpdateMode(DirectorUpdateMode.Manual);
        var playable = AnimatorControllerPlayable.Create(graph, controller);
        AnimationPlayableOutput.Create(graph, "Face", animator).SetSourcePlayable(playable);
        graph.Play();
        playable.SetLayerWeight(2, facial);
        playable.SetLayerWeight(3, talk);
        var path = controller.layers[0].stateMachine.name + ".Base001";
        playable.Play(Animator.StringToHash(path), 0, 0);
        return new Actor { root = nodes[0].gameObject, face = renderer, controller = playable, graph = graph };
    }

    private static float[] Weights(Actor actor, int size)
    {
        var values = new float[size];
        for (var i = 0; i < size; i++) values[i] = actor.face.GetBlendShapeWeight(i);
        return values;
    }

    [Serializable] public sealed class ClipFrame
    {
        public float seconds;
        public float[] weights;
    }

    [Serializable] public sealed class ClipSamples
    {
        public string name;
        public float duration;
        public ClipFrame[] frames;
    }

    [Serializable] public sealed class BodyClipReport
    {
        public string editor;
        public string[] targets;
        public ClipSamples[] clips;
    }

    private static void SaveGzip(string path, string json)
    {
        using (var stream = File.Create(path))
        using (var gzip = new GZipStream(stream, System.IO.Compression.CompressionLevel.Optimal))
        using (var writer = new StreamWriter(gzip))
            writer.Write(json);
    }

    public static void RunBodyClips()
    {
        var folder = Path.GetDirectoryName(Application.dataPath);
        var rig = JsonUtility.FromJson<StoryBodyControllerProbe.Rig>(File.ReadAllText(Path.Combine(folder, "story-body-probe.json")));
        var config = JsonUtility.FromJson<Settings>(File.ReadAllText(Path.Combine(folder, "story-face-probe.json")));
        var controller = AssetDatabase.LoadAssetAtPath<AnimatorController>(rig.controller);
        if (config.bodyClips.Length == 0) throw new InvalidOperationException("Expected locked opening body clips");
        var report = new BodyClipReport { editor = Application.unityVersion, targets = config.names,
                                          clips = new ClipSamples[config.bodyClips.Length] };
        for (var index = 0; index < config.bodyClips.Length; index++)
        {
            var clip = AssetDatabase.LoadAssetAtPath<AnimationClip>(config.bodyClips[index]);
            if (!clip || clip.length <= 0) throw new InvalidOperationException(config.bodyClips[index]);
            var actor = Create(rig, config, controller, 0, 0);
            actor.graph.Destroy();
            var animator = actor.root.GetComponent<Animator>();
            var graph = PlayableGraph.Create("Direct body clip face binding");
            try
            {
                graph.SetTimeUpdateMode(DirectorUpdateMode.Manual);
                var playable = AnimationClipPlayable.Create(graph, clip);
                AnimationPlayableOutput.Create(graph, "Face", animator).SetSourcePlayable(playable);
                graph.Play();
                var frames = new List<ClipFrame>();
                for (var frame = 0; frame / 60f < clip.length - .000001f; frame++)
                {
                    var time = frame / 60f;
                    playable.SetTime(time);
                    graph.Evaluate(0);
                    frames.Add(new ClipFrame { seconds = time, weights = Weights(actor, config.names.Length) });
                }
                report.clips[index] = new ClipSamples { name = clip.name, duration = clip.length, frames = frames.ToArray() };
            }
            finally
            {
                graph.Destroy();
                UnityEngine.Object.DestroyImmediate(actor.face.sharedMesh);
                UnityEngine.Object.DestroyImmediate(actor.root);
            }
        }
        SaveGzip(Path.Combine(folder, "story-body-face-clips.json.gz"), JsonUtility.ToJson(report));
        Debug.Log("Sampled face bindings in " + report.clips.Length + " original body clips");
    }

    public static void Run()
    {
        var folder = Path.GetDirectoryName(Application.dataPath);
        var rig = JsonUtility.FromJson<StoryBodyControllerProbe.Rig>(File.ReadAllText(Path.Combine(folder, "story-body-probe.json")));
        var config = JsonUtility.FromJson<Settings>(File.ReadAllText(Path.Combine(folder, "story-face-probe.json")));
        var controller = AssetDatabase.LoadAssetAtPath<AnimatorController>(rig.controller);
        if (!controller || controller.layers.Length != 5 || controller.layers[2].name != "Facial" ||
            controller.layers[3].name != "Talk" || config.names.Length != 57)
            throw new InvalidOperationException("Expected the original facial and talk layers");

        var bodyCommands = new Dictionary<int, int> { {0, 0}, {45, 400}, {349, 401}, {388, 12}, {556, 0} };
        var faceCommands = new Dictionary<int, int>
        {
            {0, 0}, {15, 15}, {70, 2}, {77, 13}, {182, 5}, {240, 1}, {330, 0}, {358, 18},
            {402, 14}, {504, 6}, {553, 0}
        };
        var talkCommands = new Dictionary<int, bool>
        {
            {0, false}, {22, true}, {52, false}, {107, true}, {155, false},
            {247, true}, {251, false}, {261, true}, {325, false}, {419, true}, {480, false}
        };
        var actors = new[] { Create(rig, config, controller, 1, 1), Create(rig, config, controller, 0, 0),
                             Create(rig, config, controller, 1, 0) };
        var report = new Report { editor = Application.unityVersion, sampleRate = 60, targets = config.names,
                                  frames = new Frame[631] };
        var body = 0;
        var facial = 0;
        var talking = false;
        try
        {
            for (var frame = 0; frame < report.frames.Length; frame++)
            {
                foreach (var actor in actors)
                {
                    var playable = actor.controller;
                    if (bodyCommands.TryGetValue(frame, out var motion))
                    {
                        body = motion;
                        playable.SetInteger("MotionType", body);
                        if (body == 400)
                        {
                            playable.SetTrigger("GameStart_Start");
                            playable.Play(Animator.StringToHash("GameStart_Start"), 0, 0);
                        }
                        else if (body == 401)
                        {
                            playable.ResetTrigger("GameStart_Start");
                            playable.SetTrigger("GameStart_End");
                        }
                    }
                    if (faceCommands.TryGetValue(frame, out var expression)) playable.SetInteger("Facial", facial = expression);
                    if (talkCommands.TryGetValue(frame, out var voice)) playable.SetBool("Enable_Talk", talking = voice);
                    actor.graph.Evaluate(frame == 0 ? 0 : 1f / report.sampleRate);
                }
                var active = actors[0].controller;
                var currentFace = active.GetCurrentAnimatorStateInfo(2);
                var nextFace = active.GetNextAnimatorStateInfo(2);
                var currentTalk = active.GetCurrentAnimatorStateInfo(3);
                var nextTalk = active.GetNextAnimatorStateInfo(3);
                report.frames[frame] = new Frame {
                    number = frame, body = body, facialRequested = facial, talking = talking,
                    facialCurrent = currentFace.shortNameHash, faceNext = nextFace.shortNameHash,
                    facialTime = currentFace.normalizedTime, faceNextTime = nextFace.normalizedTime,
                    faceBlend = active.GetAnimatorTransitionInfo(2).normalizedTime,
                    talkCurrent = currentTalk.shortNameHash, talkNext = nextTalk.shortNameHash,
                    talkTime = currentTalk.normalizedTime, talkNextTime = nextTalk.normalizedTime,
                    talkBlend = active.GetAnimatorTransitionInfo(3).normalizedTime,
                    weights = Weights(actors[0], config.names.Length),
                    baseWeights = Weights(actors[1], config.names.Length),
                    faceWeights = Weights(actors[2], config.names.Length)
                };
            }
            SaveGzip(Path.Combine(folder, "story-face-controller-trace.json.gz"), JsonUtility.ToJson(report));
            Debug.Log("Sampled " + report.frames.Length + " frames of the original face and talk controller");
        }
        finally
        {
            foreach (var actor in actors)
            {
                actor.graph.Destroy();
                UnityEngine.Object.DestroyImmediate(actor.face.sharedMesh);
                UnityEngine.Object.DestroyImmediate(actor.root);
            }
        }
    }
}
