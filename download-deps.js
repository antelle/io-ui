#!/usr/bin/env node

console.log('Downloading deps...');

var
    ps = require('child_process'),
    fs = require('fs'),
    path = require('path'),
    http = require('http');

var deps = [
    { name: 'node', repo: 'https://github.com/joyent/node.git', branch: 'v0.12.2-release' }
];

process.chdir(__dirname);
exec('npm update');

if (!fs.existsSync('deps')) {
    fs.mkdirSync('deps');
}

deps.forEach(function(dep) {
    if (dep.os && dep.os.indexOf(process.platform) < 0)
        return;
    console.log(dep.name + '/' + (dep.branch || dep.file));
    process.chdir(path.join(__dirname, 'deps'));
    if (dep.repo) {
        var justCloned = false;
        if (!fs.existsSync(dep.name) || !fs.existsSync(path.join(dep.name, '.git'))) {
            exec('git clone -b ' + dep.branch + ' ' + dep.repo);
            justCloned = true;
        }
        process.chdir(dep.name);
        if (!justCloned) {
            exec('git reset --hard');
            exec('git pull');
        }
        var branch = exec('git rev-parse --abbrev-ref HEAD').trim();
        if (branch !== dep.branch) {
            exec('git checkout ' + dep.branch);
        }
    }
});
if (process.platform === 'linux' && !existsInPath('upx')) {
    exec('sudo apt-get install upx');
}
// sudo apt-get install libwebkit2gtk-3.0-dev
// sudo apt-get install libgtk-3-dev
// sudo apt-get install upx

function exec(cmd) {
    console.log(cmd);
    return ps.execSync(cmd).toString();
}

function existsInPath(program) {
    try {
        ps.execSync(program + ' --version');
        return true;
    } catch (e) {
        return false;
    }
}

console.log('Deps downloaded OK');
