import subprocess
import sys

pio = r"C:\Users\Korisnik\AppData\Roaming\Python\Python310\Scripts\platformio.exe"
project = r"C:\Users\Korisnik\prog\ESP32S3\firmware"

result = subprocess.run(
    [pio, "run", "--target", "upload", "--project-dir", project],
    capture_output=True, text=True, timeout=300
)
print(result.stdout)
print(result.stderr)
print("EXIT_CODE:", result.returncode)
sys.exit(result.returncode)
