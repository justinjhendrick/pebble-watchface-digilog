var ENABLE_CACHE = true;

function sendToWatch(d) {
  Pebble.sendAppMessage(
    d,
    function() {},
    function(e) { console.log("Error sending to Pebble: " + e.error.message); }
  );
}

function millis_from_secs(v) {
  return v * 1000;
}

function millis_from_mins(v) {
  return millis_from_secs(v * 60);
}

function millis_from_hours(v) {
  return millis_from_mins(v * 60);
}

function handleAbort(e) {
  console.log("GET Abort. " + JSON.stringify(e));
}

function handleError(e) {
  console.log("GET Error. " + JSON.stringify(e));
}

function handleTimeout(e) {
  console.log("GET Timeout. " + JSON.stringify(e));
}

function getRequest(url, onload) {
  var xhr = new XMLHttpRequest();
  xhr.addEventListener("load", function() { onload(this) });
  xhr.addEventListener("abort", handleAbort);
  xhr.addEventListener("error", handleError);
  xhr.addEventListener("timeout", handleTimeout);
  xhr.open("GET", url);
  xhr.timeout = millis_from_secs(15);
  xhr.setRequestHeader("User-Agent", "https://github.com/justinjhendrick/pebble-watchface-digilog");
  xhr.send();
}

// These INVALIDS must match watch side definition
var INVALID_TIME = 0;

var location_cache = {
  lat: null,
  lon: null,
}

var sun_cache = {
  time: 0,
  rise: INVALID_TIME,
  set: INVALID_TIME,
}

function getSun() {
  var now = Date.now();
  var sun_cache_str = localStorage.getItem("sun_cache");
  if (sun_cache_str != null) {
    sun_cache = JSON.parse(sun_cache_str);
  }
  if (ENABLE_CACHE && now <= sun_cache.time + millis_from_hours(24)) {
    console.log("resending cached sun");
    sendToWatch(
      {
        "sunrise": sun_cache.rise,
        "sunset": sun_cache.set,
      }
    );
    return;
  }
  if (location_cache.lat == null || location_cache.lon == null) {
    console.log("cannot get sunrise/sunset if we don't know where");
    return;
  }
  var url =
    "https://api.met.no/weatherapi/sunrise/3.0/sun"
    + "?lat=" + location_cache.lat
    + "&lon=" + location_cache.lon;
  console.log("Fetching sun from " + url);
  getRequest(url, function(response) {
    if (response.status < 200 || response.status >= 300) {
      console.log("Error code from sun " + response.status);
      return;
    }
    var json = JSON.parse(response.responseText);
    sun_cache.time = now;
    sun_cache.rise = Math.round(new Date(json.properties.sunrise.time).valueOf() / 1000);
    sun_cache.set = Math.round(new Date(json.properties.sunset.time).valueOf() / 1000);
    localStorage.setItem("sun_cache", JSON.stringify(sun_cache))

    sendToWatch(
      {
        "sunrise": sun_cache.rise,
        "sunset": sun_cache.set,
      }
    );
  });
}

function locationSuccess(pos) {
  location_cache.lat = pos.coords.latitude.toFixed(1)
  location_cache.lon = pos.coords.longitude.toFixed(1)
  localStorage.setItem("location_cache_v2", JSON.stringify(location_cache))
  getSun();
}

function locationError(err) {
  console.log("Error requesting location: " + JSON.stringify(err));
  if (location_cache.lat == null || location_cache.lon == null) {
    var location_cache_str = localStorage.getItem("location_cache_v2")
    if (location_cache_str != null) {
      location_cache = JSON.parse(location_cache_str);
    }
  }
  getSun();
}

function getLocation() {
  navigator.geolocation.getCurrentPosition(
    locationSuccess,
    locationError,
    {
      timeout: millis_from_secs(15),
      maximumAge: millis_from_hours(24),
      enableHighAccuracy: false
    }
  );
}

Pebble.addEventListener("ready", function(e) {
  getLocation();
});

Pebble.addEventListener("appmessage", function(e) {
  // sending an empty message from watch to phone
  // is interpreted as "give me the current weather"
  getLocation();
});
