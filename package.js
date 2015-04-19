#!/usr/bin/env node

var
    fs = require('fs'),
    ps = require('child_process'),
    path = require('path'),
    os = require('os'),
    JSZip = require('jszip'),
    StreamZip = require('node-stream-zip');

var basePath = process.argv[2];
var appPath = process.argv[3];
if (!basePath) {
    console.error('Usage: package.js <path_to_app> [<path_to_output>]');
    process.exit(1);
}
basePath = path.resolve(basePath);
if (!fs.existsSync(basePath)) {
    console.error('App folder doesn\'t exist: ' + basePath);
    process.exit(1);
}
if (!fs.existsSync(path.join(basePath, 'main.js'))) {
    console.error('App folder doesn\'t contain main.js file: ' + path.join(basePath, 'main.js'));
    process.exit(1);
}
var tmpPath = path.join(os.tmpdir(), 'io-ui.pack.' + Math.round(Math.random() * 1000000) + '.tmp.zip');

console.log('Creating package: ' + basePath);
preparePackage();
createOsPackages();

function preparePackage() {
    var zip = new JSZip();
    var filesCount = addFolder(zip, basePath);
    console.log('Processed ' + filesCount + ' files');
    var content = zip.generate({ type: 'nodebuffer', compression: 'DEFLATE' });
    fs.writeFileSync(tmpPath, content);
}

function addFolder(zip, folder) {
    var zipFolder = folder.substr(basePath.length + 1).replace(/\\/g, '/');
    if (zipFolder)
        zip.folder(zipFolder);
    var addedCount = 0;
    fs.readdirSync(folder).forEach(function(file) {
        if (file[0] === '.')
            return;
        var fileName = path.join(folder, file);
        if (fs.statSync(fileName).isDirectory()) {
            addedCount += addFolder(zip, fileName);
        } else {
            zip.file((zipFolder ? zipFolder + '/' : '') + file, fs.readFileSync(fileName));
            addedCount++;
        }
    });
    return addedCount;
}

function createOsPackages() {
    var zip = new StreamZip({file: tmpPath});
    zip.on('error', function (err) {
        console.error('Error creating archive: ' + err);
        process.exit(1);
    });
    zip.on('ready', function () {
        var entries = zip.entries();
        zip.close();
        var exitCode = 0;
        try {
            if (fs.existsSync('bin')) {
                var platforms = ['win32', 'darwin', 'linux'];
                var success = 0;
                platforms.forEach(function (platform) {
                    if (fs.existsSync('bin/' + platform)) {
                        createOsPackage(platform, 'bin/' + platform, entries, zip);
                        success++;
                    }
                });
                if (!success) {
                    console.error('Please ensure that folder bin/%platform% contains binary files for at least one platform ('
                        + platforms.join(', ') + ')');
                    exitCode = 1;
                }
            } else if (fs.existsSync('build')) {
                createOsPackage(process.platform, 'build', entries, zip);
            } else {
                console.error('Please run this script from directory containing binary files in folders bin/ or build/');
                exitCode = 1;
            }
            console.log('Done in ' + process.uptime() + 's');
        } finally {
            fs.unlinkSync(tmpPath);
        }
        process.exit(exitCode);
    });
}

function createOsPackage(platform, binFolder, entries, zip) {
    console.log('Packaging ' + platform + ' executable...');
    var ext = platform === 'win32' ? '.exe' : '';
    var buildExecutable = binFolder + '/node' + ext;
    var targetPath = appPath;
    if (!targetPath) {
        targetPath = 'build/' + path.basename(basePath) + ext;
        if (!fs.existsSync('build'))
            fs.mkdirSync('build');
    } else {
        if (fs.existsSync(targetPath)) {
            if (fs.statSync(targetPath).isDirectory())
                targetPath = path.resolve(targetPath, path.basename(basePath) + ext);
        } else if (platform === 'win32' && path.extname(targetPath).toLowerCase() !== ext) {
            targetPath += ext;
        }
    }
    targetPath = path.resolve(targetPath);
    if (platform === 'darwin') {
        if (targetPath.indexOf('.app') < 0) {
            var targetAppName = path.basename(targetPath);
            targetPath += '.app';
            if (fs.existsSync(targetPath))
                ps.execSync('rm -rf ' + targetPath);
            fs.mkdirSync(targetPath);
            fs.mkdirSync(targetPath + '/Contents');
            fs.mkdirSync(targetPath + '/Contents/Resources');
            fs.mkdirSync(targetPath + '/Contents/MacOS');
            fs.writeFileSync(targetPath + '/Contents/Info.plist', createPlist(targetAppName), 'utf8');
            fs.writeFileSync(targetPath + '/Contents/Resources/icon.icns', fs.readFileSync('res/icon.icns'));
            targetPath += '/Contents/MacOS/' + targetAppName;
        }
    }
    if (fs.existsSync(targetPath))
        fs.unlinkSync(targetPath);

    var zipBuffer = fs.readFileSync(tmpPath);
    var execBuffer = fs.readFileSync(buildExecutable);
    Object.keys(entries).forEach(function (entry) {
        entry = entries[entry];
        var offset = entry.offset + execBuffer.length;
        zipBuffer.writeUInt32LE(offset, entry.headerOffset + 42);
    });
    zipBuffer.writeUInt32LE(zip.centralDirectory.offset + execBuffer.length, zip.centralDirectory.headerOffset + 16);
    var buffer = Buffer.concat([execBuffer, zipBuffer]);
    fs.writeFileSync(targetPath, buffer);
    if (process.platform !== 'win32' && process.platform === platform)
        fs.chmodSync(targetPath, 0x777);
    console.log('Created ' + platform + ' app: ' + targetPath);
}

function createPlist(appName) {
    return '<?xml version="1.0" encoding="UTF-8"?>\n\
<!DOCTYPE plist PUBLIC "-//Apple//DTD PLIST 1.0//EN" "http://www.apple.com/DTDs/PropertyList-1.0.dtd">\n\
<plist version="1.0">\n\
<dict>\n\
    <key>CFBundleName</key>\n\
    <string>ioui</string>\n\
    <key>CFBundleDisplayName</key>\n\
    <string>IoUi</string>\n\
    <key>CFBundleIdentifier</key>\n\
    <string>org.io-ui</string>\n\
    <key>CFBundleIconFile</key>\n\
    <string>icon</string>\n\
    <key>CFBundleVersion</key>\n\
    <string>0.0.1</string>\n\
    <key>CFBundlePackageType</key>\n\
    <string>APPL</string>\n\
    <key>CFBundleSignature</key>\n\
    <string>APSE</string>\n\
    <key>CFBundleExecutable</key>\n\
    <string>' + appName + '</string>\n\
    <key>NSPrincipalClass</key>\n\
    <string>NSApplication</string>\n\
    <key>NSHighResolutionCapable</key>\n\
    <true/>\n\
</dict>\n\
</plist>';
}
