'use strict';
const fs = require('node:fs');
const validator = require('gltf-validator');
const path = process.argv[2];
if (!path) throw new Error('Usage: node tools/validate_glb.cjs scene.glb');
validator.validateBytes(new Uint8Array(fs.readFileSync(path)), {
  uri: path,
  maxIssues: 1000,
}).then(report => {
  fs.writeFileSync(`${path}.validation.json`, JSON.stringify(report, null, 2));
  console.log(`glTF: ${report.issues.numErrors} errors, ${report.issues.numWarnings} warnings`);
  if (report.issues.numErrors) {
    console.error(JSON.stringify(report.issues.messages, null, 2));
    process.exitCode = 1;
  }
}).catch(error => { console.error(error); process.exitCode = 1; });
