#! /bin/sh

md components
cd components
git clone --recursive --branch idf-release/v4.4 https://github.com/espressif/arduino-esp32.git arduino

# the commands below are intended to the S2 HMI devkit board that has PSRAM
cp ../CMakeLists-ToBeUsedAsArduinoComponent.txt arduino
cd ..
cp sdkconfig.esp32s2 sdkconfig
