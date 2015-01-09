var vertx = require('vertx');
var server = vertx.createHttpServer();

server.requestHandler(function(req) {
  var file = '';
  if (req.path() == '/') {
    file = 'index.html';
  } else if (req.path().indexOf('..') == -1) {
    file = req.path();
  }
  req.response.sendFile('/var/www/www.urbin.info/' + file);   
}).listen(8000, '127.0.0.1');

