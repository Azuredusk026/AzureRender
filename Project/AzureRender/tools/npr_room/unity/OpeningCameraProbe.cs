using System;
using System.IO;
using UnityEditor;
using UnityEditor.Animations;
using UnityEngine;

public static class OpeningCameraProbe
{
    [Serializable] public sealed class Config
    {
        public int root, camera, anchor;
        public string state, controller, clip;
        public StoryBodyControllerProbe.Node[] nodes;
    }

    [Serializable] public sealed class Pose
    {
        public Vector3 position;
        public Quaternion rotation;
    }

    [Serializable] public sealed class Report
    {
        public string editor, state;
        public int sampleRate;
        public float seconds;
        public Pose initial, anchor, rebind, disabled;
        public Pose[] samples;
    }

    public static void Run()
    {
        var folder = Path.GetDirectoryName(Application.dataPath);
        var config = JsonUtility.FromJson<Config>(File.ReadAllText(Path.Combine(folder, "opening-camera-probe.json")));
        var nodes = new Transform[config.nodes.Length];
        for (var i = 0; i < nodes.Length; ++i)
        {
            var source = config.nodes[i];
            var node = new GameObject(source.name).transform;
            if (source.parent >= 0) node.SetParent(nodes[source.parent], false);
            node.localPosition = source.position;
            node.localRotation = source.rotation;
            node.localScale = source.scale;
            nodes[i] = node;
        }

        var camera = nodes[config.camera];
        var anchor = nodes[config.anchor];
        Pose Current() => new Pose { position = camera.position, rotation = camera.rotation };
        var report = new Report { editor = Application.unityVersion, sampleRate = 60, state = config.state,
                                  initial = Current() };
        var controller = AssetDatabase.LoadAssetAtPath<AnimatorController>(config.controller);
        var clip = AssetDatabase.LoadAssetAtPath<AnimationClip>(config.clip);
        if (!controller || !clip || clip.isLooping || clip.frameRate != report.sampleRate)
            throw new InvalidOperationException("Expected the original Windows camera clip");
        var hash = Animator.StringToHash(config.state);
        var animator = nodes[config.root].gameObject.AddComponent<Animator>();
        animator.enabled = false;
        animator.cullingMode = AnimatorCullingMode.AlwaysAnimate;
        animator.runtimeAnimatorController = controller;
        animator.Rebind();
        camera.SetPositionAndRotation(anchor.position, anchor.rotation);
        report.anchor = Current();
        animator.enabled = true;
        if (!animator.HasState(0, hash)) throw new InvalidOperationException("Missing Windows camera state");
        animator.Play(hash, 0, 0);
        report.seconds = clip.length;
        report.samples = new Pose[Mathf.RoundToInt(clip.length * report.sampleRate) + 31];
        for (var i = 0; i < report.samples.Length; ++i)
        {
            animator.Update(i == 0 ? 0 : 1f / report.sampleRate);
            report.samples[i] = Current();
        }
        animator.Rebind();
        report.rebind = Current();
        animator.enabled = false;
        report.disabled = Current();
        File.WriteAllText(Path.Combine(folder, "opening-camera-unity-trace.json"), JsonUtility.ToJson(report));
        Debug.Log("Sampled " + report.samples.Length + " frames of the original Windows camera controller");
    }
}
