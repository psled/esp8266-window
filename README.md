# ESP8266 Window

An Arduino code for ESP8266 module which trasform your standard window shade into wireless electric one.

# Why

Because I always wanted to have an electric window shade. However, they used to be quite expensive for my budget so I decided to build my own. Plus, I really wanted to find a practical application for an ESP8266 module which was lying around for some time. It's nice to see what this tiny wireless module can actually do.

# Requirements

- ESP8266 module (obviously, I used first generation ESP-01)
- Continuous rotation servo (I used [PowerHD AR-3603HB](https://www.amazon.co.uk/Power-HD-AR-3603HB-Speed-Robot/dp/B00CBTXQVI))
- Two limit switches with rollers (for detecting open / close position)

Except above main components we will need as usual: a breadboard, couple wires and resistors and some power supply (keep in mind that ESP8266 works on 3.3V and my servo for example works on 5V).

Last but not least, we need also a spare time, cause even if software and electronic parts are pretty straightforward, a physical mounting a servo on the shade could be quite tricky actually. Maybe 3D printer could help here.

# Installation

First, download and install [ESP8266 Arduino](https://github.com/esp8266/Arduino) if you haven't done this before.

Then connect everything electronically and prepare the valid mounting for both servo and limit switches.

In my servo I have replaced the potentiometer with two 2k Ohm resistors and in this particular unit and this configuration the middle point seems to be around 85 degrees (not 90 like it ideally should be). Depending on servo you use either calibrate the pot accordingly or change above values in code.

Finally, upload code from this repository to your ESP8266 module.

You could probably start using your brand new electric window shade now although controlling it directly through HTTP requests could be quirky. Because of that you can check out the [Homebridge plugin](https://github.com/pawelsledzikowski/homebridge-esp8266-window). It was created to allows you to integrate your ESP8266 module with Apple HomeKit platform and Siri.

# Usage

- Current position
```
GET /window/currentPosition HTTP/1.1
```

- Position state
```
GET /window/positionState HTTP/1.1
```

- Target position
```
POST /window/targetPosition HTTP/1.1
Content-Type: application/json

{
  "value": 78
}
```

# TODO

- Fix calculating open / close percentage. At the moment we are using a single timer for both opening / closing shade but this speed can vary. Moreover, that kind of method seems to be quite unreliable in long-term so finding something better would be appreciated.

# License

ISC