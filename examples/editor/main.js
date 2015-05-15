var ui = require('ui'),
    fs = require('fs');
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
        menu: [
            {
                title: '&File', items: [
                    { title: '&Open', callback: openFile },
                    { title: '&Save As', callback: saveFileAs }
                ]
            },
            { title: '&Edit' },
            { title: '&Help', items: [
                    { title: 'Examples', items: [
                        { title: 'window.top', id: 'ex-win-top', callback: setExample },
                        { title: 'window.left', id: 'ex-win-left', callback: setExample },
                        { title: 'process.exit', id: 'ex-process-exit', callback: setExample }
                    ] }
                ]
            }
        ]
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

function setExample(id) {
    var code = '// hit F5 to run this example\n';
    switch (id) {
        case 'ex-win-top':
            code += 'app.window.top += 5;';
            break;
        case 'ex-win-left':
            code += 'app.window.left += 5;';
            break;
        case 'ex-process-exit':
            code += 'process.exit(0);';
            break;
        default:
            return;
    }
    app.window.postMessage({ cmd: 'set-text', text: code });
}

function openFile() {
    app.window.selectFile({
        open: true,
        title: 'Select a file',
        ext: ['txt', 'js', 'html', 'css', 'md', '*'],
        complete: function(files) {
            if (files && files[0]) {
                var content = fs.readFileSync(files[0], 'utf8');
                app.window.postMessage({ cmd: 'set-text', text: content });
            }
        }
    });
}

function saveFileAs() {
    app.window.selectFile({
        open: false,
        title: 'Select a file',
        ext: ['txt', '*'],
        complete: function(files) {
            if (files && files[0]) {
                app.window.postMessage({ cmd: 'get-text' }, function(content) {
                    fs.writeFileSync(files[0], content, 'utf8');
                });
            }
        }
    });
}
