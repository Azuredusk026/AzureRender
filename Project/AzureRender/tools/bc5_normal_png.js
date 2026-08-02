const zlib = require("zlib");

const PNG_SIGNATURE = Buffer.from([
  0x89, 0x50, 0x4e, 0x47, 0x0d, 0x0a, 0x1a, 0x0a,
]);

function paeth(left, above, upperLeft) {
  const prediction = left + above - upperLeft;
  const leftDistance = Math.abs(prediction - left);
  const aboveDistance = Math.abs(prediction - above);
  const upperLeftDistance = Math.abs(prediction - upperLeft);
  if (leftDistance <= aboveDistance && leftDistance <= upperLeftDistance) {
    return left;
  }
  return aboveDistance <= upperLeftDistance ? above : upperLeft;
}

function crc32(data) {
  let crc = 0xffffffff;
  for (const value of data) {
    crc ^= value;
    for (let bit = 0; bit < 8; ++bit) {
      crc = (crc >>> 1) ^ (crc & 1 ? 0xedb88320 : 0);
    }
  }
  return (crc ^ 0xffffffff) >>> 0;
}

function makeChunk(type, data) {
  const typeData = Buffer.from(type, "ascii");
  const chunk = Buffer.alloc(12 + data.length);
  chunk.writeUInt32BE(data.length, 0);
  typeData.copy(chunk, 4);
  data.copy(chunk, 8);
  chunk.writeUInt32BE(
    crc32(Buffer.concat([typeData, data])),
    8 + data.length
  );
  return chunk;
}

function decodePng(data) {
  if (!data.subarray(0, 8).equals(PNG_SIGNATURE)) {
    throw new Error("Expected a PNG image");
  }

  let cursor = 8;
  let width;
  let height;
  let bitDepth;
  let colorType;
  let interlace;
  const idat = [];
  while (cursor < data.length) {
    const length = data.readUInt32BE(cursor);
    const type = data.subarray(cursor + 4, cursor + 8).toString("ascii");
    const chunk = data.subarray(cursor + 8, cursor + 8 + length);
    if (type === "IHDR") {
      width = chunk.readUInt32BE(0);
      height = chunk.readUInt32BE(4);
      bitDepth = chunk[8];
      colorType = chunk[9];
      interlace = chunk[12];
    } else if (type === "IDAT") {
      idat.push(chunk);
    } else if (type === "IEND") {
      break;
    }
    cursor += 12 + length;
  }

  if (
    bitDepth !== 8 ||
    (colorType !== 0 && colorType !== 2 && colorType !== 6) ||
    interlace !== 0
  ) {
    throw new Error(
      `Unsupported PNG layout: depth=${bitDepth}, color=${colorType}, ` +
        `interlace=${interlace}`
    );
  }

  const channels = colorType === 0 ? 1 : colorType === 6 ? 4 : 3;
  const stride = width * channels;
  const filtered = zlib.inflateSync(Buffer.concat(idat));
  const pixels = Buffer.alloc(width * height * channels);

  let inputOffset = 0;
  for (let row = 0; row < height; ++row) {
    const filter = filtered[inputOffset++];
    const rowOffset = row * stride;
    for (let column = 0; column < stride; ++column) {
      const raw = filtered[inputOffset++];
      const left = column >= channels ? pixels[rowOffset + column - channels] : 0;
      const above = row > 0 ? pixels[rowOffset + column - stride] : 0;
      const upperLeft =
        row > 0 && column >= channels
          ? pixels[rowOffset + column - stride - channels]
          : 0;
      let value;
      switch (filter) {
        case 0:
          value = raw;
          break;
        case 1:
          value = raw + left;
          break;
        case 2:
          value = raw + above;
          break;
        case 3:
          value = raw + Math.floor((left + above) / 2);
          break;
        case 4:
          value = raw + paeth(left, above, upperLeft);
          break;
        default:
          throw new Error(`Unsupported PNG filter ${filter}`);
      }
      pixels[rowOffset + column] = value & 0xff;
    }
  }
  return { width, height, channels, pixels };
}

function encodeRgbaPng(width, height, pixels) {
  const stride = width * 4;
  const filtered = Buffer.alloc(height * (stride + 1));
  for (let row = 0; row < height; ++row) {
    const destination = row * (stride + 1);
    filtered[destination] = 0;
    pixels.copy(
      filtered,
      destination + 1,
      row * stride,
      (row + 1) * stride
    );
  }

  const ihdr = Buffer.alloc(13);
  ihdr.writeUInt32BE(width, 0);
  ihdr.writeUInt32BE(height, 4);
  ihdr[8] = 8;
  ihdr[9] = 6;
  ihdr[10] = 0;
  ihdr[11] = 0;
  ihdr[12] = 0;

  return Buffer.concat([
    PNG_SIGNATURE,
    makeChunk("IHDR", ihdr),
    makeChunk("IDAT", zlib.deflateSync(filtered, { level: 9 })),
    makeChunk("IEND", Buffer.alloc(0)),
  ]);
}

function convertUnrealBc5NormalPng(data) {
  const decoded = decodePng(data);
  const output = Buffer.alloc(decoded.width * decoded.height * 4);
  for (
    let source = 0, destination = 0;
    source < decoded.pixels.length;
    source += decoded.channels, destination += 4
  ) {
    let x = decoded.pixels[source] / 127.5 - 1.0;
    let y = 1.0 - decoded.pixels[source + 1] / 127.5;
    const xyLengthSquared = x * x + y * y;
    if (xyLengthSquared > 1.0) {
      const inverseLength = 1.0 / Math.sqrt(xyLengthSquared);
      x *= inverseLength;
      y *= inverseLength;
    }
    const z = Math.sqrt(Math.max(0.0, 1.0 - x * x - y * y));

    output[destination] = Math.round((x * 0.5 + 0.5) * 255);
    output[destination + 1] = Math.round((y * 0.5 + 0.5) * 255);
    output[destination + 2] = Math.round((z * 0.5 + 0.5) * 255);
    output[destination + 3] = 255;
  }
  return encodeRgbaPng(decoded.width, decoded.height, output);
}

function convertUnrealMsreToGltfMrPng(
  data,
  { metallicStrength = 1.0, roughnessOffset = 0.0 } = {}
) {
  const decoded = decodePng(data);
  const output = Buffer.alloc(decoded.width * decoded.height * 4);
  const clampedMetallicStrength = Math.min(
    Math.max(metallicStrength, 0.0),
    1.0
  );
  for (
    let source = 0, destination = 0;
    source < decoded.pixels.length;
    source += decoded.channels, destination += 4
  ) {
    const sourceMetallic = decoded.pixels[source] / 255.0;
    const sourceRoughness = decoded.pixels[source + 2] / 255.0;
    // The Unreal instances expose Roughnessmap_Strengh as a signed surface
    // adjustment. The original material graph is unavailable, so bake a
    // deliberately conservative quarter-scale approximation instead of
    // treating the packed channel as the final glTF value.
    const adjustedRoughness = Math.min(
      Math.max(sourceRoughness + roughnessOffset * 0.25, 0.08),
      1.0
    );
    output[destination] = 255;
    output[destination + 1] = Math.round(adjustedRoughness * 255);
    output[destination + 2] = Math.round(
      sourceMetallic * clampedMetallicStrength * 255
    );
    output[destination + 3] = 255;
  }
  return encodeRgbaPng(decoded.width, decoded.height, output);
}

function convertUnrealSpecularEmissivePng(packedData, emissiveData) {
  const packed = decodePng(packedData);
  const emissive = emissiveData ? decodePng(emissiveData) : undefined;
  const output = Buffer.alloc(packed.width * packed.height * 4);
  for (let pixel = 0; pixel < packed.width * packed.height; ++pixel) {
    const packedSource = pixel * packed.channels;
    const destination = pixel * 4;
    const emissiveMask =
      packed.channels > 3 ? packed.pixels[packedSource + 3] / 255.0 : 0.0;
    const packedX = pixel % packed.width;
    const packedY = Math.floor(pixel / packed.width);
    const emissiveX = emissive
      ? Math.min(
          emissive.width - 1,
          Math.floor((packedX + 0.5) * emissive.width / packed.width)
        )
      : 0;
    const emissiveY = emissive
      ? Math.min(
          emissive.height - 1,
          Math.floor((packedY + 0.5) * emissive.height / packed.height)
        )
      : 0;
    const emissiveSource = emissive
      ? (emissiveY * emissive.width + emissiveX) * emissive.channels
      : 0;
    for (let channel = 0; channel < 3; ++channel) {
      const emissiveValue = emissive
        ? emissive.pixels[emissiveSource + channel]
        : 0;
      output[destination + channel] = Math.round(
        emissiveValue * emissiveMask
      );
    }
    output[destination + 3] = packed.pixels[packedSource + 1];
  }
  return encodeRgbaPng(packed.width, packed.height, output);
}

module.exports = {
  convertUnrealBc5NormalPng,
  convertUnrealMsreToGltfMrPng,
  convertUnrealSpecularEmissivePng,
  decodePng,
};
