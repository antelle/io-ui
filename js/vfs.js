(function() {
    'use strict';

    var
        fs = require('fs'),
        path = require('path'),
        binding = process.binding('fs'),
        StreamZip = require('node_stream_zip'),

        functions = { binding: { access: null, close: null, open: null, read: null, readdir: null, stat: null, fstat: null, lstat: null } },
        active = false,
        files = {},
        fds = [],
        contentFile = '',
        basePath = '',
        dt,
        contentZip,
        splitRegex = /[\/\\]/;

    function init(config) {
        if (active)
            return;
        active = true;
        files = {};
        fds = [0]; // to prevent returning -1 as fd
        contentFile = makeLongPath(config.contentFile);
        basePath = makeLongPath(config.basePath);
        dt = fs.statSync(contentFile).ctime;

        (function main() {
            loadArchive();
            initFsHook();
            if (config.initialized)
                config.initialized();
        })();

        function loadArchive() {
            files = { files: {} };
            contentZip = new StreamZip({ file: contentFile });
            contentZip.on('entry', function(entry) {
                var dir = files;
                var parts = entry.name.split('/');
                for (var i = 0; i < parts.length - 1; i++) {
                    var part = parts[i];
                    var newDir = dir.files[part];
                    if (!newDir) {
                        newDir = { files: {} };
                        dir.files[part] = newDir;
                    }
                    dir = newDir;
                }
                var name = parts[parts.length - 1];
                if (name)
                    dir.files[name] = entry.isFile ? entry : {files: {}};
            });
            contentZip.on('ready', function() {
                config.success();
            });
            contentZip.on('error', function(ex) {
                config.error(ex);
            });
        }

        function initFsHook() {
            var constants = process.binding('constants'),
                O_RDONLY = constants.O_RDONLY || 0,
                S_IFREG = constants.S_IFREG || 0,
                S_IFDIR = constants.S_IFDIR || 0;

            function err(req, str) {
                var ex = new Error(str);
                if (req)
                    req.oncomplete(ex);
                else
                    throw ex;
            }

            var FileWrap = function(entry) {
                this.entry = entry;
                this.size = entry.size;
                this.pos = 0;
                this.data = null;
            };

            FileWrap.prototype.read = function(buffer, offset, length, position, req) {
                if (!this.data) {
                    try {
                        this.data = contentZip.entryDataSync(this.entry);
                        if (this.data.length !== this.entry.size)
                            throw 'Bad data size';
                    } catch (ex) {
                        return err(req, ex);
                    }
                }
                if (!(buffer instanceof Buffer))
                    return err(req, 'Bad buffer');
                offset = +offset;
                if (isNaN(offset) || offset < 0 || offset >= buffer.length)
                    return err(req, 'Bad offset');
                length = +length;
                if (isNaN(length) || length < 0 || offset + length > buffer.length)
                    return err(req, 'Bad length');
                if (typeof position === 'number' && position >= 0) {
                    if (position >= this.size)
                        position = this.size;
                } else {
                    position = this.pos;
                }
                if (position + length > this.size)
                    length -= Math.max(this.size - position, 0);
                this.data.copy(buffer, offset, position, position + length);
                if (req) {
                    setImmediate(function() { req.oncomplete(null, length); });
                } else {
                    this.pos += length;
                    return length;
                }
            };

            moveFn(binding, functions.binding, Object.keys(functions.binding));

            binding.access = function(path, mode, req) {
                var f = getFile(path);
                if (!f)
                    return functions.binding.access.apply(binding, arguments);
                var ex = (mode & fs.W_OK) ? new Error('EPERM') : null;
                if (ex)
                    ex.code = ex.message;
                if (req)
                    setImmediate(function() { req.oncomplete(ex); });
                else if (ex)
                    throw ex;
            };

            binding.close = function(fd, req) {
                if (fd >= 0)
                    return functions.binding.close.apply(binding, arguments);
                var file = fds[-fd - 1];
                if (file)
                    fds[-fd - 1] = null;
                if (req)
                    setImmediate(function() { req.oncomplete(null); });
            };

            binding.open = function(path, flags, mode, req) {
                var file = getFile(path);
                var ex = null, fd = null;
                if (!file) {
                    return functions.binding.open.apply(binding, arguments);
                } else if ((flags & O_RDONLY) !== O_RDONLY) {
                    ex = new Error('EPERM');
                    ex.code = ex.message;
                } else {
                    fd = -fds.length - 1;
                    fds.push(new FileWrap(file));
                }
                if (req)
                    setImmediate(function() { req.oncomplete(ex, fd); });
                else if (ex)
                    throw ex;
                else
                    return fd;
            };

            binding.read = function(fd, buffer, offset, length, position, req) {
                if (fd >= 0)
                    return functions.binding.read.apply(binding, arguments);
                var file = fds[-fd - 1];
                if (!file || file.files) {
                    var ex = new Error(file ? 'EISDIR' : 'EBADF');
                    ex.code = ex.message;
                    if (req)
                        setImmediate(function() { req.oncomplete(ex, null); });
                    else
                        throw ex;
                }
                return file.read(buffer, offset, length, position, req);
            };

            binding.readdir = function(path, req) {
                var dir = getFile(path);
                if (!dir)
                    return functions.binding.readdir.apply(binding, arguments);
                var ex = dir.files ? null : 'ENOTDIR';
                var files = null;
                if (!ex) {
                    files = [];
                    for (var f in dir.files)
                        if (dir.files.hasOwnProperty(f))
                            files.push(f);
                }
                if (req)
                    setImmediate(function() { req.oncomplete(ex, files) });
                else if (ex)
                    throw ex;
                else
                    return files;
            };

            binding.stat = function(path, req) {
                var file = getFile(path);
                if (!file)
                    return functions.binding.stat.apply(binding, arguments);
                var isDir = !!file.files;
                var attr = isDir ? S_IFDIR : S_IFREG;
                var result = new fs.Stats(0, attr, 1, 0, 0, 0, undefined, undefined, file.size || 0, undefined, dt, dt, dt, dt);
                result.vfs = true;
                if (req)
                    setImmediate(function() { req.oncomplete(null, result); });
                else
                    return result;
            };

            binding.lstat = binding.stat;

            binding.fstat = function(fd, req) {
                if (fd >= 0)
                    return functions.binding.fstat.apply(binding, arguments);
                var file = fds[-fd - 1];
                var ex = file ? null : new Error('EBADF');
                if (ex)
                    ex.code = ex.message;
                var result = new fs.Stats(0, S_IFREG, 1, 0, 0, 0, undefined, undefined, file.size, undefined, dt, dt, dt, dt);
                result.vfs = true;
                if (req)
                    setImmediate(function() { req.oncomplete(ex, result); });
                else if (ex)
                    throw ex;
                else
                    return result;
            };
        }
    }

    function deinit() {
        if (!active)
            return;
        active = false;
        moveFn(functions.binding, binding, Object.keys(functions.binding));
    }

    function moveFn(from, to, props) {
        props.forEach(function(fn) {
            to[fn] = from[fn];
            from[fn] = null;
        });
    }

    function getFile(filePath) {
        filePath = filePath ? makeLongPath(filePath) : filePath;
        if (!filePath || filePath.length <= basePath.length + 1 || filePath.lastIndexOf(basePath, 0) !== 0)
            return null;
        filePath = filePath.substr(basePath.length + 1);
        var file = files;
        filePath.split(splitRegex).every(function(part) {
            if (part) {
                file = file.files && file.files[part] || null;
            }
            return file;
        });
        return file;
    }

    function stream(file, callback) {
        file = makeLongPath(file);
        var entry = getFile(file);
        if (!entry)
            return callback('File not found: ' + file);
        contentZip.stream(entry, callback);
    }

    function makeLongPath(p) {
        return path._makeLong(path.resolve(p));
    }

    module.exports = {
        init: init,
        deinit: deinit,
        stream: stream
    };
})();