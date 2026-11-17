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

    internal sealed class Actor
    {
        public GameObject root;
        public SkinnedMeshRenderer face;
        public Transform[] nodes;
        public AnimatorControllerPlayable controller;
        public PlayableGraph graph;
    }

    internal static Actor Create(StoryBodyControllerProbe.Rig rig, Settings config,
                                AnimatorController controller, float facial, float talk, float look = 1, float plank = .5f)
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
        playable.SetLayerWeight(1, look);
        playable.SetLayerWeight(2, facial);
        playable.SetLayerWeight(3, talk);
        playable.SetLayerWeight(4, plank);
        var path = controller.layers[0].stateMachine.name + ".Base001";
        playable.Play(Animator.StringToHash(path), 0, 0);
        return new Actor { root = nodes[0].gameObject, face = renderer, nodes = nodes, controller = playable, graph = graph };
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

    internal static void SaveGzip(string path, string json)
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

    [Serializable] public sealed class OverlayPose
    {
        public float scale, left, right;
        public Quaternion[] rotations;
        public Vector3[] translations;
    }

    [Serializable] public sealed class OverlayFrame
    {
        public int frame, motion;
        public OverlayPose[] look;
        public OverlayPose plank;
    }

    [Serializable] public sealed class OverlayReport
    {
        public string editor;
        public int sampleRate;
        public string[] joints;
        public OverlayFrame[] frames;
    }

    private static OverlayPose Difference(Actor control, Actor other, int[] joints, float scale, int left, int right)
    {
        var result = new OverlayPose { scale = scale, rotations = new Quaternion[joints.Length],
                                       translations = new Vector3[joints.Length] };
        for (var i = 0; i < joints.Length; i++)
        {
            var from = control.nodes[joints[i]];
            var to = other.nodes[joints[i]];
            result.rotations[i] = Quaternion.Inverse(from.localRotation) * to.localRotation;
            result.translations[i] = to.localPosition - from.localPosition;
        }
        result.left = other.face.GetBlendShapeWeight(left) - control.face.GetBlendShapeWeight(left);
        result.right = other.face.GetBlendShapeWeight(right) - control.face.GetBlendShapeWeight(right);
        return result;
    }

    internal static void SetBody(AnimatorControllerPlayable playable, int motion)
    {
        playable.SetInteger("MotionType", motion);
        if (motion == 400)
        {
            playable.SetTrigger("GameStart_Start");
            playable.Play(Animator.StringToHash("GameStart_Start"), 0, 0);
        }
        else if (motion == 401)
        {
            playable.ResetTrigger("GameStart_Start");
            playable.SetTrigger("GameStart_End");
        }
    }

    [Serializable] public sealed class MuscleSample
    {
        public float[] muscle;
        public Quaternion rotation;
    }

    [Serializable] public sealed class MuscleBasis
    {
        public string joint;
        public string[] muscles;
        public MuscleSample[] samples;
    }

    [Serializable] public sealed class MuscleMotion
    {
        public int frame, motion;
        public MuscleSample[] normal, gaze;
    }

    [Serializable] public sealed class MuscleReport
    {
        public string editor;
        public MuscleBasis[] basis;
        public MuscleMotion[] animated;
    }

    public static void RunHumanoidBasis()
    {
        var folder = Path.GetDirectoryName(Application.dataPath);
        var rig = JsonUtility.FromJson<StoryBodyControllerProbe.Rig>(File.ReadAllText(Path.Combine(folder, "story-body-probe.json")));
        var config = JsonUtility.FromJson<Settings>(File.ReadAllText(Path.Combine(folder, "story-face-probe.json")));
        var controller = AssetDatabase.LoadAssetAtPath<AnimatorController>(rig.controller);
        var calibration = Create(rig, config, controller, 0, 0, 0, 0);
        var normal = Create(rig, config, controller, 0, 0, 0, 0);
        var gaze = Create(rig, config, controller, 0, 0, 1, 0);
        var actors = new[] { calibration, normal, gaze };
        var boneIds = new[] { HumanBodyBones.Chest, HumanBodyBones.Neck, HumanBodyBones.Head };
        var names = new[] {
            new[] { "Chest Left-Right", "Chest Front-Back", "Chest Twist Left-Right" },
            new[] { "Neck Nod Down-Up", "Neck Tilt Left-Right", "Neck Turn Left-Right" },
            new[] { "Head Nod Down-Up", "Head Tilt Left-Right", "Head Turn Left-Right" }
        };
        var muscleIds = Array.ConvertAll(names, trio => Array.ConvertAll(trio, name => Array.IndexOf(HumanTrait.MuscleName, name)));
        if (Array.Exists(muscleIds, trio => Array.Exists(trio, id => id < 0)))
            throw new InvalidOperationException("Missing source gaze muscle in the Avatar");
        var report = new MuscleReport { editor = Application.unityVersion, basis = new MuscleBasis[boneIds.Length],
                                        animated = new MuscleMotion[720] };
        HumanPoseHandler Handler(Actor actor)
        {
            var animator = actor.root.GetComponent<Animator>();
            return new HumanPoseHandler(animator.avatar, animator.transform);
        }
        Transform Bone(Actor actor, int index) => actor.root.GetComponent<Animator>().GetBoneTransform(boneIds[index]);
        MuscleSample[] Capture(Actor actor, HumanPoseHandler handler)
        {
            var pose = new HumanPose();
            handler.GetHumanPose(ref pose);
            var samples = new MuscleSample[boneIds.Length];
            for (var index = 0; index < boneIds.Length; index++)
                samples[index] = new MuscleSample {
                    muscle = Array.ConvertAll(muscleIds[index], id => pose.muscles[id]),
                    rotation = Bone(actor, index).localRotation
                };
            return samples;
        }
        try
        {
            using (var handler = Handler(calibration))
            {
                var pose = new HumanPose();
                calibration.graph.Evaluate(0);
                handler.GetHumanPose(ref pose);
                var grid = new[] { -.8f, -.4f, 0f, .4f, .8f };
                for (var index = 0; index < boneIds.Length; index++)
                {
                    var results = new List<MuscleSample>();
                    foreach (var first in grid)
                    foreach (var second in grid)
                    foreach (var third in grid)
                    {
                        Array.Clear(pose.muscles, 0, pose.muscles.Length);
                        var values = new[] { first, second, third };
                        for (var coordinate = 0; coordinate < values.Length; coordinate++)
                            pose.muscles[muscleIds[index][coordinate]] = values[coordinate];
                        handler.SetHumanPose(ref pose);
                        results.Add(new MuscleSample { muscle = values, rotation = Bone(calibration, index).localRotation });
                    }
                    report.basis[index] = new MuscleBasis { joint = Bone(calibration, index).name,
                                                          muscles = names[index], samples = results.ToArray() };
                }
            }
            using (var source = Handler(normal))
            using (var adjusted = Handler(gaze))
            {
                var commands = new Dictionary<int, int> {
                    {0, 0}, {40, 5}, {95, 1}, {150, 3}, {205, 0}, {280, 400}, {590, 401}, {650, 12}
                };
                gaze.controller.SetFloat("LookScale", .7f);
                var motion = 0;
                for (var frame = 0; frame < report.animated.Length; frame++)
                {
                    if (commands.TryGetValue(frame, out var next))
                    {
                        motion = next;
                        SetBody(normal.controller, motion);
                        SetBody(gaze.controller, motion);
                    }
                    var dt = frame == 0 ? 0 : 1f / 60;
                    normal.graph.Evaluate(dt);
                    gaze.graph.Evaluate(dt);
                    report.animated[frame] = new MuscleMotion {
                        frame = frame, motion = motion, normal = Capture(normal, source), gaze = Capture(gaze, adjusted)
                    };
                }
            }
            SaveGzip(Path.Combine(folder, "story-humanoid-basis.json.gz"), JsonUtility.ToJson(report));
            Debug.Log("Sampled source Avatar muscle basis and " + report.animated.Length + " animated frames");
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

    public static void RunAdditiveLayers()
    {
        var folder = Path.GetDirectoryName(Application.dataPath);
        var rig = JsonUtility.FromJson<StoryBodyControllerProbe.Rig>(File.ReadAllText(Path.Combine(folder, "story-body-probe.json")));
        var config = JsonUtility.FromJson<Settings>(File.ReadAllText(Path.Combine(folder, "story-face-probe.json")));
        var controller = AssetDatabase.LoadAssetAtPath<AnimatorController>(rig.controller);
        if (!controller || controller.layers.Length != 5 || controller.layers[1].name != "LookPlayer" ||
            controller.layers[4].name != "plankshake" || controller.layers[4].defaultWeight != .5f ||
            !controller.layers[1].stateMachine.defaultState.motion || !controller.layers[4].stateMachine.defaultState.motion)
            throw new InvalidOperationException("Expected both authored additive layers");
        var scales = new[] { 0f, .3f, .4f, .5f, .65f, .7f, 1f };
        var control = Create(rig, config, controller, 0, 0, 0, 0);
        var plank = Create(rig, config, controller, 0, 0, 0, .5f);
        var lookActors = Array.ConvertAll(scales, _ => Create(rig, config, controller, 0, 0, 1, 0));
        var actors = new List<Actor> { control, plank };
        actors.AddRange(lookActors);
        var report = new OverlayReport {
            editor = Application.unityVersion, sampleRate = 60, frames = new OverlayFrame[720],
            joints = new[] { "Character_Spine", "Character_Spine1", "Character_Spine2", "Character_Neck", "Character_Head" }
        };
        var joints = Array.ConvertAll(report.joints, name => Array.FindIndex(rig.nodes, node => node.name == name));
        var left = Array.IndexOf(config.names, "blendShape1.Eye_look_left");
        var right = Array.IndexOf(config.names, "blendShape1.Eye_look_right");
        if (left < 0 || right < 0 || Array.Exists(joints, value => value < 0))
            throw new InvalidOperationException("Missing original additive joint bindings");
        var commands = new Dictionary<int, int> {
            {0, 0}, {40, 5}, {95, 1}, {150, 3}, {205, 0}, {280, 400}, {590, 401}, {650, 12}
        };
        var motion = 0;
        try
        {
            for (var index = 0; index < lookActors.Length; index++)
                lookActors[index].controller.SetFloat("LookScale", scales[index]);
            for (var frame = 0; frame < report.frames.Length; frame++)
            {
                if (commands.TryGetValue(frame, out var next))
                {
                    motion = next;
                    foreach (var actor in actors) SetBody(actor.controller, motion);
                }
                foreach (var actor in actors) actor.graph.Evaluate(frame == 0 ? 0 : 1f / report.sampleRate);
                var look = new OverlayPose[scales.Length];
                for (var i = 0; i < scales.Length; i++)
                    look[i] = Difference(control, lookActors[i], joints, scales[i], left, right);
                report.frames[frame] = new OverlayFrame {
                    frame = frame, motion = motion, look = look,
                    plank = Difference(control, plank, joints, .5f, left, right)
                };
            }
            SaveGzip(Path.Combine(folder, "story-additive-layer-trace.json.gz"), JsonUtility.ToJson(report));
            Debug.Log("Sampled " + report.frames.Length + " frames of the original additive layers");
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
                        SetBody(playable, body);
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
