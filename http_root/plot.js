document.addEventListener('DOMContentLoaded', function() {
    var volumeplot = document.getElementById('volumeplot');
    var volumectx = volumeplot.getContext('2d');
    var hilowplot = document.getElementById('hilowplot');
    var hilowctx = hilowplot.getContext('2d');
    var bufferplot = document.getElementById('bufferplot');
    var bufferctx = bufferplot.getContext('2d');
    var socket = new WebSocket("ws://" + location.host + "/commands");
    var currentVolume = 0.;
    var currentHi = 0.;
    var currentLow = 0.;
    var volumeposition = 0;
    var hilowposition = 0;
    var dynamicparameterdeferreds = {};
    var latestbufferdeferreds = {};
    var latestbuffer = [];
    var nextMessageId = 0;
    var messageReplyTimeout = 10000;
    function getMessageId() {
        var messageIdString = '' + nextMessageId;
        nextMessageId++;
        return messageIdString;
    }
    socket.onmessage = function(event) {
        var data = JSON.parse(event.data);
        if(data.cmd === "outvolume") {
            currentVolume = data.args;
        } else if(data.cmd === "dynamicparameters") {
            var id = data.args.id;
            var deferred = dynamicparameterdeferreds[id];
            if(deferred === undefined) {
                console.log('unable to find dynamic parameter deferred with id %s', id);
                return;
            }
            delete dynamicparameterdeferreds[id];
            deferred.deferred.resolve(data.args);
        } else if(data.cmd === "outhilow") {
            currentHi = data.args[0];
            currentLow = data.args[1];
        } else if(data.cmd === "latestbuffer") {
            var id = data.args.id;
            var deferred = latestbufferdeferreds[id];
            if(deferred === undefined) {
                console.log('unable to find dynamic parameter deferred with id %s', id);
                return;
            }
            delete latestbufferdeferreds[id];
            deferred.deferred.resolve(data.args);
        } else {
            console.log('unknown message %s received', data.cmd);
        }        
    };
    function getLatestBuffer() {
        var messageid = getMessageId();
        var deferred = Q.defer();
        latestbufferdeferreds[messageid] = {'time': Date.now(),
                                            'deferred': deferred};
        socket.send(JSON.stringify({'cmd': 'getlatestbuffer',
                                    'args': {'id': messageid}}));
        deferred.promise.then(function(args) {
            latestbuffer = args.buffer;
        });
    }
    function draw() {
        if(socket.readyState === WebSocket.OPEN) {
            socket.send(JSON.stringify({'cmd': 'getoutvolume'}));
            socket.send(JSON.stringify({'cmd': 'getouthilow'}));
            getLatestBuffer();
        }
        volumectx.clearRect(volumeposition, 0, 1, volumeplot.height);
        volumectx.fillStyle = 'black';
        var lineHeight = Math.min(currentVolume, 1) * volumeplot.height;
        volumectx.fillRect(volumeposition, volumeplot.height - lineHeight, 1, lineHeight);
        volumeposition = (volumeposition + 1) % volumeplot.width;

        hilowctx.clearRect(hilowposition, 0, 1, hilowplot.height);
        hilowctx.fillStyle = 'black';
        var clampedHi = Math.min(1., Math.max(-1., currentHi));
        var clampedLow = Math.min(1., Math.max(-1., currentLow));
        hilowctx.fillRect(hilowposition, (1. - clampedHi) * hilowplot.height / 2, 1, Math.max(1., (clampedHi - clampedLow) * hilowplot.height / 2));
        hilowctx.fillStyle = 'red';
        hilowctx.fillRect(hilowposition, hilowplot.height / 2, 1, 1);
        hilowposition = (hilowposition + 1) % hilowplot.width;

        bufferctx.clearRect(0, 0, bufferplot.width, bufferplot.height);
        if(latestbuffer !== undefined) {
            bufferctx.fillStyle = '#000000';
            _.each(latestbuffer, function(value, index) {
                bufferctx.fillRect(index / latestbuffer.length * bufferplot.width, (1. + Math.min(1., Math.max(-1., value))) * bufferplot.height / 2, bufferplot.width / latestbuffer.length, 1);
            });
        }

        window.requestAnimationFrame(draw);
    }
    draw();
    function pruneDeferreds() {
        var now = Date.now();
        // _.filter doesn't seem to behave
        _.each(_.keys(dynamicparameterdeferreds), function(key) {
            var value = dynamicparameterdeferreds[key];
            var keep = (now - value.time) < messageReplyTimeout;
            if(!keep)
            {
                console.log('removing old message with id %s', key);
                delete dynamicparamterdeferreds[key];
            }
        });
        _.each(_.keys(latestbufferdeferreds), function(key) {
            var value = latestbufferdeferreds[key];
            var keep = (now - value.time) < messageReplyTimeout;
            if(!keep)
            {
                console.log('removing old message with id %s', key);
                delete latestbufferdeferreds[key];
            }
        });
        window.setTimeout(pruneDeferreds, 1000);
    }
    window.setTimeout(pruneDeferreds, 1000);
    function getDynamicParameters() {
        var messageid = getMessageId();
        var deferred = Q.defer();
        dynamicparameterdeferreds[messageid] = {time: Date.now(),
                                                'deferred': deferred};
        socket.send(JSON.stringify({'cmd': 'getdynamicparameters',
                                    'args': {'id': messageid}}));
        deferred.promise.then(function(args) {
            var params = args.params;
            if(params !== undefined)
                document.getElementById('stuff').innerHTML = params.join(' ');
        });
    }
    socket.onopen = function() {
        getDynamicParameters();
    };
});
