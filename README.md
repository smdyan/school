# School Schedule IoT — ESP32-C6 E-Paper Client + FastAPI Backend

A low-power e-paper IoT gadget that shows today’s school lesson schedule.  
Built on **ESP32-C6** (ESP-IDF) and using **deep sleep**, it reaches very low power consumption (~**200 µA**).

The client wakes up periodically, connects to Wi-Fi, fetches the schedule from the backend, renders it on the e-paper display, and goes back to sleep.

The backend is a **FastAPI** service with a **SQLite** database. It provides:
- **POST** endpoint(s) to import lesson schedules from a CSV file
- **GET** endpoint(s) to retrieve the schedule by **weekday number**

## Tech Stack
- **ESP-IDF** (ESP32-C6), Wi-Fi, deep sleep
- **E-paper display** + LVGL
- **FastAPI** backend
- **SQLite** storage
- **CSV** import