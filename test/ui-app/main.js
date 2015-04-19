var config = require('./config.json');
var http = require('http');
var ui = require('ui');
var app,
    transport,
    ev = [];
var conf = {
    url: '/',
    server: {
        basePath: 'app'
    },
    window: {
        title: 'io-ui tests app loading...',
        width: 600,
        height: 400,
        left: '10%',
        top: '10%',
        frame: true,
        resizable: true,
        topmost: false,
        opacity: 1,
        state: ui.Window.STATE_NORMAL,
        menu: [{
            title: 'My menu',
            items: [
                { title: 'Hello' },
                { title: 'World', items: [{ title: 'w1' }, { title: 'w2' }] },
                { type: ui.Window.MENU_TYPE_SEPARATOR },
                { title: 'Exit', callback: function () { console.log('Menu exit'); process.exit(0); } }]
        }, {
            title: 'Help',
            items: [{ title: 'Some help...' }]
        }]
    }
};

function run() {
    app = ui.run(conf);
    app.server.backend = function (req, res) {
        throw 'err';
    };
    app.window.on('documentComplete', function() {
        ev.push('documentComplete');
    });
}

var JsTransport = function(port) {
    var lastReqId = 0;

    function openReq(data) {
        data = data ? data.toString() : '';
        var options = {
            hostname: '127.0.0.1',
            port: port,
            path: '/',
            method: 'POST',
            headers: {
                'Content-Type': 'text/plain',
                'Content-Length': data.length.toString()
            }
        };
        if (lastReqId)
            options.headers['x-req-id'] = lastReqId.toString();
        var req = http.request(options, function(res) {
            lastReqId = res.headers['x-req-id'];
            res.setEncoding('utf8');
            var body = '';
            res.on('data', function (data) { body += data; });
            res.on('end', function () {
                processBody(body);
            });
        });

        req.on('error', function(e) {
            openReq();
        });

        req.end(data);
    }

    function processBody(body) {
        var result;
        try {
            var data = eval(body);
            result = { data: data };
        } catch (e) {
            result = { error: e.toString() };
        }
        openReq(JSON.stringify(result));
    }

    (function() {
        openReq();
    })();
};
transport = new JsTransport(config.server_port);
