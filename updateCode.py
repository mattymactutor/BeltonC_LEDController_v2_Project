import os
import time

os.system("cd")
os.system("sudo rm -r BeltonC_LEDController_v2_Project")
os.system("git clone https://github.com/mattymactutor/BeltonC_LEDController_v2_Project.git")
time.sleep(2)
os.system("cd BeltonC_LEDController_v2_Project")
print("Building the make file with qmake...");
os.system("qmake -o Makefile BeltonC_LEDController_v2_Project.pro")
time.sleep(0.5);
print("Making new program...");
os.system("make")
time.sleep(0.5);
print("Running now...")
os.system("./BeltonC_LEDController_v2_Project")

