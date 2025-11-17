import sounddevice as sd
import time

print(sd.query_devices())
print()
print("Finished, will close in 15 seconds")
time.sleep(15)