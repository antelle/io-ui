#!/usr/bin/env node

process.chdir(__dirname);

var
    fs = require('fs'),
    path = require('path'),
    ps = require('child_process');

var buildDir = path.resolve('../deps/node');
var fileName = path.resolve('node.patch');
var fStm = fs.createWriteStream(fileName);

process.chdir(buildDir);
var proc = ps.spawn('git', ['diff', '--patch', '--minimal']);
proc.on('close', function (code) {
    fStm.close();
    if (code) {
        fs.unlinkSync(fileName);
        console.error('Failed to create patch');
    } else {
        var content = fs.readFileSync(fileName, 'utf8');
        content = content.replace(/diff.*\nindex.*\nBinary.*differ\n/g, '');
        fs.writeFileSync(fileName, content, 'utf8');
        console.log('Patch created OK');
    }
});
proc.stdout.on('data', function(data) {
    fStm.write(data);
});
proc.stdout.on('end', function(data) {
    fStm.end();
});
proc.stderr.on('data', function(data) {
    console.error(data.toString());
});