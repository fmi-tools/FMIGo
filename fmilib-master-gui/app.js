var express = require('express')
  , routes = require('./routes')
  , http = require('http')
  , config = require('config.json');

var app = express();

app.configure(function(){
  app.set('views', __dirname + '/views');
  app.set('view engine', 'ejs');
  app.use(express.favicon());
  app.use(express.logger('dev'));
  app.use(express.static(__dirname + '/public'));
  app.use(express.bodyParser());
  app.use(express.methodOverride());
  app.use(app.router);
});

app.configure('development', function(){
  app.use(express.errorHandler());
});

app.get('/',                                         routes.index);
app.get(/\/fmus\/(.*\.fmu)\/modelDescription\.xml$/, routes.modelDescription);
app.post('/simulate',                                routes.simulate);

http.createServer(app).listen(config.port);

console.log("Express server listening on port "+config.port);
