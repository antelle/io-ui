#!/usr/bin/env node

console.log('Building io-ui...');

process.chdir(__dirname);

var
    run = require('./tools/run-task.js'),
    fs = require('fs.extra'),
    path = require('path');

var buildDir = 'deps/node';
var patchFile = path.resolve('tools/node.patch');
var targetFile = process.platform === 'win32' ? 'build\\node.exe' : 'build/node';
var exeFile = process.platform === 'win32' ? 'deps\\node\\Release\\node.exe' : 'deps/node/out/Release/node';
var upxPath = process.platform === 'win32' ? 'tools/win32/upx/upx.exe' :
    process.platform === 'darwin' ? 'tools/darwin/upx/upx' : 'upx';

if (fs.existsSync(exeFile))
    fs.unlinkSync(exeFile);
if (fs.existsSync('build')) {
    //console.log('Cleaning...');
    //fs.rmrfSync('build');
}
if (!fs.existsSync('build'))
    fs.mkdirSync('build');

process.chdir(buildDir);

console.log('Patching...');
run('git', ['reset', '--hard'], function() {
    run('git', ['apply', '--whitespace=fix', patchFile], function() {
        console.log('Patch applied OK');
        copyFiles();
        build();
    });
});

function copyFiles() {
    fs.writeFileSync('src/res/node.ico', fs.readFileSync(path.join(__dirname, 'res/icon.ico')));
}

function build() {
    console.log('Building...');
    if (process.platform === 'win32') {
        var buildCmd = 'vcbuild.bat';
        run(buildCmd, ['noetw', 'noperfctr', 'nosign', 'release', 'x86', 'nobuild'], function() {
            var content = fs.readFileSync('node.vcxproj', 'utf8');
            content = content.replace(/<PlatformToolset>.*?<\/PlatformToolset>/g, '<PlatformToolset>v120_xp</PlatformToolset>');
            fs.writeFileSync('node.vcxproj', content, 'utf8');
            run(buildCmd, ['noetw', 'noperfctr', 'nosign', 'release', 'x86', 'noprojgen'], function() {
                compress();
            });
        });
    } else {
        var destCpu = process.platform === 'darwin' ? 'ia32' : 'x64';
        run('./configure', ['--dest-cpu=' + destCpu, '--without-dtrace', '--without-npm'], function() {
            run('make', [], function() {
                compress();
            });
        });
    }
}

function compress() {
    console.log('Compressing...');
    process.chdir(__dirname);
    if (!fs.existsSync(exeFile)) {
        console.error('File doesn\'t exist: ' + exeFile);
        process.exit(1);
    }
    if (fs.existsSync(targetFile)) {
        fs.unlinkSync(targetFile);
    }
    if (process.platform === 'win32') {
        run(upxPath, ['-9', '--lzma', '-v', '-o' + targetFile, exeFile], function() {
            complete();
        });
    } else {
        //fs.writeFileSync(targetFile, fs.readFileSync(exeFile));fs.chmodSync(targetFile, 0x777);complete();return;
        run('strip', ['-x', exeFile], function() {
            run(upxPath, ['-9', '--lzma', '-v', '-o' + targetFile, exeFile], function() {
                fs.chmodSync(targetFile, 0x777);
                complete();
            });
        });
    }
}

function complete() {
    console.log('Build complete in ' + process.uptime() + 's');
}
