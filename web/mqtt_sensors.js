var mqtt_topic = "homebridge/to/set";
const options = {
  // Authentication details
  username: 'pi',
  password: 'password',
  clientID: 'aqualinkd.mqtt.sensors',
  port: 9001
};

var client = mqtt.connect("ws://", options);

client.on("connect", () => {
  client.subscribe(mqtt_topic, (err) => {
  });
});

function formatTwoDecimalsOrInteger(num) {
  const roundedNum = Math.round(num * 100) / 100;
  let result = String(roundedNum);

  // Check if the string ends with '.00' and remove it if present
  if (result.endsWith('.00')) {
    result = result.slice(0, -3); // Remove the last 3 characters (".00")
  }
    
  return result;
}

client.on("message", (topic, message) => {
  const jsonObj = JSON.parse(message.toString());
  if (jsonObj.name == 'Acid Tank Level') {
    if ((tile = document.getElementById("lightbulb.acid_tank_level")) == null) {
      create_mytitle("lightbulb.acid_tank_level", "Acid Tank Level", "value", "%");
    }
    if (jsonObj.characteristic == 'On') {
      setTileOn("lightbulb.acid_tank_level", jsonObj.value == true ? 'on' : 'off', null);
      setTileAttribute("lightbulb.acid_tank_level", "last", jsonObj.value == true ? 'on' : 'off');
    } else if (jsonObj.characteristic == "Brightness") {
      setTileValue("lightbulb.acid_tank_level", formatTwoDecimalsOrInteger(jsonObj.value));
    }
  } else if (jsonObj.name == 'pH Alarm') {
    if ((tile = document.getElementById("switch.ph_alarm")) == null) {
      create_mytitle("switch.ph_alarm", "pH Alarm", "switch", "");
    }
    if (jsonObj.characteristic == 'On') {
      setTileOn("switch.ph_alarm", jsonObj.value == true ? 'on' : 'off', null);
      setTileAttribute("switch.ph_alarm", "last", jsonObj.value == true ? 'on' : 'off');
    }
  } else if (jsonObj.name == 'ORP Alarm') {
    if ((tile = document.getElementById("switch.orp_alarm")) == null) {
      create_mytitle("switch.orp_alarm", "ORP Alarm", "switch", "");
    }
    if (jsonObj.characteristic == 'On') {
      setTileOn("switch.orp_alarm", jsonObj.value == true ? 'on' : 'off', null);
      setTileAttribute("switch.orp_alarm", "last", jsonObj.value == true ? 'on' : 'off');
    }
  }
});

client.on("error", (err) => {
    console.error('Connection error:', err);
});

function create_mytitle(tile_id, tile_name, title_type, uom = "") {
  if ((tile = document.getElementById(tile_id)) == null) {
    var tile = {};
    tile["id"] = tile_id;
    tile["name"] = tile_name;
    tile["display"] = "true";
    tile["type"] = title_type; // switch or value
    tile["state"] = 'off';
    if (title_type == "value") {
      tile['value'] = "0.0";
    }
    if (uom != "") {
      tile['uom'] = uom;
    }
    tile["status"] = tile["state"]; // status and state are different for AqualinkD, but for purposes of a switch or sensor they are the same.
    tile["last"] = 'off';

    // Call AqualinkD function to create the tile and add to display.
    createTile(tile);

    // Make sure we use our own callback for button press. (only needed for a switch)
    var subdiv = document.getElementById(tile["id"]);
    subdiv.setAttribute('onclick', "mqttTilePressedCallback('" + tile["id"] + "')");
  }
}

function mqttTilePressedCallback(tile_id) {
  // These tile state can not be changed
  if (getTileAttribute(tile_id, 'last') == 'off') {
    setTileOn(tile_id, 'off', null);
  } else {
    setTileOn(tile_id, 'on', null);
  }
}

