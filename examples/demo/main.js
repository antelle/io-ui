var ui = require('ui');
var app = ui.run({
    url: '/',
    server: {
        basePath: 'app'
    },
    support: {
        "msie": "6.0.2",
        "webkit": "533.16",
        "webkitgtk": "2.3"
    },
    window: {
        title: 'Hello World',
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
            title: '&My menu',
            items: [
                { title: '&Hello' },
                { title: '&World', items: [{ title: 'w1' }, { title: 'w&2' }] },
                { type: ui.Window.MENU_TYPE_SEPARATOR },
                { title: 'E&xit', callback: function () { console.log('Menu exit'); process.exit(0); } }]
        }, {
            title: '&Help',
            items: [{ title: 'Some help...' }]
        }]
    }
});
app.server.backend = function (req, res) {
    if (req.url.lastIndexOf('/btn-click', 0) === 0) {
        res.writeHead(200, { 'Content-Type': 'text/plain' });
        return res.end((new Date()).toString());
    } else if (req.url.lastIndexOf('/runtime-info', 0) === 0) {
        res.writeHead(200, { 'Content-Type': 'text/plain' });
        return res.end('ui: v' + ui.version + ', engine: ' + ui.engine.name + ' v' + ui.engine.version + ', uptime: ' + process.uptime() + 's');
    }
    throw 'err';
};
app.window.once('documentComplete', function () {
    app.window.postMessage({ text: "Hello from backend" }, function(res, err) {
        console.log('Message processed: ' + (err ? 'ERROR: ' + err : JSON.stringify(res)));
    });
    app.window.onMessage = function(msg) {
        console.log('Message received: ' + JSON.stringify(msg));
        return { result: 'ok' };
    };
});
