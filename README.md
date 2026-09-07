 Clocky

A customizable Wi-Fi desk clock built for the ESP32 CYD (Cheap Yellow Display).

Clocky combines a touchscreen clock, countdowns, Wi-Fi connectivity, and a built-in web interface into a customizable desk display.

> 🚧 Clocky is currently under active development.

---

## Features

- 🕒 Large touchscreen clock display
- 📅 Date and time
- ⏳ Custom countdowns
- 📶 Wi-Fi connectivity
- 🌐 Built-in web configuration interface
- 🎨 Customizable appearance and layout
- 📱 Configure Clocky from another device on your network
- 💾 Saved settings
- 🔧 Designed for the ESP32 CYD touchscreen

More features are planned as development continues.

---

## Hardware

Clocky is designed for the **ESP32-2432S028R**, commonly known as the **Cheap Yellow Display (CYD)**.

### Required

- ESP32 CYD touchscreen display
- USB-C cable and power source
- Wi-Fi network

### Optional

- 3D-printed Clocky enclosure
- Desk stand

---

## Installation

### 1. Clone the repository

```bash
git clone https://github.com/h8bbphgjhd-dotcom/Clocky.git
cd Clocky
````

### 2. Open the project

Clocky is built using PlatformIO.

Open the project folder in:

* VS Code with the PlatformIO extension, or
* another PlatformIO-compatible environment.

### 3. Build and upload

Connect the CYD to your computer using USB.

Then build and upload the firmware:

```bash
pio run --target upload
```

---

## Wi-Fi Setup

When Clocky is not connected to Wi-Fi, it can enter setup mode.

Connect to the Clocky Wi-Fi setup network and configure your Wi-Fi credentials.

Once configured, Clocky will connect to your network and make its web interface available.

---

## Web Interface

Clocky includes a built-in web server.

Use the web interface to configure features such as:

* Wi-Fi settings
* Clock settings
* Countdown timers
* Display preferences
* Colors and layout

---

## Project Structure

```text
Clocky/
├── data/        Web interface files
├── include/     Header files
├── src/         Firmware source code
├── platformio.ini
└── README.md
```

---

## 3D Printed Case

A 3D-printable enclosure is being developed for Clocky.

The case is designed to:

* Hold the CYD securely
* Provide access to ports
* Support the display at a comfortable desk viewing angle
* Be easy to print on consumer FDM printers

---

## Development

Clocky is an evolving project.

Future ideas include:

* 🌤️ Weather information
* 🌙 Automatic night mode
* 🔆 Adjustable brightness
* 🎨 Additional themes
* 🖥️ More display layouts
* 🔔 Reminders and events

Suggestions and contributions are welcome.

---

## Contributing

If you'd like to contribute:

1. Fork the repository.
2. Create a new branch.
3. Make your changes.
4. Submit a pull request.

---

## License

This project is licensed under the MIT License.

---

## About

Clocky is a DIY smart desk clock project built around the ESP32 CYD touchscreen.

The goal is to create a simple, customizable, and useful desk display that can be built and modified by anyone.

```firmware behaves.
```
