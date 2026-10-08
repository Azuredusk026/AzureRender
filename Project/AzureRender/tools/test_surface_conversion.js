const assert = require('assert');
const fs = require('fs');
const vm = require('vm');
const sandbox = {require, Buffer, module:{exports:{}}};
vm.runInNewContext(fs.readFileSync(require.resolve('./bc5_normal_png'),'utf8')
  + '\nmodule.exports.encodeRgbaPng=encodeRgbaPng;module.exports.decodePng=decodePng;',sandbox);
const { encodeRgbaPng, decodePng, convertUnrealMsreToGltfMrPng } = sandbox.module.exports;

function convert(rgba, options = {}) {
  return decodePng(convertUnrealMsreToGltfMrPng(
    encodeRgbaPng(1, 1, Buffer.from(rgba)), options)).pixels;
}
// Changing AO must not change gloss-derived roughness or metallic.
const a = convert([204, 128, 26, 191]);
const b = convert([204, 128, 230, 191]);
assert.equal(a[1], b[1], 'AO must not drive roughness');
assert.equal(a[2], 204, 'metallic channel must retain its authored value');
assert.equal(a[0], 26, 'physical AO must retain its own channel');
assert(Math.abs(a[1] - 64) <= 1, 'default roughness is inverse gloss');
const matte = convert([204, 128, 26, 0]);
assert.equal(matte[1], 255, 'zero gloss produces rough surface');
const sourceMix = convert([204, 128, 26, 191], {roughnessBase:1,roughnessMapped:.5});
assert(Math.abs(sourceMix[1] - 223) <= 1, 'source roughness uses gloss as interpolation weight');
assert.throws(() => convert([1,2,3,4], {roughnessBase:NaN}), /finite/);
console.log('Surface channel separation and source interpolation passed');

const {convertUnrealSpecularEmissivePng}=sandbox.module.exports;
const e=encodeRgbaPng(1,1,Buffer.from([200,100,50,255]));
const low=decodePng(convertUnrealSpecularEmissivePng(encodeRgbaPng(1,1,Buffer.from([0,128,255,0])),e)).pixels;
const high=decodePng(convertUnrealSpecularEmissivePng(encodeRgbaPng(1,1,Buffer.from([0,128,255,255])),e)).pixels;
assert.equal(low[0],200,'gloss cannot mask emissive');
assert.equal(low[0],high[0]);
