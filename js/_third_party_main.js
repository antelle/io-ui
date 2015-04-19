(function() {
    'use strict';

    var
        fs = require('fs'),
        stream = require('stream'),
        path = require('path'),
        module = require('module');

    var mainSource;

    (function main() {
        fixStdio();
        if (process.perfTraceReg) process.perfTraceReg(5);
        detectEntryPoint();
        var isDir = fs.statSync(mainSource).isDirectory();
        if (process.perfTraceReg) process.perfTraceReg(6);
        if (isDir) {
            process.chdir(mainSource);
            run();
        } else if (path.extname(mainSource).toLowerCase() === '.js') {
            run();
        } else {
            var cwd = process.cwd();
            var vfs = require('vfs');
            vfs.init({
                contentFile: mainSource,
                basePath: cwd,
                success: vfsReady,
                error: errorExit,
                initialized: vfsInitialized
            });
        }
    })();

    function fixStdio() {
        var
            tty_wrap = process.binding('tty_wrap'),
            knownHandleTypes = ['TTY', 'FILE', 'PIPE', 'TCP'];
        ['stdin', 'stdout', 'stderr'].forEach(function(name, fd) {
            var handleType = tty_wrap.guessHandleType(fd);
            if (knownHandleTypes.indexOf(handleType) < 0) {
                delete process[name];
                process[name] = new stream.PassThrough();
            }
        });
    }

    function detectEntryPoint() {
        mainSource = process.argv[1];
        if (mainSource) {
            if (!fs.existsSync(mainSource)) {
                errorExit('File not found: ' + mainSource);
            }
        } else {
            mainSource = 'main.zip';
            if (!fs.existsSync(mainSource)) {
                mainSource = 'main.js';
                if (!fs.existsSync(mainSource)) {
                    mainSource = process.execPath;
                }
            }
        }
        mainSource = path.resolve(mainSource);
    }

    function vfsInitialized() {
        if (process.perfTraceReg) process.perfTraceReg(7);
    }

    function vfsReady() {
        if (process.perfTraceReg) process.perfTraceReg(8);
        run();
    }

    function run() {
        if (path.extname(mainSource).toLowerCase() !== '.js') {
            mainSource = './main.js';
        }
        process.mainModule = new module(mainSource, null);
        process.mainModule.id = '.';
        module._cache[mainSource] = process.mainModule;
        process.mainModule._compile = function() {
            if (process.perfTraceReg) process.perfTraceReg(9);
            module.prototype._compile.apply(process.mainModule, arguments);
        };
        process.mainModule.load(mainSource);
        //module._load(mainSource, null, true);
    }

    function errorExit(err) {
        console.error('Main file not found: ' + (err || 'unexpected error'));
        process.exit(1);
    }
})();