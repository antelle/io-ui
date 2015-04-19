#!/usr/bin/env node

console.log('Building io-ui examples...');

var
    ps = require('child_process'),
    fs = require('fs'),
    path = require('path');

process.chdir(__dirname);

var outDir = path.resolve('build/examples');
var exDir = path.resolve('examples');

if (!fs.existsSync(outDir)) {
    fs.mkdirSync(outDir);
}

fs.readdirSync(exDir).filter(function(f) { return /^[^\.]/.test(f); }).forEach(function(example) {
    console.log('Building ' + example + '...');
    process.chdir(path.join(exDir, example));
    if (fs.existsSync('bower.json'))
        exec('bower install');
    if (fs.existsSync('package.json'))
        exec('npm install');
    process.chdir(__dirname);
    exec('node package.js ' + path.join(exDir, example) + ' ' + outDir);
});

function exec(command) {
    console.log(command);
    console.log(ps.execSync(command).toString());
}