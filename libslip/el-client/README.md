ELClient
========
This is a Wifi library for arduino that uses the SLIP protocol to communicate via serial with
an ESP8266 module running the [esp-link firmware](https://github.com/jeelabs/esp-link).

This library requires esp-link v2.2.beta2 or later.

Features
========
- C++ classes to manage the communication with esp-link
- Support outbound REST requests
- Support MQTT pub/sub
- Support additional commands to query esp-link about wifi and such

- MQTT functionality: 
    + MQTT protocol itself implemented by esp-link
    + Support subscribing, publishing, LWT, keep alive pings and all QoS levels 0&1

- REST functionality:
    + Support methods GET, POST, PUT, DELETE
    + setContent type, set header, set User Agent

- UDP socket functionality:
    + Support sending and receiving UDP socket packets and broadcasting UDP socket packets

- TCP socket functionality:
    + Support TCP socket clients to send packets to a TCP server
    + Support TCP socket server to receive packets from TCP socket clients and send back responses

Examples
========
Currently two examples are provided that are known to work and that come with HEX files ready
to load into an Atmega 328 based arduino:
- A REST example that fetches the current time from a server on the internet and prints it.
  This example is in `./ELClient/examples/rest`.
- An MQTT example that publishes messages to an MQTT server and subscribes so it receives and
  prints its own messages. This example is in `./ELClient/examples/mqtt`.
- An UDP socket client example to connect to a UDP socket server or broadcast to a local network. This example is in `./ELClient/examples/udp`.
- A simple TCP socket client example that sends data to a TCP server without waiting for response. This example is in `./ELClient/examples/tcp-client`.
- A TCP socket client example that sends data to a TCP server and waits for a response. This example is in `./ELClient/examples/tcp-client_resp`.
- A TCP socket server example that waits for connections from a TCP socket client. This example is in `./ELClient/examples/tcp-server`.
- A Thingspeak example to use REST POST to send data to thingspeak. This example is in `./ELClient/examples/udp`.

The "demo" example are currently not maintained and therefore won't work as-is.

API documentation
========
A prelimenary documentation for the library is available on [ELClient API Doc](http://desire.giesecke.tk/docs/el-client/).