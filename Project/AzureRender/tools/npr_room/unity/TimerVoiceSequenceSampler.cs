using System;
using System.Collections.Generic;
using System.IO;
using System.IO.Compression;
using UnityEditor;
using UnityEditor.Animations;
using UnityEngine;
using UnityEngine.Animations;
using UnityEngine.Playables;

public static class TimerVoiceSequenceSampler
{
    [Serializable] public sealed class Node
    {
        public int parent;
        public string name;
        public Vector3 position, scale;
        public Quaternion rotation;
    }

    [Serializable] public sealed class Transition
    {
        public float exitTime, duration, offset;
        public bool hasExitTime, fixedDuration;
    }

    [Serializable] public sealed class Rig
    {
        public int animatorNode;
        public string avatar;
        public Node[] nodes;
        public string[] clips, sourceHashes;
        public Transition[] transitions;
    }

    [Serializable] public sealed class Frame
    {
        public double time;
        public Vector3[] positions, scales;
        public Quaternion[] rotations;
    }

    [Serializable] public sealed class Clip
    {
        public string name = "timer_voice_motion_67";
        public bool human = true;
        public float length, rate = 60;
        public List<Frame> frames = new List<Frame>();
    }

    [Serializable] public sealed class Sequence
    {
        public int motion = 67;
        public string clip;
        public string[] sourceHashes;
        public List<TimerVoiceCueRecorder.Cue> cues;
    }

    [Serializable] public sealed class Result
    {
        public string version;
        public Clip[] clips;
        public Sequence sequence;
    }

    public static void Run()
    {
        var project = Path.GetDirectoryName(Application.dataPath);
        var rig = JsonUtility.FromJson<Rig>(File.ReadAllText(Path.Combine(project, "timer-voice-rig-batchD.json")));
        if (rig.clips.Length != 3 || rig.transitions.Length != 2)
            throw new InvalidOperationException("Expected three original states and two transitions for motion 67");
        var transforms = new Transform[rig.nodes.Length];
        for (var index = 0; index < rig.nodes.Length; ++index)
        {
            var node = rig.nodes[index];
            var current = new GameObject(node.name).transform;
            if (node.parent >= 0) current.SetParent(transforms[node.parent], false);
            current.localPosition = node.position;
            current.localRotation = node.rotation;
            current.localScale = node.scale;
            transforms[index] = current;
        }
        var root = transforms[rig.animatorNode].gameObject;
        var recorder = root.AddComponent<TimerVoiceCueRecorder>();
        var animator = root.AddComponent<Animator>();
        animator.avatar = AssetDatabase.LoadAssetAtPath<Avatar>("Assets/" + rig.avatar);
        if (!animator.avatar || !animator.avatar.isHuman || !animator.avatar.isValid)
            throw new InvalidOperationException("Recovery Avatar failed Humanoid import");
        animator.applyRootMotion = false;
        animator.fireEvents = true;
        animator.cullingMode = AnimatorCullingMode.AlwaysAnimate;

        var target = AssetDatabase.GenerateUniqueAssetPath("Assets/TimerVoiceSequence.controller");
        var controller = AnimatorController.CreateAnimatorControllerAtPath(target);
        var stateMachine = controller.layers[0].stateMachine;
        var states = new List<AnimatorState>();
        foreach (var relative in rig.clips)
        {
            var source = AssetDatabase.LoadAssetAtPath<AnimationClip>("Assets/" + relative);
            if (!source || !source.humanMotion || source.length <= 0)
                throw new InvalidOperationException("Recovery clip failed Humanoid import: " + relative);
            var clip = UnityEngine.Object.Instantiate(source);
            clip.events = Array.FindAll(source.events, value => value.functionName == nameof(TimerVoiceCueRecorder.PlayPomodoroVoice));
            var state = stateMachine.AddState(source.name);
            state.motion = clip;
            state.iKOnFeet = false;
            state.writeDefaultValues = true;
            states.Add(state);
        }
        stateMachine.defaultState = states[0];
        for (var index = 0; index < rig.transitions.Length; ++index)
        {
            var from = rig.transitions[index];
            var edge = states[index].AddTransition(states[index + 1]);
            edge.hasExitTime = from.hasExitTime;
            edge.exitTime = from.exitTime;
            edge.hasFixedDuration = from.fixedDuration;
            edge.duration = from.duration;
            edge.offset = from.offset;
            edge.interruptionSource = TransitionInterruptionSource.None;
            edge.orderedInterruption = true;
        }

        var sampled = new Clip();
        var graph = PlayableGraph.Create("Original timer voice sequence");
        try
        {
            graph.SetTimeUpdateMode(DirectorUpdateMode.Manual);
            var playable = AnimatorControllerPlayable.Create(graph, controller);
            AnimationPlayableOutput.Create(graph, "Timer motion", animator).SetSourcePlayable(playable);
            graph.Play();
            for (var index = 0; index <= 18 * sampled.rate; ++index)
            {
                recorder.SampleTime = index / (double)sampled.rate;
                graph.Evaluate(index == 0 ? 0 : 1 / sampled.rate);
                var frame = new Frame
                {
                    time = recorder.SampleTime,
                    positions = new Vector3[transforms.Length],
                    rotations = new Quaternion[transforms.Length],
                    scales = new Vector3[transforms.Length]
                };
                for (var joint = 0; joint < transforms.Length; ++joint)
                {
                    frame.positions[joint] = transforms[joint].localPosition;
                    frame.rotations[joint] = transforms[joint].localRotation;
                    frame.scales[joint] = transforms[joint].localScale;
                }
                sampled.frames.Add(frame);
                var state = playable.GetCurrentAnimatorStateInfo(0);
                if (state.shortNameHash == states[2].nameHash && state.normalizedTime >= 1)
                {
                    sampled.length = (float)frame.time;
                    break;
                }
            }
        }
        finally
        {
            graph.Destroy();
        }
        if (sampled.length <= 0 || !recorder.Cues.Exists(value => value.cue == 1)
            || !recorder.Cues.Exists(value => value.cue == 2))
            throw new InvalidOperationException("Original timer sequence failed to reach its end and emit both cues");
        var result = new Result
        {
            version = Application.unityVersion,
            clips = new[] { sampled },
            sequence = new Sequence { clip = sampled.name, sourceHashes = rig.sourceHashes, cues = recorder.Cues }
        };
        using (var file = new FileStream(Path.Combine(project, "timer-voice-samples-batchD.json.gz"), FileMode.CreateNew))
        using (var compressed = new GZipStream(file, System.IO.Compression.CompressionLevel.Optimal))
        using (var output = new StreamWriter(compressed))
            output.Write(JsonUtility.ToJson(result));
    }
}
