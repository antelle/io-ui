var ui = require('ui');
var app = ui.run({
    url: '/',
    server: {
        basePath: 'app'
    },
    window: {
        title: 'Editor',
        width: 600,
        height: 400,
        left: '10%',
        top: '10%',
        frame: true,
        resizable: true,
        topmost: false,
        opacity: 1,
        state: ui.Window.STATE_NORMAL,
        menu: []
    }
});
app.server.backend = function (req, res) {
    var qs = require('querystring');
    if (req.method == 'POST') {
        var body = '';
        req.on('data', function (data) {
            body += data;
        });
        req.on('end', function () {
            var post = qs.parse(body);
            if (req.url.lastIndexOf('/exec', 0) === 0) {
                console.log('EXEC\n' + post.code);
                res.writeHead(200, { 'Content-Type': 'text/plain' });
                var result;
                try { result = eval(post.code); }
                catch (err) { result = 'error: ' + err; }
                result = JSON.stringify(result, null, 4);
                return res.end(result);
            } else {
                return res.end();
            }
        });
    } else {
        throw 'err';
    }
};
