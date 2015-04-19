var ps = require('child_process');

module.exports = function run(cmd, args, success) {
    console.log(cmd + ' ' + args.join(' '));
    var p = ps.spawn(cmd, args, { stdio: 'inherit' }).on('close', function (code) {
        if (code) {
            console.error('Command finished with error ' + code);
            process.exit(code);
        }
        if (success) {
            success();
        }
    });
};
