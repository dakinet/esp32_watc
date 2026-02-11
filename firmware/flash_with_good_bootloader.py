import subprocess
import sys

pio = r"C:\Users\Korisnik\AppData\Roaming\Python\Python310\Scripts\platformio.exe"
project = r"C:\Users\Korisnik\prog\ESP32S3\firmware"
good_bl_src = r"C:\Users\Korisnik\prog\ESP32S3\ESP32S3-NxxRxx-128I80T_开发板 V1.01\示例代码\FullFunctionTest\BIN\0x0target.bin"

# Step 1: Build only (no upload)
print("=== BUILDING ===")
result = subprocess.run(
    [pio, "run", "--project-dir", project],
    capture_output=True, text=True, timeout=300
)
print(result.stdout[-500:] if len(result.stdout) > 500 else result.stdout)
if result.returncode != 0:
    print("BUILD FAILED!")
    print(result.stderr[-500:] if len(result.stderr) > 500 else result.stderr)
    sys.exit(1)
print("=== BUILD OK ===\n")

# Step 2: Extract bootloader+partitions from working BIN
with open(good_bl_src, 'rb') as f:
    bl_data = f.read(0x10000)

bl_file = project + r"\good_bl_and_pt.bin"
with open(bl_file, 'wb') as f:
    f.write(bl_data)

our_app = project + r"\.pio\build\waveshare_s3_1_28\firmware.bin"

# Step 3: Flash good bootloader + our app
print("=== FLASHING ===")
result = subprocess.run([
    sys.executable, "-m", "esptool",
    "--chip", "esp32s3",
    "--port", "COM3",
    "--baud", "921600",
    "write_flash",
    "--flash_mode", "dio",
    "--flash_freq", "80m",
    "--flash_size", "16MB",
    "0x0", bl_file,
    "0x10000", our_app
], capture_output=True, text=True, timeout=120)

print(result.stdout)
if result.returncode != 0:
    print(result.stderr)
print("EXIT_CODE:", result.returncode)
sys.exit(result.returncode)
