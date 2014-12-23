document.addEventListener('DOMContentLoaded', function() {
    var plot = document.getElementById('plot');
    var c = plot.getContext('2d');
    var socket = new WebSocket("ws://" + location.host + "/asdf");
    var currentVolume = 0.;
    socket.onmessage = function(event) {
        currentVolume = JSON.parse(event.data);
    };
    function draw()
    {
        if(socket.readyState === WebSocket.OPEN)
            socket.send(JSON.stringify({'cmd': 'getoutvolume'}));
        c.clearRect(0, 0, c.width, c.height);
        c.fillStyle = 'red';
        c.fillRect(30, 30 * currentVolume, 50, 50);
        window.requestAnimationFrame(draw);
    }
    draw();
});
