using System;
using System.Collections.Generic;
using System.IO;
using UnityEditor;
using UnityEditor.Animations;
using UnityEngine;
using UnityEngine.Animations;
using UnityEngine.Playables;

public static class StoryBodyControllerProbe
{
    [Serializable] public sealed class Node
    {
        public int parent;
        public string name;
        public Vector3 position, scale;
        public Quaternion rotation;
    }

    [Serializable] public sealed class Rig
    {
        public int animatorNode;
        public string avatar, controller;
        public bool tutorialCamera;
        public Node[] nodes;
    }

    [Serializable] public sealed class Frame
    {
        public int frame, motion;
        public string current, next;
        public float currentTime, nextTime, blendTime, blendDuration;
    }

    [Serializable] public sealed class Report
    {
        public string editor;
        public int sampleRate, totalFrames;
        public List<Frame> frames = new List<Frame>();
    }

    private static void Index(AnimatorStateMachine machine, string path, Dictionary<int, string> names)
    {
        foreach (var child in machine.states)
        {
            var fullName = path + "." + child.state.name;
            names.Add(Animator.StringToHash(fullName), fullName);
        }
        foreach (var child in machine.stateMachines)
            Index(child.stateMachine, path + "." + child.stateMachine.name, names);
    }

    public static void Run()
    {
        var project = Path.GetDirectoryName(Application.dataPath);
        var rig = JsonUtility.FromJson<Rig>(File.ReadAllText(Path.Combine(project, "story-body-probe.json")));
        var controller = AssetDatabase.LoadAssetAtPath<AnimatorController>(rig.controller);
        if (!controller || controller.layers.Length != 5 || controller.layers[0].name != "Base Layer")
            throw new InvalidOperationException("Expected the imported five-layer source controller");
        var root = controller.layers[0].stateMachine;
        var names = new Dictionary<int, string>();
        Index(root, root.name, names);
        var transforms = new Transform[rig.nodes.Length];
        for (var index = 0; index < rig.nodes.Length; ++index)
        {
            var node = rig.nodes[index];
            var transform = new GameObject(node.name).transform;
            if (node.parent >= 0) transform.SetParent(transforms[node.parent], false);
            transform.localPosition = node.position;
            transform.localRotation = node.rotation;
            transform.localScale = node.scale;
            transforms[index] = transform;
        }
        var animator = transforms[rig.animatorNode].gameObject.AddComponent<Animator>();
        animator.avatar = AssetDatabase.LoadAssetAtPath<Avatar>("Assets/" + rig.avatar);
        if (!animator.avatar || !animator.avatar.isValid || !animator.avatar.isHuman)
            throw new InvalidOperationException("Expected the original Humanoid Avatar");
        animator.applyRootMotion = false;
        animator.fireEvents = false;
        animator.cullingMode = AnimatorCullingMode.AlwaysAnimate;

        // Each early return arrives before its entrance sequence has finished.
        var schedule = new Dictionary<int, int>
        {
            {0, 0}, {15, 5}, {40, 0}, {285, 1}, {330, 0}, {485, 3}, {550, 0},
            {720, 1000}, {735, 1003}, {1020, 0}
        };
        if (rig.tutorialCamera)
        {
            schedule = new Dictionary<int, int>
            {
                {0, 0}, {30, 400}, {335, 401}, {370, 12}, {550, 15}, {640, 0}, {790, 3}, {865, 0}
            };
        }
        var report = new Report { editor = Application.unityVersion, sampleRate = 60,
                                  totalFrames = rig.tutorialCamera ? 1030 : 1140 };
        var graph = PlayableGraph.Create("Original opening body controller");
        try
        {
            graph.SetTimeUpdateMode(DirectorUpdateMode.Manual);
            var playable = AnimatorControllerPlayable.Create(graph, controller);
            AnimationPlayableOutput.Create(graph, "Base layer", animator).SetSourcePlayable(playable);
            graph.Play();
            playable.Play(Animator.StringToHash(root.name + ".Base001"), 0, 0);
            string priorCurrent = null, priorNext = null;
            var motion = 0;
            for (var frame = 0; frame < report.totalFrames; ++frame)
            {
                var hasCommand = schedule.TryGetValue(frame, out var command);
                if (hasCommand)
                {
                    motion = command;
                    var trigger = motion == 1003 ? "Story_SubBase001_Fun" :
                                  motion == 400 ? "GameStart_Start" : motion == 401 ? "GameStart_End" : null;
                    if (trigger != null)
                    {
                        playable.ResetTrigger("GameStart_Start");
                        playable.ResetTrigger("GameStart_End");
                        playable.ResetTrigger("Story_SubBase001_Fun");
                        playable.SetTrigger(trigger);
                    }
                    playable.SetInteger("MotionType", motion);
                    if (motion == 400) playable.Play(Animator.StringToHash("GameStart_Start"), 0, 0);
                }
                graph.Evaluate(frame == 0 ? 0 : 1f / report.sampleRate);
                var current = playable.GetCurrentAnimatorStateInfo(0);
                var next = playable.GetNextAnimatorStateInfo(0);
                var transition = playable.GetAnimatorTransitionInfo(0);
                var currentName = names[current.fullPathHash];
                var nextName = playable.IsInTransition(0) ? names[next.fullPathHash] : "";
                if (hasCommand || currentName != priorCurrent || nextName != priorNext)
                {
                    var clips = playable.GetCurrentAnimatorClipInfo(0);
                    if (clips.Length != 1 || !clips[0].clip.humanMotion ||
                        (nextName != "" && playable.GetNextAnimatorClipInfo(0).Length != 1))
                        throw new InvalidOperationException("Expected a source Humanoid clip at " + currentName + " -> " + nextName);
                    report.frames.Add(new Frame { frame = frame, motion = motion, current = currentName, next = nextName,
                        currentTime = current.normalizedTime, nextTime = next.normalizedTime,
                        blendTime = transition.normalizedTime, blendDuration = transition.duration });
                }
                priorCurrent = currentName;
                priorNext = nextName;
            }
        }
        finally
        {
            graph.Destroy();
            UnityEngine.Object.DestroyImmediate(transforms[0].gameObject);
        }
        File.WriteAllText(Path.Combine(project, "story-body-controller-trace.json"), JsonUtility.ToJson(report, true));
    }
}
