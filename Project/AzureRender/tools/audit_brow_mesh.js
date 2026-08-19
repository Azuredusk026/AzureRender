const fs = require("fs");
const path = require("path");

const inputPath = path.resolve(
  process.argv[2] ??
    path.join(
      __dirname,
      "..",
      "assets_private",
      "laevat_skinned",
      "laevat_idle_material.glb",
    ),
);

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
    const chunk = data.subarray(cursor + 8, cursor + 8 + length);
    if (type === 0x4e4f534a) {
      json = JSON.parse(chunk.toString("utf8").trimEnd());
    } else if (type === 0x004e4942) {
      binary = chunk;
    }
    cursor += 8 + length;
  }
  if (!json || !binary) {
    throw new Error("GLB must contain JSON and BIN chunks");
  }
  return { json, binary };
}

const componentInfo = {
  5120: { bytes: 1, read: "readInt8" },
  5121: { bytes: 1, read: "readUInt8" },
  5122: { bytes: 2, read: "readInt16LE" },
  5123: { bytes: 2, read: "readUInt16LE" },
  5125: { bytes: 4, read: "readUInt32LE" },
  5126: { bytes: 4, read: "readFloatLE" },
};
const typeComponents = { SCALAR: 1, VEC2: 2, VEC3: 3, VEC4: 4 };

function normalizedValue(value, componentType) {
  if (componentType === 5120) return Math.max(value / 127, -1);
  if (componentType === 5121) return value / 255;
  if (componentType === 5122) return Math.max(value / 32767, -1);
  if (componentType === 5123) return value / 65535;
  return value;
}

function readAccessor(json, binary, accessorIndex) {
  const accessor = json.accessors[accessorIndex];
  const view = json.bufferViews[accessor.bufferView];
  const info = componentInfo[accessor.componentType];
  const components = typeComponents[accessor.type];
  if (!info || !components || accessor.sparse) {
    throw new Error(`Unsupported accessor ${accessorIndex}`);
  }
  const packedStride = info.bytes * components;
  const stride = view.byteStride ?? packedStride;
  const start = (view.byteOffset ?? 0) + (accessor.byteOffset ?? 0);
  return Array.from({ length: accessor.count }, (_, element) =>
    Array.from({ length: components }, (_, component) => {
      const offset = start + element * stride + component * info.bytes;
      const value = binary[info.read](offset);
      return accessor.normalized
        ? normalizedValue(value, accessor.componentType)
        : value;
    }),
  );
}

function connectedComponentCount(vertexCount, edges) {
  const parent = Array.from({ length: vertexCount }, (_, index) => index);
  const find = (value) => {
    while (parent[value] !== value) {
      parent[value] = parent[parent[value]];
      value = parent[value];
    }
    return value;
  };
  const unite = (left, right) => {
    left = find(left);
    right = find(right);
    if (left !== right) parent[right] = left;
  };
  for (const [left, right] of edges) unite(left, right);
  return new Set(parent.map((_, index) => find(index))).size;
}

const { json, binary } = parseGlb(fs.readFileSync(inputPath));
const findings = [];

for (const [meshIndex, mesh] of (json.meshes ?? []).entries()) {
  for (const [primitiveIndex, primitive] of mesh.primitives.entries()) {
    const material = json.materials?.[primitive.material];
    if (!material?.name?.toLowerCase().includes("brow")) continue;
    if ((primitive.mode ?? 4) !== 4) {
      throw new Error("Brow audit currently requires triangle primitives");
    }

    const positions = readAccessor(json, binary, primitive.attributes.POSITION);
    const joints = readAccessor(json, binary, primitive.attributes.JOINTS_0);
    const weights = readAccessor(json, binary, primitive.attributes.WEIGHTS_0);
    const indices = readAccessor(json, binary, primitive.indices).flat();
    const triangles = [];
    const edges = [];
    const edgeCounts = new Map();
    let degenerateTriangles = 0;
    for (let offset = 0; offset < indices.length; offset += 3) {
      const triangle = indices.slice(offset, offset + 3);
      triangles.push(triangle);
      if (new Set(triangle).size !== 3) degenerateTriangles += 1;
      for (let edge = 0; edge < 3; edge += 1) {
        const pair = [triangle[edge], triangle[(edge + 1) % 3]].sort(
          (left, right) => left - right,
        );
        edges.push(pair);
        const key = pair.join(":");
        edgeCounts.set(key, (edgeCounts.get(key) ?? 0) + 1);
      }
    }

    const skinNode = (json.nodes ?? []).find(
      (node) => node.mesh === meshIndex && node.skin !== undefined,
    );
    const skin = json.skins?.[skinNode?.skin];
    const jointUsage = new Map();
    let zeroWeightVertices = 0;
    let invalidWeightSums = 0;
    let minWeightSum = Number.POSITIVE_INFINITY;
    let maxWeightSum = Number.NEGATIVE_INFINITY;
    let multiInfluenceVertices = 0;
    for (let vertex = 0; vertex < weights.length; vertex += 1) {
      const sum = weights[vertex].reduce((total, value) => total + value, 0);
      minWeightSum = Math.min(minWeightSum, sum);
      maxWeightSum = Math.max(maxWeightSum, sum);
      if (sum < 1e-6) zeroWeightVertices += 1;
      if (Math.abs(sum - 1) > 1e-4) invalidWeightSums += 1;
      const active = weights[vertex]
        .map((weight, slot) => ({ weight, joint: joints[vertex][slot] }))
        .filter(({ weight }) => weight > 1e-5);
      if (active.length > 1) multiInfluenceVertices += 1;
      for (const { weight, joint } of active) {
        const nodeIndex = skin?.joints?.[joint];
        const nodeName = json.nodes?.[nodeIndex]?.name ?? `node_${nodeIndex}`;
        const entry = jointUsage.get(joint) ?? {
          joint,
          nodeIndex,
          nodeName,
          vertices: 0,
          totalWeight: 0,
          maxWeight: 0,
        };
        entry.vertices += 1;
        entry.totalWeight += weight;
        entry.maxWeight = Math.max(entry.maxWeight, weight);
        jointUsage.set(joint, entry);
      }
    }

    const positionGroups = new Map();
    for (const [index, position] of positions.entries()) {
      const key = position.map((value) => Math.round(value * 1e6)).join(":");
      const group = positionGroups.get(key) ?? [];
      group.push(index);
      positionGroups.set(key, group);
    }
    const duplicatePositionGroups = [...positionGroups.values()].filter(
      (group) => group.length > 1,
    );

    const result = {
      asset: inputPath,
      meshIndex,
      primitiveIndex,
      material: material.name,
      topology: {
        vertices: positions.length,
        indices: indices.length,
        triangles: triangles.length,
        indexedComponents: connectedComponentCount(positions.length, edges),
        boundaryEdges: [...edgeCounts.values()].filter((count) => count === 1)
          .length,
        nonManifoldEdges: [...edgeCounts.values()].filter((count) => count > 2)
          .length,
        degenerateTriangles,
        duplicatePositionGroups: duplicatePositionGroups.length,
        duplicatePositionVertices: duplicatePositionGroups.reduce(
          (total, group) => total + group.length,
          0,
        ),
        bounds: {
          min: [0, 1, 2].map((axis) =>
            Math.min(...positions.map((position) => position[axis])),
          ),
          max: [0, 1, 2].map((axis) =>
            Math.max(...positions.map((position) => position[axis])),
          ),
        },
      },
      skinning: {
        skin: skinNode?.skin,
        skeletonNode: skin?.skeleton,
        skeletonName: json.nodes?.[skin?.skeleton]?.name,
        minWeightSum,
        maxWeightSum,
        invalidWeightSums,
        zeroWeightVertices,
        multiInfluenceVertices,
        usedJoints: [...jointUsage.values()].sort(
          (left, right) => right.totalWeight - left.totalWeight,
        ),
      },
    };
    findings.push(result);
  }
}

if (findings.length === 0) {
  throw new Error("No brow primitive was found");
}
console.log(JSON.stringify(findings, null, 2));
