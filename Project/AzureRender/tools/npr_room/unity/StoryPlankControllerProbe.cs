using System;
using System.Collections.Generic;
using System.IO;
using UnityEditor;
using UnityEditor.Animations;
using UnityEngine;

public static class StoryPlankControllerProbe
{
    [Serializable] public sealed class Delta
    {
        public int index;
        public Quaternion before, rotation;
        public Vector3 translation;
    }

    [Serializable] public sealed class Frame
    {
        public int frame, motion;
        public float phase;
        public float[] before, muscles;
        public Delta[] nodes;
    }

    [Serializable] public sealed class Report
    {
        public string editor;
        public string[] joints, muscles;
        public int sampleRate;
        public float seconds, weight, gaze;
        public Frame[] frames;
    }

    public static void Run()
    {
        var folder = Path.GetDirectoryName(Application.dataPath);
        var rig = JsonUtility.FromJson<StoryBodyControllerProbe.Rig>(File.ReadAllText(Path.Combine(folder, "story-body-probe.json")));
        var config = JsonUtility.FromJson<StoryFaceControllerProbe.Settings>(File.ReadAllText(Path.Combine(folder, "story-face-probe.json")));
        var controller = AssetDatabase.LoadAssetAtPath<AnimatorController>(rig.controller);
        if (!controller || controller.layers.Length != 5 || controller.layers[4].name != "plankshake" ||
            controller.layers[4].defaultWeight != .5f)
            throw new InvalidOperationException("Expected the source plankshake layer");
        var clip = controller.layers[4].stateMachine.defaultState.motion as AnimationClip;
        if (!clip || clip.name != "anim_plankshake" || !clip.isLooping)
            throw new InvalidOperationException("Expected the original looping body layer");
        Debug.Log("Original plankshake clip length " + clip.length);
        var look = Environment.GetEnvironmentVariable("AZURE_NPR_PLANK_WITH_LOOK") == "1" ? .7f : 0f;
        var lookLayer = look > 0 ? 1 : 0;
        var control = StoryFaceControllerProbe.Create(rig, config, controller, 0, 0, lookLayer, 0);
        var active = StoryFaceControllerProbe.Create(rig, config, controller, 0, 0, lookLayer, .5f);
        control.controller.SetFloat("LookScale", look);
        active.controller.SetFloat("LookScale", look);
        var left = control.root.GetComponent<Animator>();
        var right = active.root.GetComponent<Animator>();
        var report = new Report {
            editor = Application.unityVersion, sampleRate = 60, seconds = clip.length, weight = .5f, gaze = look,
            joints = Array.ConvertAll(rig.nodes, node => node.name), muscles = HumanTrait.MuscleName,
            frames = new Frame[720]
        };
        var schedule = new Dictionary<int, int> {
            {0, 0}, {40, 5}, {95, 1}, {150, 3}, {205, 0}, {280, 400}, {590, 401}, {650, 12}
        };
        try
        {
            using (var normal = new HumanPoseHandler(left.avatar, left.transform))
            using (var overlay = new HumanPoseHandler(right.avatar, right.transform))
            {
                var first = new HumanPose();
                var second = new HumanPose();
                var motion = 0;
                for (var frame = 0; frame < report.frames.Length; frame++)
                {
                    if (schedule.TryGetValue(frame, out var next))
                    {
                        motion = next;
                        StoryFaceControllerProbe.SetBody(control.controller, motion);
                        StoryFaceControllerProbe.SetBody(active.controller, motion);
                    }
                    var dt = frame == 0 ? 0 : 1f / report.sampleRate;
                    control.graph.Evaluate(dt);
                    active.graph.Evaluate(dt);
                    normal.GetHumanPose(ref first);
                    overlay.GetHumanPose(ref second);
                    var changed = new List<Delta>();
                    for (var index = 0; index < control.nodes.Length; index++)
                    {
                        var a = control.nodes[index];
                        var b = active.nodes[index];
                        var rotation = Quaternion.Inverse(a.localRotation) * b.localRotation;
                        var translation = b.localPosition - a.localPosition;
                        if (translation.sqrMagnitude < 1e-14f &&
                            rotation.x*rotation.x + rotation.y*rotation.y + rotation.z*rotation.z < 1e-14f)
                            continue;
                        changed.Add(new Delta { index = index, before = a.localRotation, rotation = rotation, translation = translation });
                    }
                    var offsets = new float[report.muscles.Length];
                    for (var i = 0; i < offsets.Length; i++) offsets[i] = second.muscles[i] - first.muscles[i];
                    report.frames[frame] = new Frame {
                        frame = frame, motion = motion, phase = active.controller.GetCurrentAnimatorStateInfo(4).normalizedTime,
                        before = (float[])first.muscles.Clone(), muscles = offsets, nodes = changed.ToArray()
                    };
                }
            }
            var target = look > 0 ? "story-plank-gaze-trace.json.gz" : "story-plank-layer-trace.json.gz";
            StoryFaceControllerProbe.SaveGzip(Path.Combine(folder, target), JsonUtility.ToJson(report));
            Debug.Log("Sampled " + report.frames.Length + " frames of the original plankshake layer");
        }
        finally
        {
            foreach (var actor in new[] {control, active})
            {
                actor.graph.Destroy();
                UnityEngine.Object.DestroyImmediate(actor.face.sharedMesh);
                UnityEngine.Object.DestroyImmediate(actor.root);
            }
        }
    }
}
