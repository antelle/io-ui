(function() {
    'use strict';

    var codeExecResult = '';

    backend.onMessage = function(msg) {
        if (msg.code) {
            codeExecResult += eval(msg.code);
            return { result: codeExecResult };
        }
        return { error: true };
    }
})();