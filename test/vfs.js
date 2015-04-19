var
    Module = require('module'),
    fs = require('fs'),
    path = require('path'),
    node_stream_zip = require(path.resolve(__dirname, '../node_modules/node-stream-zip/node_stream_zip.js'))

var Module_load = Module._load;
Module._load = function(request, parent, isMain) {
    if (request === 'node_stream_zip')
        return node_stream_zip;
    return Module_load.call(Module, request, parent, isMain);
};

var vfs = require('../js/vfs.js');

process.chdir(path.join(__dirname, 'vfs_root'));

module.exports.setUp = function(callback) {
    vfs.init({
        contentFile: 'vfs.zip',
        basePath: '.',
        success: function() {
            callback();
        },
        error: function(err) {
            console.error(err);
            throw err;
        }
    });
};

module.exports.tearDown = function(callback) {
    vfs.deinit();
    callback();
};

module.exports.statSync_file = function(test) {
    test.expect(5);
    var stat = fs.statSync('file1.txt');
    test.ok(stat);
    test.ok(stat.isFile());
    test.ok(!stat.isDirectory());
    test.equal(stat.size, 5);
    test.ok(stat.vfs);
    test.done();
};

module.exports.statSync_fileInFolder = function(test) {
    test.expect(5);
    var stat = fs.statSync('folder2/folder3/file4.txt');
    test.ok(stat);
    test.ok(stat.isFile());
    test.ok(!stat.isDirectory());
    test.equal(stat.size, 5);
    test.ok(stat.vfs);
    test.done();
};

module.exports.statSync_folder = function(test) {
    test.expect(5);
    var stat = fs.statSync('folder1');
    test.ok(stat);
    test.ok(!stat.isFile());
    test.ok(stat.isDirectory());
    test.equal(stat.size, 0);
    test.ok(stat.vfs);
    test.done();
};

module.exports.statSync_notExisting = function(test) {
    test.expect(1);
    test.throws(function() {
        stat = fs.statSync('notExisting');
    }, 'ENOENT');
    test.done();
};

module.exports.statSync_outsideFile = function(test) {
    test.expect(5);
    var stat = fs.statSync('folderOut');
    test.ok(stat);
    test.ok(stat.isDirectory());
    stat = fs.statSync('fileOut.txt');
    test.ok(stat);
    test.equal(stat.size, 7);
    test.ok(!stat.vfs);
    test.done();
};

module.exports.stat_file = function(test) {
    test.expect(5);
    fs.stat('file1.txt', function(err, stat) {
        test.ok(stat);
        test.ok(stat.isFile());
        test.ok(!stat.isDirectory());
        test.equal(stat.size, 5);
        test.ok(stat.vfs);
        test.done();
    });
};

module.exports.stat_fileInFolder = function(test) {
    test.expect(5);
    fs.stat('folder2/folder3/file4.txt', function(err, stat) {
        test.ok(stat);
        test.ok(stat.isFile());
        test.ok(!stat.isDirectory());
        test.equal(stat.size, 5);
        test.ok(stat.vfs);
        test.done();
    });
};

module.exports.stat_folder = function(test) {
    test.expect(5);
    fs.stat('folder1', function(err, stat) {
        test.ok(stat);
        test.ok(!stat.isFile());
        test.ok(stat.isDirectory());
        test.equal(stat.size, 0);
        test.ok(stat.vfs);
        test.done();
    });
};

module.exports.stat_notExisting = function(test) {
    test.expect(1);
    fs.stat('notExisting', function (err, stat) {
        test.equal(err.code, 'ENOENT');
        test.done();
    });
};

module.exports.stat_outsideFile = function(test) {
    test.expect(5);
    fs.stat('folderOut', function(err, stat) {
        test.ok(stat);
        test.ok(stat.isDirectory());
        stat = fs.statSync('fileOut.txt');
        test.ok(stat);
        test.equal(stat.size, 7);
        test.ok(!stat.vfs);
        test.done();
    });
};

module.exports.readdirSync_top = function(test) {
    test.expect(1);
    var dir = fs.readdirSync('folder2');
    test.equal(dir.join(','), 'file2.txt,folder3');
    test.done();
};

module.exports.readdirSync_inDir = function(test) {
    test.expect(1);
    var dir = fs.readdirSync('folder2/folder3');
    test.equal(dir.join(','), 'file3.txt,file4.txt');
    test.done();
};

module.exports.readdirSync_outside = function(test) {
    test.expect(1);
    var dir = fs.readdirSync('folderOut');
    test.equal(dir.join(','), 'file2.txt');
    test.done();
};

module.exports.readdirSync_notExisting = function(test) {
    test.expect(1);
    test.throws(function() {
        fs.readdirSync('notExisting');
    }, 'ENOENT');
    test.done();
};

module.exports.readdir_top = function(test) {
    test.expect(1);
    fs.readdir('folder2', function(err, dir) {
        test.equal(dir.join(','), 'file2.txt,folder3');
        test.done();
    });
};

module.exports.readdir_inDir = function(test) {
    test.expect(1);
    fs.readdir('folder2/folder3', function(err, dir) {
        test.equal(dir.join(','), 'file3.txt,file4.txt');
        test.done();
    });
};

module.exports.readdir_outside = function(test) {
    test.expect(1);
    fs.readdir('folderOut', function(err, dir) {
        test.equal(dir.join(','), 'file2.txt');
        test.done();
    });
};

module.exports.readdir_notExisting = function(test) {
    test.expect(1);
    fs.readdir('notExisting', function (err, dir) {
        test.equal(err.code, 'ENOENT');
        test.done();
    });
};

module.exports.accessSync_fileVisible = function(test) {
    test.expect(1);
    test.doesNotThrow(function() {
        fs.accessSync('file1.txt', fs.F_OK);
        fs.accessSync('file1.txt');
    });
    test.done();
};

module.exports.accessSync_fileRead = function(test) {
    test.expect(1);
    test.doesNotThrow(function() {
        fs.accessSync('file1.txt', fs.R_OK);
    });
    test.done();
};

module.exports.accessSync_fileExecute = function(test) {
    test.expect(1);
    test.doesNotThrow(function() {
        fs.accessSync('file1.txt', fs.X_OK);
    });
    test.done();
};

module.exports.accessSync_fileWrite = function(test) {
    test.expect(1);
    test.throws(function() {
        fs.accessSync('file1.txt', fs.W_OK);
    }, 'EPERM');
    test.done();
};

module.exports.accessSync_fileReadWrite = function(test) {
    test.expect(1);
    test.throws(function() {
        fs.accessSync('file1.txt', fs.R_OK | fs.W_OK);
    }, 'EPERM');
    test.done();
};

module.exports.accessSync_fileRead = function(test) {
    test.expect(1);
    test.doesNotThrow(function() {
        fs.accessSync('file1.txt', fs.R_OK);
    });
    test.done();
};

module.exports.accessSync_folderRead = function(test) {
    test.expect(1);
    test.doesNotThrow(function() {
        fs.accessSync('folder1', fs.R_OK);
    });
    test.done();
};

module.exports.accessSync_folderWrite = function(test) {
    test.expect(1);
    test.throws(function() {
        fs.accessSync('folder1', fs.W_OK);
    }, 'EPERM');
    test.done();
};

module.exports.accessSync_fileOutside = function(test) {
    test.expect(1);
    test.doesNotThrow(function() {
        fs.accessSync('fileOut.txt', fs.R_OK | fs.W_OK);
    }, 'EPERM');
    test.done();
};

module.exports.accessSync_folderOutside = function(test) {
    test.expect(1);
    test.doesNotThrow(function() {
        fs.accessSync('folderOut', fs.R_OK);
    }, 'EPERM');
    test.done();
};

module.exports.access_fileVisible = function(test) {
    test.expect(1);
    fs.access('file1.txt', fs.F_OK, function(err) {
        test.equal(null, err);
        test.done();
    });
};

module.exports.access_fileRead = function(test) {
    test.expect(1);
    fs.access('file1.txt', fs.R_OK, function(err) {
        test.equal(null, err);
        test.done();
    });
};

module.exports.access_fileExecute = function(test) {
    test.expect(1);
    fs.access('file1.txt', fs.X_OK, function(err) {
        test.equal(null, err);
        test.done();
    });
};

module.exports.access_fileWrite = function(test) {
    test.expect(1);
    fs.access('file1.txt', fs.W_OK, function(err) {
        test.equal(err && err.code, 'EPERM');
        test.done();
    });
};

module.exports.access_fileReadWrite = function(test) {
    test.expect(1);
    fs.access('file1.txt', fs.R_OK | fs.W_OK, function(err) {
        test.equal(err && err.code, 'EPERM');
        test.done();
    });
};

module.exports.access_fileRead = function(test) {
    test.expect(1);
    fs.access('file1.txt', fs.R_OK, function(err) {
        test.equal(null, err);
        test.done();
    });
};

module.exports.access_folderRead = function(test) {
    test.expect(1);
    fs.access('folder1', fs.R_OK, function(err) {
        test.equal(null, err);
        test.done();
    });
};

module.exports.access_folderWrite = function(test) {
    test.expect(1);
    fs.access('folder1', fs.W_OK, function(err) {
        test.equal(err && err.code, 'EPERM');
        test.done();
    });
};

module.exports.access_fileOutside = function(test) {
    test.expect(1);
    fs.access('fileOut.txt', fs.R_OK | fs.W_OK, function(err) {
        test.equal(null, err);
        test.done();
    });
};

module.exports.access_folderOutside = function(test) {
    test.expect(1);
    fs.access('folderOut', fs.R_OK, function(err) {
        test.equal(null, err);
        test.done();
    });
};

module.exports.readFileSync_file = function(test) {
    test.expect(1);
    var content = fs.readFileSync('file1.txt', 'utf8');
    test.equal('file1', content);
    test.done();
};

module.exports.readFileSync_outside = function(test) {
    test.expect(1);
    var content = fs.readFileSync('fileOut.txt', 'utf8');
    test.equal('fileOut', content);
    test.done();
};

module.exports.readFile_file = function(test) {
    test.expect(1);
    fs.readFile('file1.txt', 'utf8', function(err, content) {
        test.equal('file1', content);
        test.done();
    });
};

module.exports.readFile_outside = function(test) {
    test.expect(1);
    fs.readFile('fileOut.txt', 'utf8', function(err, content) {
        test.equal('fileOut', content);
        test.done();
    });
};

module.exports.stream = function(test) {
    test.expect(3);
    vfs.stream(path.resolve('folder2//folder3/file4.txt'), function(err, stm) {
        test.equal(err, null);
        test.ok(stm);
        var content = '';
        stm.on('data', function(chunk) {
            content += chunk.toString();
        });
        stm.on('end', function() {
            test.equal(content, 'file4');
            test.done();
        });
    });
};

module.exports.package_load = function(test) {
    test.expect(2);
    var optimist;
    test.doesNotThrow(function() { optimist = require('./vfs_root/node_modules/optimist'); });
    test.ok(optimist);
    test.done();
};
