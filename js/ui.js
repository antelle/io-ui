(function() {
    'use strict';

    var DEFAULT_CEF_URL = 'https://dl.bintray.com/antelle/io-ui/cef-release-{platform}-{arch}-{ver}.zip';

    if (process.perfTraceReg) process.perfTraceReg(10);
    var ui = process.binding('ui_wnd');
    if (process.perfTraceReg) process.perfTraceReg(11);
    var
        events = require('events'),
        http = require('http'),
        util = require('util'),
        path = require('path'),
        fs = require('fs'),
        vfs;

    var MIME_TYPES = {
        'js': 'application/javascript',
        'json': 'application/json',
        'pdf': 'application/pdf',
        'rtf': 'application/rtf',
        'xml': 'application/xml',
        'xsl': 'application/xml',
        'dtd': 'application/xml-dtd',
        'xslt': 'application/xslt+xml',
        'eot': 'application/vnd.ms-fontobject',
        'ttf': 'application/x-font-ttf',
        'woff': 'application/font-woff',
        'otf': 'application/x-font-otf',
        'zip': 'application/zip',
        'jpeg': 'image/jpeg',
        'jpg': 'image/jpeg',
        'jpe': 'image/jpeg',
        'gif': 'image/gif',
        'png': 'image/gif',
        'svg': 'image/svg+xml',
        'tiff': 'image/tiff',
        'tif': 'image/tiff',
        'ico': 'image/x-icon',
        'appcache': 'text/cache-manifest',
        'css': 'text/css',
        'csv': 'text/csv',
        'html': 'text/html',
        'htm': 'text/html',
        'txt': 'text/plain',
        'text': 'text/plain',
        'log': 'text/plain',
        'mp4': 'video/mp4',
        'mp4v': 'video/mp4',
        'mpg4': 'video/mp4',
        'mpeg': 'video/mpeg',
        'mpg': 'video/mpeg',
        'mpe': 'video/mpeg',
        'm1v': 'video/mpeg',
        'm2v': 'video/mpeg',
        'ogv': 'video/ogg',
        'qt': 'video/quicktime'
    };

    ui.Window.prototype.__proto__ = events.EventEmitter.prototype;

    var FileProviderDir = function(basePath) {
        var that = this;

        this.serveFile = function(req, res) {
            var relPath = req.url.substr(1);
            var absPath = path.join(basePath, relPath);
            if (absPath.lastIndexOf(basePath, 0) !== 0) {
                return this.serveBackend(req, res);
            }
            this.checkWriteFile(absPath, req, res);
        };

        this.checkWriteFile = function(absPath, req, res) {
            fs.stat(absPath, function(err, stat) {
                if (stat) {
                    if (stat.isFile()) {
                        that.writeFile(res, absPath, stat.size, stat.vfs);
                    } else if (stat.isDirectory()) {
                        that.checkWriteFile(path.join(absPath, that.index), req, res);
                    }
                } else {
                    that.serveBackend(req, res);
                }
            });
        };

        this.writeFile = function (res, absPath, size, fromVfs) {
            var successHeaders = {
                'Content-Type': MIME_TYPES[path.extname(absPath).replace('.', '')] || '',
                'Content-Length': size,
                'Cache-Control': 'no-cache, no-store, must-revalidate',
                'Pragma': 'no-cache',
                'Expires': '0'
            };
            if (fromVfs) {
                if (!vfs)
                    vfs = require('vfs');
                vfs.stream(absPath, function(err, stm) {
                    if (err) {
                        res.writeHead(500, { 'Content-Type': 'text/plain' });
                        res.end(err.toString());
                    } else {
                        res.writeHead(200, successHeaders);
                        stm.pipe(res);
                    }
                });
            } else {
                res.writeHead(200, successHeaders);
                var fileStm = fs.createReadStream(absPath);
                fileStm.pipe(res);
            }
        };
    };

    ui.Server = function(config) {
        this.httpServer = config.httpServer;
        this.host = config.host;

        var that = this;

        this.fp = new FileProviderDir(config.basePath);
        this.fp.serveBackend = function(req, res) { that.serveBackend(req, res); };
        this.fp.index = config.index || 'index.html';

        this.backend = function(req, res) {
            res.writeHead(404, { 'Content-Type': 'text/plain' });
            return res.end('Not found: ' + req.url);
        };

        this.process = function (req, res) {
            if (this.host && (req.headers.host !== this.host)) {
                res.writeHead(400, { 'Content-Type': 'text/plain' });
                return res.end();
            }
            if (req.method === 'GET') {
                this.serveFile(req, res);
            } else {
                this.serveBackend(req, res);
            }
        };

        this.serveBackend = function(req, res) {
            try {
                this.backend(req, res);
            } catch (err) {
                res.writeHead(500, { 'Content-Type': 'text/plain' });
                return res.end(err.toString());
            }
        };

        this.serveFile = function(req, res) {
            this.fp.serveFile(req, res);
        };
    };

    ui.Server.prototype.__proto__ = events.EventEmitter.prototype;

    ui.Server.createDefault = function(config) {
        var
            MAX_TRIES = 10,
            MIN_PORT = 20000,
            MAX_PORT = 40000;
        var
            addr = '127.0.0.1',
            tryNum = 1,
            execPath = process.execPath + __filename,
            hash = 0;
        for (var i = 0; i < execPath.length; i++)
            hash += execPath.charCodeAt(i);
        var port = MIN_PORT + (hash % (MAX_PORT - MIN_PORT));
        config.host = addr + ':' + port;
        config.basePath = path.normalize(config.basePath);
        var server = new ui.Server(config);
        server.httpServer = http.createServer(server.process.bind(server));
        server.httpServer.listen(port, addr);
        server.httpServer.on('listening', function() {
            setImmediate(function() { server.emit('ready'); });
        });
        server.httpServer.on('request', function(req, res) {
            server.emit('request', req, res);
        });
        server.httpServer.on('error', function (e) {
            if (tryNum++ > MAX_TRIES) {
                return;
            }
            if (e.message.indexOf('EADDRINUSE') >= 0) {
                port = Math.round(MIN_PORT + (MAX_PORT - MIN_PORT) * Math.random());
                server.host = addr + ':' + port;
                server.httpServer.listen(port, addr);
            }
        });
        return server;
    };

    ui.App = function(config) {
        var that = this;

        this.url = config.url;

        var windowConfig = util._extend({}, config.window);
        this.window = new ui.Window(windowConfig);

        var serverConfig = util._extend({}, config.server);
        this.server = ui.Server.createDefault(serverConfig);

        this.run = function () {
            var engineReadyPromise = new Promise(function(resolve) {
                if (checkVersion(false)) {
                    resolve();
                } else {
                    downloadCef(resolve);
                }
            });
            var windowPromise = new Promise(function(resolve) {
                engineReadyPromise.then(function() {
                    that.window.show();
                    that.window.on('ready', function() {
                        if (process.perfTraceReg) process.perfTraceReg(15);
                        setImmediate(resolve);
                    });
                });
            });
            var serverPromise = new Promise(function(resolve) {
                that.server.on('ready', function () {
                    if (process.perfTraceReg) process.perfTraceReg(12);
                    setImmediate(resolve);
                });
            });
            Promise.all([windowPromise, serverPromise]).then(function() {
                setImmediate(function() {
                    that.window.navigate(that.resolveUrl(that.url));
                });
            });
            this.server.once('request', function() {
                if (process.perfTraceReg) process.perfTraceReg(18);
            });
            this.window.once('documentComplete', function () {
                var uptime, perfStat;
                if (process.perfTraceReg) {
                    process.perfTraceReg(20);
                    perfStat = ui.getPerfStat();
                    var lastStat = perfStat.length - 1;
                    while (!perfStat[lastStat])
                        lastStat--;
                    uptime = Math.round(perfStat[lastStat]);
                } else {
                    uptime = Math.round(process.uptime() * 1000);
                }
                console.log('[+' + uptime + 'ms] ui: v' + ui.version + ', engine: ' + ui.engine.name + ' v' + ui.engine.version + ', server: ' + that.server.host
                    +  (perfStat ? '\nperf: ' + perfStat : ''));
            });
        };

        this.resolveUrl = function(url) {
            if (!url) {
                url = 'http://' + this.server.host;
            } else if (url.indexOf('://') < 0) {
                if (url.indexOf('/') !== 0) {
                    url = '/' + url;
                }
                url = 'http://' + this.server.host + url;
            }
            return url;
        };

        function checkVersion(exitIfLower) {
            var requested = config && config.support && config.support[ui.engine.name];
            var running = ui.engine.version;
            if (requested) {
                requested = requested.split('.');
                running = running.split('.');
                for (var i = 0; i < requested.length; i++) {
                    if ((running[i]|0) > (requested[i]|0))
                        return true;
                    if ((running[i]|0) < (requested[i]|0)) {
                        if (exitIfLower) {
                            var msg = 'Sorry, your system is not supported.\n' + 'Technical details: ' + msg +
                                ui.engine.name + ' version ' + running.join('.') + ', supported from ' + requested.join('.');
                            errorExit(msg);
                        }
                        return false;
                    }
                }
            }
            return true;
        }

        function downloadCef(success) {
            var url = config.cef && config.cef.hasOwnProperty('url') ? config.cef.url : DEFAULT_CEF_URL;
            var ver = ui.getSupportedCefVersion();
            if (url && ver && ui.engine.name !== 'cef') {
                if (ui.Window.alert('Additional component is required to run this app. Do you want to download it (~30 MB)?', ui.Window.ALERT_QUESTION)) {
                    url = url.replace('{platform}', process.platform).replace('{arch}', process.arch).replace('{ver}', ver);
                    downloadAndExtract(url, function() {
                        ui.updateEngineVersion();
                        checkVersion(true);
                        success();
                    });
                } else {
                    process.exit(3);
                }
            } else {
                checkVersion(true);
            }
        }

        function downloadAndExtract(url, success) {
            var tmpFilePath = path.join(path.dirname(process.execPath), '.tmp-' + path.basename(url));
            console.log('Downloading: ' + url + ' => ' + tmpFilePath);
            var tmpFile;
            try {
                tmpFile = fs.createWriteStream(tmpFilePath);
            } catch (err) {
                errorExit('Cannot download necessary app component: access to file is denied. Please, run this app as Administrator first time', err);
                return;
            }
            // ui.showProgressDialog();
            var h = url.lastIndexOf('https', 0) === 0 ? require('https') : http;
            h.get(url, function(response) {
                var contentLength = response.headers['content-length'],
                    downloadedLength = 0;
                if (response.statusCode !== 200) {
                    tmpFile.close();
                    fs.unlink(tmpFilePath);
                    errorExit('Error downloading app component: supporting file for your app version was not found', 'response status code ' + response.statusCode);
                    return;
                }
                response.pipe(tmpFile);
                tmpFile.on('finish', function() {
                    tmpFile.close(function() {
                        extractFile(tmpFilePath, path.dirname(process.execPath), success);
                    });
                });
                if (contentLength) {
                    response.on('data', function (chunk) {
                        downloadedLength += chunk.length;
                        console.log('Downloaded: ' + (downloadedLength / contentLength));
                        //ui.setProgress(downloadedLength / contentLength);
                    });
                } else {
                    //ui.setProgress(null);
                }
            }).on('error', function(err) {
                tmpFile.close();
                fs.unlink(tmpFilePath);
                errorExit('Network error while downloading app component', err);
            });
        }

        function extractFile(file, folder, success) {
            var errMsg = 'Error extracting necessary component';
            var StreamZip = require('node_stream_zip');
            var zip = new StreamZip({ file: file });
            zip.on('error', function(err) {
                zip.close();
                fs.unlink(file);
                errorExit(errMsg, err);
            });
            zip.on('ready', function() {
                zip.extract(null, folder, function(err) {
                    zip.close();
                    fs.unlink(file);
                    if (err) {
                        errorExit(errMsg, err);
                    } else {
                        success();
                    }
                });
            });
        }

        function errorExit(msg, err) {
            // ui.hideProgressDialog();
            console.error(msg, err);
            ui.Window.alert(msg, ui.Window.ALERT_ERROR);
            process.exit(2);
        }
    };

    ui.run = function (config) {
        var app = new ui.App(config);
        app.run();
        return app;
    };

    module.exports = ui;
})();