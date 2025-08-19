function send() {
  var message = {
    "sunriseMinuteSinceMidnight": 6.5 * 60,
    "sunsetMinuteSinceMidnight": (12 + 10) * 60,
  };
    
  // send the message to the watch
  Pebble.sendAppMessage(message,
    function(e) {
      console.log('JS message send success');
    },
    function(e) {
      console.log('JS message send error');
    }
  );
}

Pebble.addEventListener('ready', send);