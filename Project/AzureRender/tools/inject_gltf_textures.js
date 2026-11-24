const fs = require("fs");
const path = require("path");
const {
  convertUnrealBc5NormalPng,
  convertUnrealMsreToGltfMrPng,
  convertUnrealSpecularEmissivePng,
} = require("./bc5_normal_png");

const root = path.resolve(__dirname, "..");
const assetRoot = path.join(root, "assets_private", "laevat_static");
const inputPath = process.argv[2]
  ? path.resolve(process.argv[2])
  : path.join(assetRoot, "laevat_static.glb");
const outputPath = process.argv[3]
  ? path.resolve(process.argv[3])
  : path.join(assetRoot, "laevat_static_material.glb");
const manifestPath = path.join(assetRoot, "unreal_material_textures.json");
const textureRoot = path.join(assetRoot, "textures");
const faceSdfPath = path.join(root, "assets_public", "face_sdf_v1.png");
const faceSdfHeadNode = "Bip001_Head";

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

function getTextureFilename(assetPath) {
  if (!assetPath) {
    return undefined;
  }
  if (assetPath.startsWith("/Game/Matcap/")) {
    return `${assetPath.split(".").at(-1)}.png`;
  }
  if (!assetPath.startsWith("/Game/ZMD/莱万汀/Tex/")) {
    return undefined;
  }
  const objectName = assetPath.split(".").at(-1);
  return `${objectName}.png`;
}

const input = fs.readFileSync(inputPath);
const manifest = JSON.parse(fs.readFileSync(manifestPath, "utf8"));
const { json, binary: sourceBinary } = parseGlb(input);

json.bufferViews ??= [];
json.images ??= [];
json.textures ??= [];

const sourceLength = json.buffers[0].byteLength;
const binaryParts = [sourceBinary.subarray(0, sourceLength)];
let binaryLength = sourceLength;
const textureIndices = new Map();
const convertedNormals = new Map();
const convertedMetallicRoughness = new Map();
const convertedSpecularEmissive = new Map();

function appendPadding() {
  const padding = align4(binaryLength) - binaryLength;
  if (padding > 0) {
    binaryParts.push(Buffer.alloc(padding));
    binaryLength += padding;
  }
}

function addTexture(filename, imageData = undefined, cacheName = filename) {
  if (textureIndices.has(cacheName)) {
    return textureIndices.get(cacheName);
  }

  imageData ??= fs.readFileSync(path.join(textureRoot, filename));
  appendPadding();
  const bufferViewIndex = json.bufferViews.length;
  json.bufferViews.push({
    buffer: 0,
    byteOffset: binaryLength,
    byteLength: imageData.length,
  });
  binaryParts.push(imageData);
  binaryLength += imageData.length;

  const imageIndex = json.images.length;
  json.images.push({
    name: path.basename(filename, ".png"),
    mimeType: "image/png",
    bufferView: bufferViewIndex,
  });
  const textureIndex = json.textures.length;
  json.textures.push({ source: imageIndex });
  textureIndices.set(cacheName, textureIndex);
  return textureIndex;
}

const baseColorKeys = ["BaseTex", "T_BaseColor", "BaseColor"];
const normalKeys = ["NormalTex", "Normal"];
// Cloth instances expose the packed material texture as T_RGBA_P, while the
// hair master material uses _P for the same authored data role.
const packedMaterialKeys = ["T_RGBA_P", "_P"];
let boundMaterials = 0;
let boundNormals = 0;
let boundMetallicRoughness = 0;
let boundSpecularEmissive = 0;
let boundEmissive = 0;
let boundStyleMasks = 0;
let boundAoColors = 0;
let boundLamShadowColors = 0;
let boundMatcaps = 0;
let boundHairData = 0;
let boundMaterialProfiles = 0;
let boundFaceSdf = 0;

const faceSdfHeadNodes = (json.nodes ?? []).filter(
  (node) => node.name === faceSdfHeadNode
);
const requiresFaceSdf = (json.skins ?? []).length > 0;
if (requiresFaceSdf && faceSdfHeadNodes.length !== 1) {
  throw new Error(
    `Skinned Face SDF requires exactly one ${faceSdfHeadNode} node; found ` +
      faceSdfHeadNodes.length
  );
}
if (!fs.existsSync(faceSdfPath)) {
  throw new Error(`Face SDF texture was not found: ${faceSdfPath}`);
}

function buildMaterialProfile(materialName, parameters = {}) {
  const name = materialName.toLowerCase();
  let materialClass = "generic";
  let features = ["neutral-fallback"];
  const isBrow = name.includes("brow");
  if (name.includes("hairshadow") || name.includes("eyeshadow") || isBrow) {
    materialClass = "overlay";
    features = isBrow ? ["overlay", "brow-overlay"] : ["overlay"];
  } else if (name.includes("face")) {
    materialClass = "face";
    features = ["stylized-shadow", "face-sdf-eligible"];
  } else if (name.includes("iris") || name.includes("eye")) {
    materialClass = "eye";
    features = ["stylized-shadow"];
  } else if (name.includes("hair")) {
    materialClass = "hair";
    features = ["stylized-shadow", "hair-anisotropy"];
  } else if (name.includes("body") || name.includes("skin")) {
    materialClass = "skin";
    features = ["stylized-shadow"];
  } else if (name.includes("cloth") || name.includes("fabric")) {
    materialClass = "fabric";
    features = ["stylized-shadow"];
  } else if (name.includes("metal")) {
    materialClass = "metal";
    features = ["stylized-shadow"];
  }
  if (parameters._E && !features.includes("emissive-mask")) {
    features.push("emissive-mask");
  }
  // Brow cards use brow-joint-only expansion. The source primitive also
  // contains separate eyelash islands, which remain at authored size.
  const styleParameters = isBrow ? [0.0012, 0.0, 0.0, 0.0] : ({
    generic: [1.0, 1.0, 1.0, 1.0],
    skin: [0.9, 0.8, 0.35, 0.35],
    face: [0.85, 0.75, 0.15, 0.25],
    hair: [1.0, 1.0, 0.4, 0.65],
    fabric: [1.0, 1.0, 0.6, 0.5],
    metal: [0.85, 0.75, 1.35, 0.75],
    eye: [0.65, 0.5, 0.65, 0.25],
    overlay: [0.0, 0.0, 0.0, 0.0],
    emissive: [0.5, 0.0, 0.0, 0.0],
    showcase: [1.0, 1.0, 1.0, 1.0],
  }[materialClass]);
  // Unreal material distances are centimetres; glTF geometry is metres.
  const featureParameters = isBrow ? [0.02, 0.95, 0.02, 1.0] : ({
    generic: [1.0, 0.0, 1.0, 0.0],
    skin: [0.75, 0.0, 1.0, 0.0],
    face: [0.65, 0.0, 1.0, 1.0],
    hair: [0.85, 1.0, 1.0, 0.0],
    fabric: [1.0, 0.0, 1.0, 0.0],
    metal: [1.0, 0.0, 1.0, 0.0],
    eye: [0.5, 0.0, 1.0, 0.0],
    overlay: [0.0, 0.0, 0.0, 0.0],
    emissive: [0.5, 0.0, 1.5, 0.0],
    showcase: [1.0, 0.0, 1.0, 0.0],
  }[materialClass]);
  return {
    schemaVersion: 2,
    class: materialClass,
    features,
    styleParameters,
    featureParameters,
  };
}

for (const material of json.materials ?? []) {
  const parameters = manifest.material_textures[material.name];
  material.extras ??= {};
  material.extras.azureRenderMaterial = buildMaterialProfile(
    material.name,
    parameters
  );
  if (
    material.extras.azureRenderMaterial.class === "face" &&
    faceSdfHeadNodes.length === 1
  ) {
    material.extras.azureRenderMaterial.faceSdf = {
      schemaVersion: 1,
      texture: addTexture(
        path.basename(faceSdfPath),
        fs.readFileSync(faceSdfPath),
        "azure-face-sdf-v1"
      ),
      texCoord: 0,
      channel: "r",
      maskChannel: "a",
      shadowOnLowValues: true,
      horizontalAxis: "left-to-right",
      headNode: faceSdfHeadNode,
    };
    boundFaceSdf += 1;
  }
  if (material.name.toLowerCase().includes("hairshadow")) {
    const detail=manifest.material_details?.[material.name] ?? {};
    const color=detail.vector_parameters?.Color_RGB ?? {r:1,g:1,b:1};
    const hsv=detail.vector_parameters?.BackHSV ?? {r:1,g:1,b:1};
    material.extras.azureRenderMaterial.features.push("scene-tint");
    material.extras.azureRenderMaterial.styleParameters=[color.r,color.g,color.b,hsv.g];
    material.extras.azureRenderMaterial.featureParameters=[hsv.r,detail.scalar_parameters?.Opacity ?? 1,(detail.scalar_parameters?.Depth ?? 1000)*.01,hsv.b];
    material.alphaMode="BLEND";material.doubleSided=true;
  }
  if (material.name.toLowerCase().includes("eyeshadow")) {
    material.alphaMode="BLEND";
    material.pbrMetallicRoughness ??= {};
    material.pbrMetallicRoughness.baseColorFactor=[0,0,0,1];
  }
  boundMaterialProfiles += 1;
  if (!parameters) {
    continue;
  }

  // Unreal's skeletal-mesh export contains mixed triangle winding in several
  // Laevat surface sections (most notably the hair, plus a few face/cloth
  // triangles). Back-face culling those sections creates holes that expose
  // the mouth cavity and body mesh underneath clothing. Keep translucent
  // overlay cards on their authored path, but make solid character surfaces
  // two-sided so the depth-writing nearest surface remains the occluder.
  if ((material.alphaMode ?? "OPAQUE") !== "BLEND") {
    material.doubleSided = true;
  }

  const vectorParameters =
    manifest.material_details?.[material.name]?.vector_parameters ?? {};
  const scalarParameters =
    manifest.material_details?.[material.name]?.scalar_parameters ?? {};
  const toValidShadowColor = (value) => {
    if (!value) {
      return undefined;
    }
    const color = [value.r, value.g, value.b];
    return color.every(Number.isFinite) && Math.max(...color) > 0.02
      ? color.map((channel) => Math.min(Math.max(channel, 0.0), 1.0))
      : undefined;
  };
  const aoColor = toValidShadowColor(vectorParameters.AO_Color);
  const lamShadowColor = toValidShadowColor(
    vectorParameters.Lam_Shadow_Color ?? vectorParameters.Lam_ShadowColor
  );
  if (aoColor) {
    material.extras.afterglowAoColor = aoColor;
    boundAoColors += 1;
  }
  if (lamShadowColor) {
    material.extras.afterglowLamShadowColor = lamShadowColor;
    boundLamShadowColors += 1;
  }
  const matcapFilename = getTextureFilename(parameters.Matcap_Color);
  const matcapColor = toValidShadowColor(vectorParameters.Matcap_Color);
  if (
    matcapFilename &&
    matcapColor &&
    fs.existsSync(path.join(textureRoot, matcapFilename))
  ) {
    material.extras.afterglowMatcapTexture = addTexture(matcapFilename);
    material.extras.afterglowMatcapColor = matcapColor;
    boundMatcaps += 1;
  }
  const hairDataFilename = getTextureFilename(parameters._HN);
  if (
    hairDataFilename &&
    fs.existsSync(path.join(textureRoot, hairDataFilename))
  ) {
    material.extras.afterglowHairDataTexture = addTexture(hairDataFilename);
    material.extras.afterglowHairParameters = [
      scalarParameters.KK_Power ?? 64.0,
      scalarParameters.KK_Ramp_Strengh ?? 0.15,
      scalarParameters.KK_Ramp ?? 4.0,
      1.0,
    ];
    const cameraOffset=vectorParameters.KK_Spe_Camera_Offset ?? {r:0,g:0,b:0};
    material.extras.azureRenderMaterial.hair={
      power:scalarParameters.KK_Power ?? 64,strength:scalarParameters.KK_Ramp_Strengh ?? .15,
      rampRow:scalarParameters.KK_Ramp ?? 3,strandShift:.1,
      cameraOffset:[cameraOffset.r,cameraOffset.g,cameraOffset.b],upperLimit:scalarParameters.KK_Spe_Upper_Limit ?? 1
    };
    const rampFilename="azure_lwt_kk_ramp.png";
    if (fs.existsSync(path.join(textureRoot,rampFilename))) {
      material.extras.azureRenderMaterial.features.push("hair-ramp");
      material.extras.azureRenderMaterial.hair.rampTexture=addTexture(rampFilename);
    }
    boundHairData += 1;
  }

  const assetPath = baseColorKeys
    .map((key) => parameters[key])
    .find((value) => value !== undefined);
  const filename = getTextureFilename(assetPath);
  if (!filename || !fs.existsSync(path.join(textureRoot, filename))) {
    continue;
  }

  material.pbrMetallicRoughness ??= {};
  material.pbrMetallicRoughness.baseColorTexture = {
    index: addTexture(filename),
    texCoord: 0,
  };
  material.pbrMetallicRoughness.metallicFactor = 0.0;
  material.pbrMetallicRoughness.roughnessFactor = 0.75;
  boundMaterials += 1;

  const styleMaskFilename = getTextureFilename(parameters._M);
  if (
    styleMaskFilename &&
    fs.existsSync(path.join(textureRoot, styleMaskFilename))
  ) {
    // The renderer currently uses glTF's occlusion slot as its internal
    // carrier for the Unreal `_M` style mask. It is sampled separately from
    // physical AO and defaults to black for materials without `_M`.
    material.occlusionTexture = {
      index: addTexture(styleMaskFilename),
      texCoord: 0,
      strength: 1.0,
    };
    boundStyleMasks += 1;
  }

  const normalAssetPath = normalKeys
    .map((key) => parameters[key])
    .find((value) => value !== undefined);
  const normalFilename = getTextureFilename(normalAssetPath);
  if (
    normalFilename &&
    fs.existsSync(path.join(textureRoot, normalFilename))
  ) {
    if (!convertedNormals.has(normalFilename)) {
      const sourceNormal = fs.readFileSync(
        path.join(textureRoot, normalFilename)
      );
      convertedNormals.set(
        normalFilename,
        convertUnrealBc5NormalPng(sourceNormal)
      );
    }
    const convertedNormal = convertedNormals.get(normalFilename);
    material.normalTexture = {
      index: addTexture(
        normalFilename.replace(/\.png$/i, "_gltf.png"),
        convertedNormal,
        `${normalFilename}:gltf-normal`
      ),
      texCoord: 0,
      scale: 1.0,
    };
    boundNormals += 1;
  }

  const packedAssetPath = packedMaterialKeys
    .map((key) => parameters[key])
    .find((value) => value !== undefined);
  const packedFilename = getTextureFilename(packedAssetPath);
  if (
    packedFilename &&
    fs.existsSync(path.join(textureRoot, packedFilename))
  ) {
    const metallicStrength =
      scalarParameters.GGX_Metallic_Strengh ?? (material.extras.azureRenderMaterial.class==="hair"?0.0:1.0);
    const roughnessBase = vectorParameters.MSRE?.b ?? 0.0;
    const roughnessMapped = scalarParameters.Roughnessmap_Strengh ?? 1.0;
    const glossScale = scalarParameters.Glossiness_Mask_Smooth ?? 1.0;
    const glossOffset = scalarParameters.Glossiness_Mask_Offset ?? 0.0;
    const metallicRoughnessKey = [
      packedFilename,
      metallicStrength.toFixed(4),
      roughnessBase, roughnessMapped, glossScale, glossOffset, "surface-v2",
    ].join("|");
    if (!convertedMetallicRoughness.has(metallicRoughnessKey)) {
      convertedMetallicRoughness.set(
        metallicRoughnessKey,
        convertUnrealMsreToGltfMrPng(
          fs.readFileSync(path.join(textureRoot, packedFilename)),
          { metallicStrength, roughnessBase, roughnessMapped, glossScale, glossOffset }
        )
      );
    }
    material.pbrMetallicRoughness.metallicRoughnessTexture = {
      index: addTexture(
        packedFilename.replace(/\.png$/i, "_gltf_mr.png"),
        convertedMetallicRoughness.get(metallicRoughnessKey),
        `${metallicRoughnessKey}:gltf-metallic-roughness`
      ),
      texCoord: 0,
    };
    material.pbrMetallicRoughness.metallicFactor = 1.0;
    material.pbrMetallicRoughness.roughnessFactor = 1.0;
    material.extras.azureRenderMaterial.features.push("surface-ao");
    boundMetallicRoughness += 1;

    const emissiveAssetPath = parameters._E;
    const emissiveFilename = getTextureFilename(emissiveAssetPath);
    const hasEmissive =
      emissiveFilename &&
      fs.existsSync(path.join(textureRoot, emissiveFilename));
    const specularEmissiveKey =
      `${packedFilename}|${hasEmissive ? emissiveFilename : "none"}`;
    if (!convertedSpecularEmissive.has(specularEmissiveKey)) {
      convertedSpecularEmissive.set(
        specularEmissiveKey,
        convertUnrealSpecularEmissivePng(
          fs.readFileSync(path.join(textureRoot, packedFilename)),
          hasEmissive
            ? fs.readFileSync(path.join(textureRoot, emissiveFilename))
            : undefined
        )
      );
    }
    material.emissiveTexture = {
      index: addTexture(
        packedFilename.replace(/\.png$/i, "_specular_emissive.png"),
        convertedSpecularEmissive.get(specularEmissiveKey),
        `${specularEmissiveKey}:specular-emissive`
      ),
      texCoord: 0,
    };
    const unrealStrength =
      manifest.material_details?.[material.name]?.scalar_parameters
        ?._E_Strengh ?? 0.0;
    const normalizedStrength = Math.min(unrealStrength / 50.0, 1.0);
    material.emissiveFactor = [
      normalizedStrength,
      normalizedStrength,
      normalizedStrength,
    ];
    boundSpecularEmissive += 1;
    if (hasEmissive) {
      boundEmissive += 1;
    }
  }
}

// Keep only textures reachable from the authored material contract.
const usedTextures=new Set();const textureRefs=[];
const remember=ref=>{if(ref && Number.isInteger(ref.index)){usedTextures.add(ref.index);textureRefs.push(ref);}};
for(const m of json.materials ?? []){
  remember(m.pbrMetallicRoughness?.baseColorTexture);remember(m.pbrMetallicRoughness?.metallicRoughnessTexture);
  remember(m.normalTexture);remember(m.occlusionTexture);remember(m.emissiveTexture);
  for(const field of ["afterglowMatcapTexture","afterglowHairDataTexture"]){if(Number.isInteger(m.extras?.[field]))usedTextures.add(m.extras[field]);}
  if(Number.isInteger(m.extras?.azureRenderMaterial?.faceSdf?.texture))usedTextures.add(m.extras.azureRenderMaterial.faceSdf.texture);
  if(Number.isInteger(m.extras?.azureRenderMaterial?.hair?.rampTexture))usedTextures.add(m.extras.azureRenderMaterial.hair.rampTexture);
}
const textureMap=new Map([...usedTextures].sort((a,b)=>a-b).map((old,i)=>[old,i]));
const keptTextures=[...textureMap.keys()].map(old=>json.textures[old]);
for(const ref of textureRefs)ref.index=textureMap.get(ref.index);
for(const m of json.materials ?? []){
  for(const field of ["afterglowMatcapTexture","afterglowHairDataTexture"]){if(Number.isInteger(m.extras?.[field]))m.extras[field]=textureMap.get(m.extras[field]);}
  const face=m.extras?.azureRenderMaterial?.faceSdf;if(face)face.texture=textureMap.get(face.texture);
  const hair=m.extras?.azureRenderMaterial?.hair;if(Number.isInteger(hair?.rampTexture))hair.rampTexture=textureMap.get(hair.rampTexture);
}
const imageMap=new Map([...new Set(keptTextures.map(t=>t.source))].sort((a,b)=>a-b).map((old,i)=>[old,i]));
json.images=[...imageMap.keys()].map(old=>json.images[old]);json.textures=keptTextures;
for(const t of json.textures)t.source=imageMap.get(t.source);
const usedViews=new Set(json.images.map(im=>im.bufferView));
for(const accessor of json.accessors ?? []){
  if(Number.isInteger(accessor.bufferView))usedViews.add(accessor.bufferView);
  if(accessor.sparse){usedViews.add(accessor.sparse.indices.bufferView);usedViews.add(accessor.sparse.values.bufferView);}
}
const viewMap=new Map([...usedViews].sort((a,b)=>a-b).map((old,i)=>[old,i]));
const originalBinary=Buffer.concat(binaryParts,binaryLength);binaryParts.length=0;binaryLength=0;
json.bufferViews=[...viewMap.keys()].map(old=>{
  const view={...json.bufferViews[old]};appendPadding();
  binaryParts.push(originalBinary.subarray(view.byteOffset??0,(view.byteOffset??0)+view.byteLength));
  view.byteOffset=binaryLength;binaryLength+=view.byteLength;return view;
});
for(const im of json.images)im.bufferView=viewMap.get(im.bufferView);
for(const accessor of json.accessors ?? []){
  if(Number.isInteger(accessor.bufferView))accessor.bufferView=viewMap.get(accessor.bufferView);
  if(accessor.sparse){accessor.sparse.indices.bufferView=viewMap.get(accessor.sparse.indices.bufferView);accessor.sparse.values.bufferView=viewMap.get(accessor.sparse.values.bufferView);}
}
appendPadding();
const outputBinary = Buffer.concat(binaryParts, binaryLength);
json.buffers[0].byteLength = outputBinary.length;

let jsonData = Buffer.from(JSON.stringify(json), "utf8");
const jsonPadding = align4(jsonData.length) - jsonData.length;
if (jsonPadding > 0) {
  jsonData = Buffer.concat([jsonData, Buffer.alloc(jsonPadding, 0x20)]);
}

const totalLength = 12 + 8 + jsonData.length + 8 + outputBinary.length;
const header = Buffer.alloc(12);
header.writeUInt32LE(0x46546c67, 0);
header.writeUInt32LE(2, 4);
header.writeUInt32LE(totalLength, 8);
const jsonHeader = Buffer.alloc(8);
jsonHeader.writeUInt32LE(jsonData.length, 0);
jsonHeader.writeUInt32LE(0x4e4f534a, 4);
const binaryHeader = Buffer.alloc(8);
binaryHeader.writeUInt32LE(outputBinary.length, 0);
binaryHeader.writeUInt32LE(0x004e4942, 4);

fs.writeFileSync(
  outputPath,
  Buffer.concat([header, jsonHeader, jsonData, binaryHeader, outputBinary])
);
console.log(
  `Wrote ${outputPath} (${fs.statSync(outputPath).size} bytes, ` +
    `${boundMaterials} base-color materials, ${boundNormals} normal materials, ` +
    `${boundMetallicRoughness} metallic-roughness materials, ` +
    `${boundSpecularEmissive} specular materials, ` +
    `${boundEmissive} emissive materials, ` +
    `${boundStyleMasks} style-mask materials, ` +
    `${boundAoColors} AO colors, ${boundLamShadowColors} Lam shadow colors, ` +
    `${boundMatcaps} matcaps, ${boundHairData} hair-data materials, ` +
    `${boundFaceSdf} face-SDF materials, ` +
    `${boundMaterialProfiles} material profiles, ` +
    `${textureIndices.size} unique embedded textures)`
);

const crypto=require("crypto");
const hash=data=>crypto.createHash("sha256").update(data).digest("hex");
const dependencies={materialManifest:{path:manifestPath,sha256:hash(fs.readFileSync(manifestPath))},
  codec:{path:require.resolve("./bc5_normal_png"),sha256:hash(fs.readFileSync(require.resolve("./bc5_normal_png")))},
  faceSdf:{path:faceSdfPath,sha256:hash(fs.readFileSync(faceSdfPath))},
  textureSources:fs.readdirSync(textureRoot).filter(name=>name.endsWith(".source.json")).map(name=>({name,...JSON.parse(fs.readFileSync(path.join(textureRoot,name),"utf8"))})),
  textures:fs.readdirSync(textureRoot).filter(name=>name.endsWith(".png")).map(name=>({name,sha256:hash(fs.readFileSync(path.join(textureRoot,name)))}))};
fs.writeFileSync(outputPath+".provenance.json",JSON.stringify({schemaVersion:2,source:{path:inputPath,sha256:hash(input)},
  generator:{path:__filename,sha256:hash(fs.readFileSync(__filename)),nodeVersion:process.version},dependencies,
  output:{path:outputPath,sha256:hash(fs.readFileSync(outputPath))}},null,2)+"\n");
