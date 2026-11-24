"""Run inside RenderDoc's embedded Python; the parent supplies an owned request."""
import hashlib
import json
import os
from pathlib import Path
import traceback
import renderdoc as rd


request = json.loads(Path(os.environ["AZURE_ORDER_REPLAY_REQUEST"]).read_text(encoding="utf-8-sig"))
output = Path(request["output"])
reference = request["reference"]
report = dict(status="failed", variants=[])
capture = controller = original = replacement = None
sha = lambda data: hashlib.sha256(data).hexdigest()


def differences(first, second):
    if len(first) != len(second):
        raise ValueError("Output byte lengths differ")
    return sum(a != b for a, b in zip(first, second))


try:
    capture = rd.OpenCaptureFile()
    assert capture.OpenFile(request["capture"], "", None) == rd.ResultCode.Succeeded
    status, controller = capture.OpenCapture(rd.ReplayOptions(), None)
    assert status == rd.ResultCode.Succeeded
    assert rd.ShaderEncoding.DXBC in controller.GetTargetShaderEncodings()
    textures = {str(texture.resourceId): texture for texture in controller.GetTextures()}

    def replay():
        controller.SetFrameEvent(0, True)
        controller.SetFrameEvent(reference["event"], True)
        values = {}
        for row in reference["outputs"]:
            texture = textures["ResourceId::{}".format(row["resource"])]
            values[row["resource"]] = bytes(controller.GetTextureData(texture.resourceId, rd.Subresource(0, 0, 0)))
        return values

    native = replay()
    pipeline = controller.GetPipelineState()
    original = pipeline.GetShader(rd.ShaderStage.Compute)
    raw = bytes(pipeline.GetShaderReflection(rd.ShaderStage.Compute).rawBytes)
    assert sha(raw) == reference["originalShaderSha256"]
    for row in reference["outputs"]:
        data = native[row["resource"]]
        assert len(data) == row["bytes"] and sha(data) == row["sha256"]
        (output / "native-{}.bin".format(row["resource"])).write_bytes(data)
    first = reference["outputs"][0]["resource"]
    damaged = bytearray(native[first])
    damaged[0] ^= 1
    assert differences(native[first], damaged) == 1
    report["singleByteNegativeControl"] = "rejected"
    for variant in request["variants"]:
        data = Path(variant["path"]).read_bytes()
        assert sha(data) == variant["sha256"]
        replacement, diagnostic = controller.BuildTargetShader(
            "main", rd.ShaderEncoding.DXBC, data, rd.ShaderCompileFlags(), rd.ShaderStage.Compute)
        assert replacement != rd.ResourceId.Null(), diagnostic
        controller.ReplaceResource(original, replacement)
        actual = replay()
        row = dict(label=variant["label"], binarySha256=sha(data), outputs=[])
        for source in reference["outputs"]:
            resource = source["resource"]
            payload = actual[resource]
            count = differences(native[resource], payload)
            row["outputs"].append(dict(resource=resource, bytes=len(payload), differentBytes=count,
                                       sha256=sha(payload)))
            (output / "{}-{}.bin".format(row["label"], resource)).write_bytes(payload)
        row["kernelByteEquivalence"] = all(item["differentBytes"] == 0 for item in row["outputs"])
        report["variants"].append(row)
        controller.RemoveReplacement(original)
        controller.FreeTargetResource(replacement)
        replacement = None
        (output / "result.json").write_text(json.dumps(report, indent=2), encoding="utf-8")
    control, normalized = report["variants"]
    assert [row["differentBytes"] for row in control["outputs"]] == reference["expectedControlDifferences"]
    assert not control["kernelByteEquivalence"] and normalized["kernelByteEquivalence"]
    restored = replay()
    report["nativeRestoredExact"] = restored == native
    assert report["nativeRestoredExact"]
    report["status"] = "passed"
except Exception:
    report["error"] = traceback.format_exc()
finally:
    if controller and replacement and replacement != rd.ResourceId.Null():
        controller.RemoveReplacement(original)
        controller.FreeTargetResource(replacement)
    if controller:
        controller.Shutdown()
    if capture:
        capture.Shutdown()
    (output / "result.json").write_text(json.dumps(report, indent=2), encoding="utf-8")
os._exit(0 if report["status"] == "passed" else 1)
