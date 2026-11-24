using System;
using System.Collections.Generic;
using UnityEngine;

public sealed class TimerVoiceCueRecorder : MonoBehaviour
{
    [Serializable] public sealed class Cue
    {
        public int cue;
        public double seconds;
    }

    public double SampleTime { get; set; }
    public List<Cue> Cues { get; } = new List<Cue>();

    public void PlayPomodoroVoice(int cue)
    {
        Cues.Add(new Cue { cue = cue, seconds = SampleTime });
    }
}
