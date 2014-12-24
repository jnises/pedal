document.addEventListener('DOMContentLoaded', function() {
    var plot = document.getElementById('plot');
    var c = plot.getContext('2d');
    var socket = new WebSocket("ws://" + location.host + "/commands");
    var currentVolume = 0.;
    var position = 0;
    socket.onmessage = function(event) {
        var data = JSON.parse(event.data);
        if(data.cmd === "outvolume") {
            currentVolume = data.args;
        }
    };
    function draw()
    {
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
});
