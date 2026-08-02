const fs = require("fs");
const path = require("path");

const root = path.resolve(__dirname, "..");
const inputPath = process.argv[2]
  ? path.resolve(process.argv[2])
  : path.join(
      root,
      "assets_private",
      "laevat_skinned",
      "laevat_skinned_material.glb",
    );
const outputPath = process.argv[3]
  ? path.resolve(process.argv[3])
  : path.join(
      root,
      "assets_private",
      "laevat_skinned",
      "laevat_idle_material.glb",
    );

function align4(value) {
  return (value + 3) & ~3;
}

function parseGlb(data) {
  if (data.readUInt32LE(0) !== 0x46546c67 || data.readUInt32LE(4) !== 2) {
    throw new Error("Input is not a glTF 2.0 binary file");
  }
  let cursor = 12;
  let json;
  let binary;
  while (cursor < data.length) {
    const length = data.readUInt32LE(cursor);
    const type = data.readUInt32LE(cursor + 4);
    const content = data.subarray(cursor + 8, cursor + 8 + length);
    if (type === 0x4e4f534a) {
      json = JSON.parse(content.toString("utf8").trimEnd());
    } else if (type === 0x004e4942) {
      binary = content;
    }
    cursor += 8 + length;
  }
  if (!json || !binary) {
    throw new Error("Input GLB must contain JSON and BIN chunks");
  }
  return { json, binary };
}

function normalizeQuaternion(quaternion) {
  const length = Math.hypot(...quaternion);
  return quaternion.map((component) => component / length);
}

function multiplyQuaternion(left, right) {
  const [lx, ly, lz, lw] = left;
  const [rx, ry, rz, rw] = right;
  return normalizeQuaternion([
    lw * rx + lx * rw + ly * rz - lz * ry,
    lw * ry - lx * rz + ly * rw + lz * rx,
    lw * rz + lx * ry - ly * rx + lz * rw,
    lw * rw - lx * rx - ly * ry - lz * rz,
  ]);
}

function axisAngle(axis, angle) {
  const half = angle * 0.5;
  const sine = Math.sin(half);
  return [axis[0] * sine, axis[1] * sine, axis[2] * sine, Math.cos(half)];
}

function encodeFloats(values) {
  const buffer = Buffer.alloc(values.length * 4);
  values.forEach((value, index) => buffer.writeFloatLE(value, index * 4));
  return buffer;
}

function buildGlb(json, binary) {
  const jsonSource = Buffer.from(JSON.stringify(json), "utf8");
  const jsonLength = align4(jsonSource.length);
  const jsonChunk = Buffer.alloc(jsonLength, 0x20);
  jsonSource.copy(jsonChunk);
  const binaryLength = align4(binary.length);
  const binaryChunk = Buffer.alloc(binaryLength);
  binary.copy(binaryChunk);
  const output = Buffer.alloc(12 + 8 + jsonLength + 8 + binaryLength);
  output.writeUInt32LE(0x46546c67, 0);
  output.writeUInt32LE(2, 4);
  output.writeUInt32LE(output.length, 8);
  output.writeUInt32LE(jsonLength, 12);
  output.writeUInt32LE(0x4e4f534a, 16);
  jsonChunk.copy(output, 20);
  const binaryHeader = 20 + jsonLength;
  output.writeUInt32LE(binaryLength, binaryHeader);
  output.writeUInt32LE(0x004e4942, binaryHeader + 4);
  binaryChunk.copy(output, binaryHeader + 8);
  return output;
}

const source = fs.readFileSync(inputPath);
const { json, binary: sourceBinary } = parseGlb(source);
if ((json.animations ?? []).length > 0) {
  throw new Error("Input already contains animations");
}

const spineNode = json.nodes.findIndex((node) => node.name === "Bip001_Spine2");
const headNode = json.nodes.findIndex((node) => node.name === "Bip001_Head");
if (spineNode < 0 || headNode < 0) {
  throw new Error("Required idle-animation joints were not found");
}

json.bufferViews ??= [];
json.accessors ??= [];
const parts = [sourceBinary.subarray(0, json.buffers[0].byteLength)];
let binaryLength = json.buffers[0].byteLength;

function appendFloatAccessor(values, type, count, minimum, maximum) {
  const padding = align4(binaryLength) - binaryLength;
  if (padding > 0) {
    parts.push(Buffer.alloc(padding));
    binaryLength += padding;
  }
  const data = encodeFloats(values);
  const bufferView = json.bufferViews.length;
  json.bufferViews.push({
    buffer: 0,
    byteOffset: binaryLength,
    byteLength: data.length,
  });
  parts.push(data);
  binaryLength += data.length;
  const accessor = json.accessors.length;
  const definition = {
    bufferView,
    componentType: 5126,
    count,
    type,
  };
  if (minimum) definition.min = minimum;
  if (maximum) definition.max = maximum;
  json.accessors.push(definition);
  return accessor;
}

const times = Array.from({ length: 17 }, (_, index) => index * 0.25);
const timeAccessor = appendFloatAccessor(
  times,
  "SCALAR",
  times.length,
  [times[0]],
  [times.at(-1)],
);

function rotationTrack(nodeIndex, axis, amplitudeDegrees, phase = 0) {
  const base = json.nodes[nodeIndex].rotation ?? [0, 0, 0, 1];
  return times.flatMap((time) => {
    const cycle = (time / times.at(-1)) * Math.PI * 2 + phase;
    const angle = Math.sin(cycle) * amplitudeDegrees * (Math.PI / 180);
    return multiplyQuaternion(base, axisAngle(axis, angle));
  });
}

const spineAccessor = appendFloatAccessor(
  rotationTrack(spineNode, [0, 0, 1], 0.9),
  "VEC4",
  times.length,
);
const headAccessor = appendFloatAccessor(
  rotationTrack(headNode, [0, 1, 0], 0.55, Math.PI * 0.25),
  "VEC4",
  times.length,
);

json.animations = [
  {
    name: "Afterglow_ProceduralIdle",
    samplers: [
      { input: timeAccessor, output: spineAccessor, interpolation: "LINEAR" },
      { input: timeAccessor, output: headAccessor, interpolation: "LINEAR" },
    ],
    channels: [
      { sampler: 0, target: { node: spineNode, path: "rotation" } },
      { sampler: 1, target: { node: headNode, path: "rotation" } },
    ],
  },
];
json.buffers[0].byteLength = binaryLength;
const outputBinary = Buffer.concat(parts, binaryLength);
fs.mkdirSync(path.dirname(outputPath), { recursive: true });
fs.writeFileSync(outputPath, buildGlb(json, outputBinary));
console.log(
  `Wrote ${outputPath} (${fs.statSync(outputPath).size} bytes, ` +
    `${json.animations[0].name}, ${times.length} keys, 4.0 seconds)`,
);
