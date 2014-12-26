document.addEventListener('DOMContentLoaded', function() {
    var plot = document.getElementById('plot');
    var c = plot.getContext('2d');
    var socket = new WebSocket("ws://" + location.host + "/commands");
    var currentVolume = 0.;
    var position = 0;
    var dynamicparameterdeferreds = {};
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
        } else if(data.cmd == "dynamicparameters") {
            var id = data.args.id;
            var deferred = dynamicparameterdeferreds[id];
            if(deferred === undefined) {
                console.log('unable to find dynamic parameter deferred with id %s', id);
                return;
            }
            delete dynamicparameterdeferreds[id];
            deferred.deferred.resolve(data.args);
        }
    };
    function draw() {
        if(socket.readyState === WebSocket.OPEN)
            socket.send(JSON.stringify({'cmd': 'getoutvolume'}));
        c.clearRect(position, 0, 1, plot.height);
        c.fillStyle = 'black';
        var lineHeight = Math.min(currentVolume, 1) * plot.height;
        c.fillRect(position, plot.height - lineHeight, 1, lineHeight);
        position = (position + 1) % plot.width;
        window.requestAnimationFrame(draw);
    }
    draw();
    function pruneDynamicParametersDeferreds() {
        var now = Date.now();
        dynamicparameterdeferreds = _.filter(dynamicparameterdeferreds, function(value, key) {
            var keep = (now - value.time) < messageReplyTimeout;
            if(!keep)
                console.log('removing old message with id %s', key);
            return keep;
        });
        window.setTimeout(pruneDynamicParametersDeferreds, 1000);
    }
    window.setTimeout(pruneDynamicParametersDeferreds, 1000);
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
