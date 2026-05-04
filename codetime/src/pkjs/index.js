
// ============================================================================
// CONSTANTS
// ============================================================================
var KEY_TEMP        = 0;
var KEY_DESC        = 1;
var KEY_MILITARY    = 2;
var KEY_FAHRENHEIT  = 3;

var TEMP_INVALID = 999;
var WEATHER_COOLDOWN = 10 * 60 * 1000;

var lastWeatherTime = 0;

// ============================================================================
// CONFIG HTML
// ============================================================================
var CONFIG_HTML = `
<!DOCTYPE html>
<html>
<head>
<meta name="viewport" content="width=device-width,initial-scale=1">
<style>
body {
  background:#000;
  color:#0f0;
  font-family:monospace;
  padding:20px;
  margin:0;
}
h2 { color:#0ff; }
label { display:block; margin:10px 0; }
button {
  margin-top:20px;
  padding:12px;
  background:#030;
  color:#0f0;
  border:1px solid #0f0;
  width:100%;
}
</style>
</head>
<body>

<h2>// C++ Config</h2>

<p>int timeFormat =</p>
<label><input type="radio" name="fmt" value="0"> standard (12h)</label>
<label><input type="radio" name="fmt" value="1"> military (24h)</label>

<p>int tempUnit =</p>
<label><input type="radio" name="unit" value="0"> celsius</label>
<label><input type="radio" name="unit" value="1"> fahrenheit</label>

<button onclick="save()">save();</button>

<script>
function save() {
  var cfg = {
    military: parseInt(document.querySelector("input[name=fmt]:checked").value || 0),
    fahrenheit: parseInt(document.querySelector("input[name=unit]:checked").value || 0)
  };

  // ✅ THIS IS THE CRITICAL FIX FOR PEBBLE
  var encoded = encodeURIComponent(JSON.stringify(cfg));
  window.location.href = "pebblejs://close#" + encoded;
}
</script>

</body>
</html>
`;

// ============================================================================
// SAFE MESSAGE SENDING
// ============================================================================
function sendMessage(payload, retries) {
  retries = retries || 0;

  Pebble.sendAppMessage(
    payload,
    function () {
      console.log("Send OK:", payload);
    },
    function (e) {
      console.log("Send FAIL:", e);

      if (retries < 3) {
        setTimeout(function () {
          sendMessage(payload, retries + 1);
        }, 1000 * (retries + 1));
      }
    }
  );
}

// ============================================================================
// WEATHER
// ============================================================================
function wmoToDesc(code) {
  var map = {
    0:'Clear', 1:'Mostly Clear', 2:'Partly Cloudy', 3:'Overcast',
    45:'Foggy', 48:'Icy Fog',
    51:'Lt Drizzle', 53:'Drizzle', 55:'Hvy Drizzle',
    61:'Lt Rain', 63:'Rain', 65:'Hvy Rain',
    71:'Lt Snow', 73:'Snow', 75:'Hvy Snow', 77:'Snow Grains',
    80:'Showers', 81:'Showers', 82:'Hvy Showers',
    85:'Snow Showers', 86:'Hvy Snow Showers',
    95:'Thunderstorm', 96:'T-storm+Hail', 99:'T-storm+Hail'
  };
  return map[code] || "Unknown";
}

function fetchWeather(lat, lon) {
  var now = Date.now();
  if (now - lastWeatherTime < WEATHER_COOLDOWN) return;

  lastWeatherTime = now;

  var url =
    "https://api.open-meteo.com/v1/forecast" +
    "?latitude=" + lat +
    "&longitude=" + lon +
    "&current_weather=true&temperature_unit=celsius";

  var xhr = new XMLHttpRequest();
  xhr.timeout = 15000;

  xhr.onload = function () {
    if (xhr.status !== 200) {
      sendMessage({ [KEY_TEMP]: TEMP_INVALID, [KEY_DESC]: "HTTP Err" });
      return;
    }

    try {
      var data = JSON.parse(xhr.responseText);

      if (!data.current_weather) throw "missing";

      sendMessage({
        [KEY_TEMP]: Math.round(data.current_weather.temperature),
        [KEY_DESC]: wmoToDesc(data.current_weather.weathercode)
      });

    } catch (e) {
      sendMessage({ [KEY_TEMP]: TEMP_INVALID, [KEY_DESC]: "Parse Err" });
    }
  };

  xhr.onerror = function () {
    sendMessage({ [KEY_TEMP]: TEMP_INVALID, [KEY_DESC]: "No Net" });
  };

  xhr.ontimeout = function () {
    sendMessage({ [KEY_TEMP]: TEMP_INVALID, [KEY_DESC]: "Timeout" });
  };

  xhr.open("GET", url);
  xhr.send();
}

function requestLocation() {
  navigator.geolocation.getCurrentPosition(
    function (pos) {
      fetchWeather(pos.coords.latitude, pos.coords.longitude);
    },
    function () {
      sendMessage({ [KEY_TEMP]: TEMP_INVALID, [KEY_DESC]: "No GPS" });
    },
    { timeout: 15000, maximumAge: 300000 }
  );
}

// ============================================================================
// CONFIG STORAGE
// ============================================================================
function loadConfig() {
  try {
    return JSON.parse(localStorage.getItem("cfg") || "{}");
  } catch (e) {
    return {};
  }
}

function sendConfigToWatch() {
  var cfg = loadConfig();

  sendMessage({
    [KEY_MILITARY]: cfg.military ? 1 : 0,
    [KEY_FAHRENHEIT]: cfg.fahrenheit ? 1 : 0
  });
}

// ============================================================================
// EVENTS
// ============================================================================
Pebble.addEventListener("ready", function () {
  console.log("JS ready");

  sendConfigToWatch();
  requestLocation();
});

Pebble.addEventListener("appmessage", function () {
  requestLocation();
});

Pebble.addEventListener("showConfiguration", function () {
  var url =
    "data:text/html;charset=utf-8," +
    encodeURIComponent(CONFIG_HTML);

  Pebble.openURL(url);
});

Pebble.addEventListener("webviewclosed", function (e) {
  if (!e || !e.response || e.response === "CANCELLED") return;

  try {
    var config = JSON.parse(decodeURIComponent(e.response));

    localStorage.setItem("cfg", JSON.stringify(config));

    sendMessage({
      [KEY_MILITARY]: config.military ? 1 : 0,
      [KEY_FAHRENHEIT]: config.fahrenheit ? 1 : 0
    });

  } catch (err) {
    console.log("Config error:", err);
  }
});