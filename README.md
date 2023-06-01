
## Installation - Arduino // Software

The Arduino code runs on Arduino Mega (2560 Rev 3).

To run the solution in **/DobotCityBuilding_Arduino**, you'll need to install one or both of the following libraries :

* REQUIRED ! [DobotNet](https://github.com/MisTurtle/DobotNet)
* OPTIONAL ! [LiquidCrystal_I2C](https://github.com/johnrickman/LiquidCrystal_I2C)

**If you don't own an I2C LCD screen**, or don't wish to install the LiquidCrystal_I2C library, you can simply remove the references to the LCD screen in the *DobotCityBuilding_Arduino.ino file*


## Installation - Arduino // Hardware

Wire the devices to your Arduino board as follow :

#### > Storage Dobot - Left one on the demonstration video
1)  Connect the Dobot's **RX** and **TX** pins respectively on the Arduino's **TX1** on **RX1** pins. *It allows the sending of command packets to the Dobot through its UART interface*
2) Connect the Dobot's **EIO18** output pin to the Arduino's **D2** pin. *It allows the Arduino to keep track of a Dobot's working state*

#### > Building Dobot - Right one on the demonstration video
1)  Connect the Dobot's **RX** and **TX** pins respectively on the Arduino's **TX2** on **RX2** pins. *Same reason as Dobot 0*
2) Connect the Dobot's **EIO18** output pin to the Arduino's **D3** pin. *Same reason as Dobot 0*

#### > Bluetooth HC06
1) Connect the HC06 module's **TX** pin to the Arduino's **A10** pin
2) Follow [this scheme](http://www.blog.serveurduke.fr/index.php/2018/02/12/pilotage-par-bluetooth-hc-06/) to wire your HC06's **RX** pin to the Arduino's **A11** pin

#### I2C LCD screen
1) Connect the **SDA** and **SCL** pins of the microcontroller behind the screen to the **SDA** and **SCL** pins on the **Arduino's Communication area**

#
You're free to wire your **GND** and **VCC** lines whichever way you want. You can even power the Arduino board using the Dobots' **5V** / **12V** ouputs
#

## Installation - Python // Software

The Python code runs on a computer **connected to a camera with USB**

To run the solution in /DobotCityBuilding_Monitor, you'll need to install the following libraries (**ALL REQUIRED**)

* [ArucoCrop](https://github.com/MisTurtle/ArucoCrop_Library)
* OpenCV - `pip install opencv-contrib-python`
* NumPy - `pip install numpy`
* Pillow - `pip install Pillow`
* Tkinter - `pip install tk`

## Installation - Python // Hardware

1. Print the A3 pages of which you can find the layouts in [GlobalResources/MapsDesign/](https://github.com/MisTurtle/DobotCityBuilding/tree/main/GlobalResources/MapsDesign)

2. Connect the camera to your computed through USB

## Running the thing

Now that everything is wired up and being powered, you can upload the Arduino code onto the Mega board.

**CAREFUL !** The dobots will automatically home themselves once the Arduino is being powered

Once that's done, you can run the Python program (`main.py`). If everything works correctly, you should see a window with the camera feed on screen.

Click the *Next* button once you see the four Aruco markers being highlighted on the feed, and validate when you're happy with the contours you detect by tweaking the two sliders

The rest is pretty straightforward. On the interface that opens next, you'll see the camera feed and the detected blocks. You can use the various buttons to interact wirelessly with the Arduino board