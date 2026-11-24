using System;
using System.Collections.Generic;
using System.IO;
using UnityEditor;
using UnityEngine;
using UnityEngine.Animations;
using UnityEngine.Playables;

public static class TimerHumanoidSampler
{
    [Serializable] public sealed class RigNode
    {
        public int fileId, parent;
        public string name;
        public Vector3 position, scale;
        public Quaternion rotation;
    }

    [Serializable] public sealed class Rig
    {
        public int animatorNode;
        public string avatar;
        public string[] clips;
        public RigNode[] nodes;
    }

    [Serializable] public sealed class ClipResult
    {
        public string name;
        public float length, rate;
        public bool human;
        public List<Frame> frames = new List<Frame>();
    }

    [Serializable] public sealed class Frame
    {
        public double time;
        public Vector3[] positions, scales;
        public Quaternion[] rotations;
    }

    [Serializable] public sealed class Result
    {
        public string version;
        public string[] controlBones;
        public ClipResult[] clips;
    }

    public static void Run()
    {
        string project = Path.GetDirectoryName(Application.dataPath);
        string batch = Environment.GetEnvironmentVariable("AZURE_NPR_MOTION_BATCH");
        Rig input = JsonUtility.FromJson<Rig>(File.ReadAllText(Path.Combine(project, "timer-voice-rig-" + batch + ".json")));
        Transform[] transforms = new Transform[input.nodes.Length];
        for (int i = 0; i < input.nodes.Length; ++i)
        {
            RigNode node = input.nodes[i];
            Transform target = new GameObject(node.name).transform;
            if (node.parent >= 0)
                target.SetParent(transforms[node.parent], false);
            transforms[i] = target;
        }
        Restore(input, transforms);
        Animator animator = transforms[input.animatorNode].gameObject.AddComponent<Animator>();
        animator.avatar = AssetDatabase.LoadAssetAtPath<Avatar>("Assets/" + input.avatar);
        if (!animator.avatar || !animator.avatar.isHuman || !animator.avatar.isValid)
            throw new InvalidOperationException("Recovery Avatar failed Humanoid import");
        animator.applyRootMotion = false;
        animator.fireEvents = false;
        animator.cullingMode = AnimatorCullingMode.AlwaysAnimate;
        var results = new Result { version = Application.unityVersion, clips = new ClipResult[input.clips.Length] };
        for (int i = 0; i < input.clips.Length; ++i)
        {
            AnimationClip clip = AssetDatabase.LoadAssetAtPath<AnimationClip>("Assets/" + input.clips[i]);
            if (!clip || !clip.humanMotion || clip.length <= 0)
                throw new InvalidOperationException("Recovery clip failed Humanoid import: " + input.clips[i]);
            Restore(input, transforms);
            animator.Rebind();
            PlayableGraph graph = PlayableGraph.Create("Recovery original poses");
            try
            {
                graph.SetTimeUpdateMode(DirectorUpdateMode.Manual);
                AnimationClipPlayable playable = AnimationClipPlayable.Create(graph, clip);
                playable.SetApplyFootIK(false);
                playable.SetApplyPlayableIK(false);
                AnimationPlayableOutput.Create(graph, clip.name, animator).SetSourcePlayable(playable);
                graph.Play();
                var result = new ClipResult { name = clip.name, length = clip.length, rate = clip.frameRate, human = clip.humanMotion };
                int intervals = (int)Math.Ceiling(clip.length * (double)clip.frameRate - 1e-4);
                for (int key = 0; key <= intervals; ++key)
                {
                    double sampleTime = key == intervals ? clip.length : key / (double)clip.frameRate;
                    var frame = new Frame { time = sampleTime, positions = new Vector3[transforms.Length],
                                           rotations = new Quaternion[transforms.Length], scales = new Vector3[transforms.Length] };
                    playable.SetTime(frame.time);
                    graph.Evaluate(0);
                    for (int j = 0; j < transforms.Length; ++j)
                    {
                        frame.positions[j] = transforms[j].localPosition;
                        frame.rotations[j] = transforms[j].localRotation;
                        frame.scales[j] = transforms[j].localScale;
                    }
                    result.frames.Add(frame);
                }
                results.clips[i] = result;
            }
            finally
            {
                graph.Destroy();
            }
        }
        results.controlBones = Array.ConvertAll(new[] { HumanBodyBones.Hips, HumanBodyBones.Head, HumanBodyBones.LeftHand,
                                                      HumanBodyBones.RightFoot },
            bone => animator.GetBoneTransform(bone).name);
        using (var target = new FileStream(Path.Combine(project, "timer-voice-samples-" + batch + ".json.gz"), FileMode.CreateNew))
        using (var compressed = new System.IO.Compression.GZipStream(target, System.IO.Compression.CompressionLevel.Optimal))
        using (var output = new StreamWriter(compressed, new System.Text.UTF8Encoding(false)))
            output.Write(JsonUtility.ToJson(results, false));
    }

    private static void Restore(Rig rig, Transform[] transforms)
    {
        for (int i = 0; i < transforms.Length; ++i)
        {
            transforms[i].localPosition = rig.nodes[i].position;
            transforms[i].localRotation = rig.nodes[i].rotation;
            transforms[i].localScale = rig.nodes[i].scale;
        }
    }
}
