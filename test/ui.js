var actionTimeout = 0, // set convenient timeout for visual property inspection when required
    inheritStdio = false; // whether to show stdio of child process

var path = require('path'),
    ps = require('child_process'),
    http = require('http'),
    assert = require('assert');

var WindowState = {
    normal: 0,
    hidden: 1,
    maximized: 2,
    minimized: 3,
    fullscreen: 4
};

process.chdir(path.join(__dirname));
var appPath = path.resolve('ui-app');
var config = require('./ui-app/config.json');

var JsTransport = function(port, ready) {
    var
        server,
        pendingRes,
        callbacks = {},
        lastReqId = 0,
        isFirstReq = true;

    (function() {
        listen();
        setTimeout(function() {
            if (isFirstReq) {
                console.error('Server not started at port ' + port);
                assert.fail();
            }
        }, config.timeout);
    })();

    function listen() {
        server = http.createServer(function(req, res) {
            if (pendingRes)
                closeRequestWithData('New req');
            if (req.method == 'POST') {
                var body = '';
                req.on('data', function (data) { body += data; });
                req.on('end', function () {
                    if (isFirstReq) {
                        isFirstReq = false;
                        setImmediate(ready);
                    }
                    pendingRes = res;
                    var reqId = req.headers['x-req-id'];
                    var callback = callbacks[reqId];
                    if (callback) {
                        try {
                            body = JSON.parse(body);
                        } catch (e) {
                            console.error('Failed to parse response: \n' + body + '\n' + e);
                            throw e;
                        }
                        setImmediate(callback.bind(null, body));
                        delete callbacks[lastReqId];
                    }
                });
            } else {
                res.writeHead(500, { 'Content-Type': 'text/plain' });
                res.end('Not supported');
            }
        }).listen(port);
    }

    function closeRequestWithData(data, reqId) {
        if (!pendingRes) {
            console.error('Peer disconnected. Failed to send data: ' + data);
            throw 'Peer disconnected';
        }
        var headers = { 'Content-Type': 'text/plain' };
        if (reqId)
            headers['x-req-id'] = reqId;
        pendingRes.writeHead(200, headers);
        pendingRes.end(data, 'utf8');
        pendingRes = null;
    }

    this.send = function(data, callback) {
        lastReqId++;
        if (callback) {
            callbacks[lastReqId] = callback;
            setTimeout((function(reqId) {
                if (callbacks[reqId]) {
                    console.error('Request not finished: ' + data);
                    assert.fail();
                }
            }).bind(lastReqId), config.timeout);
        }
        closeRequestWithData(data, callback ? lastReqId : null);
    };

    this.close = function(callback) {
        callbacks = {};
        if (pendingRes) {
            pendingRes.end();
            pendingRes = null;
        }
        if (server) {
            server.close(callback);
            server = null;
        }
    };
};

var appProcess,
    serverTransport;

function wait(condition, callback, timeout) {
    if (timeout === undefined)
        timeout = config.timeout;
    serverTransport.send(condition, function(resp) {
        if (resp.error) {
            console.error('Wait failed: ' + condition + ' (' + resp.error + ')');
            throw resp.error;
        }
        if (resp.data) {
            callback();
            return;
        }
        if (timeout <= 0) {
            console.error('Wait failed: ' + condition + ' (timeout)');
            throw 'Wait failed: timeout';
        }
        var timeDiff = 100;
        timeout -= timeDiff;
        setTimeout(function() {
            wait(condition, callback, timeout);
        }, timeDiff);
    });
}

function start(prepareScript, callback) {
    var serverPromise = new Promise(function(resolve) {
        serverTransport = new JsTransport(config.server_port, function() {
            serverTransport.send(prepareScript + ';\nrun()', function() {
                wait('ev.indexOf("documentComplete") >= 0', resolve);
            });
        });
    });
    Promise.all([serverPromise]).then(function() { callback(); });
}

module.exports.setUp = function(callback) {
    appProcess = ps.spawn('../build/node', [appPath], { stdio: inheritStdio ? 'inherit' : null });
    callback();
};

module.exports.tearDown = function(callback) {
    try { appProcess.kill(); } catch (e) {}
    var serverPromise = new Promise(function(resolve) {
        serverTransport.close(resolve);
    });
    Promise.all([serverPromise]).then(function() { callback(); });
};

module.exports.ping = function(test) {
    test.expect(1);
    start('', function() {
        var serverPromise = new Promise(function(resolve) {
            serverTransport.send('"ping"', function(resp) {
                test.equal(resp.data, 'ping');
                resolve();
            });
        });
        Promise.all([serverPromise]).then(function() { test.done(); });
    });
};

function testPropertyTransition(prop, propFrom, propTo, test) {
    test.expect(1);
    start('conf.window.' + prop + ' = ' + propFrom, function() {
        setTimeout(function() {
            serverTransport.send('var result = app.window.' + prop + '; app.window.' + prop + ' = ' + propTo +
                    '; result += "=>" + app.window.' + prop, function(resp) {
                var expected = propFrom + '=>' + propTo;
                if (resp.data === expected) {
                    test.equal(resp.data, expected);
                    setTimeout(function() { test.done(); }, actionTimeout);
                } else {
                    var check = 'app.window.' + prop + ' == ' + propTo;
                    if (typeof propTo === 'number') {
                        var check = 'Math.abs(app.window.' + prop + ' - ' + propTo + ') < .001';
                    }
                    wait(check, function() {
                        test.ok(true);
                        setTimeout(function() { test.done(); }, actionTimeout);
                    }, 500)
                }
            });
        }, actionTimeout);
    });
}

Object.keys(WindowState).forEach(function(stateFrom) {
    Object.keys(WindowState).forEach(function(stateTo) {
        var name = 'state: ' + stateFrom;
        if (stateFrom !== stateTo)
            name += ' => ' + stateTo;
        module.exports[name] = testPropertyTransition.bind(null, 'state', WindowState[stateFrom], WindowState[stateTo]);
    });
});

module.exports.title = function(test) {
    test.expect(1);
    start('', function() {
        serverTransport.send('app.window.title', function(resp) {
            test.equal(resp.data, 'io-ui test app');
            test.done();
        });
    });
};

['resizable', 'frame', 'topmost'].forEach(function(prop) {
    module.exports[prop + ': true => false'] = testPropertyTransition.bind(null, prop, true, false);
    module.exports[prop + ': false => true'] = testPropertyTransition.bind(null, prop, false, true);
});
module.exports['opacity: 1 => .2'] = testPropertyTransition.bind(null, 'opacity', 1, .2);
module.exports['opacity: .2 => 1'] = testPropertyTransition.bind(null, 'opacity', .2, 1);
module.exports['width'] = testPropertyTransition.bind(null, 'width', 600, 400);
module.exports['height'] = testPropertyTransition.bind(null, 'height', 600, 400);
module.exports['left'] = testPropertyTransition.bind(null, 'left', 100, 200);
module.exports['top'] = testPropertyTransition.bind(null, 'top', 100, 200);

module.exports['empty config'] = function(test) {
    test.expect(0);
    start('conf.window = {}', function() {
        wait('app.window.state === ui.Window.STATE_NORMAL && app.window.width > 0 && app.window.height > 0' +
            ' && app.window.top > 0 && app.window.left > 0', function() {
                setTimeout(function() { test.done(); }, actionTimeout);
            });
    });
};

module.exports['undefined config'] = function(test) {
    test.expect(0);
    start('delete conf.window;', function() {
        wait('app.window.state === ui.Window.STATE_NORMAL && app.window.width > 0 && app.window.height > 0' +
            ' && app.window.top > 0 && app.window.left > 0', function() {
                setTimeout(function() { test.done(); }, actionTimeout);
            });
    });
};

module.exports['close'] = function(test) {
    test.expect(1);
    start('', function() {
        appProcess.on('exit', function() {
            test.equal(appProcess.exitCode, 0);
            setTimeout(function() { test.done(); }, actionTimeout);
            clearTimeout(errTimeout);
        });
        var errTimeout = setTimeout(function() {
            test.fail('process not exited');
            test.done();
        }, 1000);
        serverTransport.send('app.window.close();');
    });
};

module.exports['move'] = function(test) {
    test.expect(0);
    start('', function() {
        serverTransport.send('app.window.move(100, 50, 200, 300);', function() {
            wait('app.window.left === 100 && app.window.top === 50 && app.window.width === 200 && app.window.height === 300', function() {
                setTimeout(function() { test.done(); }, actionTimeout);
            });
        });
    });
};

module.exports['multi-window: close main'] = function(test) {
    test.expect(1);
    start('', function() {
        serverTransport.send('app.w = new ui.Window({ top: 0, left: 0 });app.w.show();app.w.on("ready", function() { app.wr = true; });', function(resp) {
            wait('app.wr === true', function() {
                serverTransport.send('app.window.close();');
                appProcess.on('exit', function() {
                    test.equal(appProcess.exitCode, 0);
                    setTimeout(function() { test.done(); }, actionTimeout);
                    clearTimeout(errTimeout);
                });
                var errTimeout = setTimeout(function() {
                    test.fail('process not exited');
                    test.done();
                }, 1000);
            });
        });
    });
};

module.exports['multi-window: close secondary'] = function(test) {
    test.expect(0);
    start('', function() {
        serverTransport.send('app.w = new ui.Window({ top: 0, left: 0 });app.w.show();app.w.on("ready", function() { app.wr = true; });', function(resp) {
            wait('app.wr === true', function() {
                serverTransport.send('app.w.on("close", function() { app.wc = true; });app.w.close();', function() {
                    wait('app.wc === true', function() {
                        setTimeout(function() { test.done(); }, 300);
                        clearTimeout(errTimeout);
                    });
                });
                var errTimeout = appProcess.on('exit', function() {
                    test.fail('process exited on secondary window close');
                    test.done();
                });
            });
        });
    });
};
