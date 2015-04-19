#!/usr/bin/env node

var fs = require('fs');
var browserify = require('browserify');
var outPath = 'build/package.js';
if (fs.existsSync(outPath))
    fs.unlinkSync(outPath);
var outFile = fs.createWriteStream(outPath);
outFile.write('#!/usr/bin/env node\n\n', 'utf8');
var b = browserify({
    entries: ['./package.js'],
    builtins: false,
    commondir: false,
    detectGlobals: false,
    insertGlobalVars: '__filename,__dirname'
});
b.bundle().pipe(outFile);
if (process.platform !== 'win32')
    fs.chmodSync(outPath, 0x777);