import secp256k1 as ice
import subprocess
import datetime
import os
import sys
import threading
import time
from itertools import cycle

if os.name == 'nt':
    os.system('cls')
else:
    os.system('clear')

sys.stdout.write(f"\033[01;33m")
sys.stdout.flush()

# Target Bitcoin address to search for
TARGET_ADDRESS = "1PfNh5fRcE9JKDmicD2Rh3pexGwce1LqyU"

# === Path Setup ===
BASE_DIR = os.path.dirname(os.path.abspath(__file__))
REPORTS_FOLDER = os.path.join(BASE_DIR, "reports")

# Ensure the reports folder exists
os.makedirs(REPORTS_FOLDER, exist_ok=True)

def log(message, log_file=None):
    """Log a message to both console and a log file."""
    global animation_active  

    # Pause the animation before logging
    animation_active = False
    time.sleep(0.4)  

    # Log the message
    timestamped_message = f"[{datetime.datetime.now().strftime('%Y-%m-%d %H:%M:%S')}] {message}"
    print(timestamped_message)
    if log_file:
        with open(log_file, "a") as file:
            file.write(timestamped_message + "\n")

    # Resume the animation after logging
    animation_active = True

# === Save Found Private Key ===
def save_found_key(hex_key, dec_key, addc, addu, log_file):
    """Save the found private key details to a file."""
    FOUND_KEY_FILE = "found_key.txt"
    file_path = os.path.join(REPORTS_FOLDER, FOUND_KEY_FILE)
    with open(file_path, "w") as file:
        file.write("Found matching private key!\n")
        file.write(f"Compressed Bitcoin Address: {addc}\n")
        file.write(f"Uncompressed Bitcoin Address: {addu}\n")
        file.write(f"Private key (hex): {hex_key}\n")
        file.write(f"Private key (decimal): {dec_key}\n")
    log(f"[I] Private key saved to {FOUND_KEY_FILE}")

# === Animation Function ===
def animate():
    """Display a spinning animation in the console."""
    spinner = cycle(['⢎⡰', '⢎⡡', '⢎⡑', '⢎⠱', '⠎⡱', '⢊⡱', '⢌⡱', '⢆⡱']) 
    try:
        while not stop_animation:
            if animation_active:  
                sys.stdout.write(f"\rSpinning the wheel... {next(spinner)}\r")
                sys.stdout.flush()
                time.sleep(0.1)  
            else:
                time.sleep(0.1)  
    except KeyboardInterrupt:
        pass

# === Main Script ===
try:
    global stop_animation, animation_active
    stop_animation = False 
    animation_active = True  

    # Start the animation in a separate thread
    animation_thread = threading.Thread(target=animate)
    animation_thread.daemon = True
    animation_thread.start()

    while True:
        # Unique log file for this run
        timestamp = datetime.datetime.now().strftime("%Y%m%d_%H%M%S")
        log_file = os.path.join(REPORTS_FOLDER, f"log_{timestamp}.txt")
        process = subprocess.Popen(
            ["./WIFRoulette"],
            stdout=subprocess.PIPE,
            stderr=subprocess.PIPE,
            bufsize=1,  # Line-buffered
            universal_newlines=True  # Decode output as text
        )

        found = False
        for line in process.stdout:
            line = line.strip()

            # Skip blank lines and speed info
            if not line or line.startswith("[I] Speed ="):
                continue

            log(line, log_file)

            if line.startswith("[W] "):
                wif_key = line[4:]
                try:
                    hex_key = ice.btc_wif_to_pvk_hex(wif_key)
                    dec_key = int(hex_key, 16)
                    addc = ice.privatekey_to_address(0, True, dec_key)
                    addu = ice.privatekey_to_address(0, False, dec_key)
                    log(f"[I] {addc}", log_file)
                    log(f"[I] {addu}", log_file)
                    if addc.startswith('1Pf') or addu.startswith('1Pf'):
                        log(f"[I] matched {addc}", log_file)
                        log(f"[I] matched {addu}", log_file)
                    if addc == TARGET_ADDRESS or addu == TARGET_ADDRESS:
                        log("[I] PRIVATE KEY FOUND!!!", log_file)
                        log(f"Compressed Bitcoin Address: {addc}", log_file)
                        log(f"Uncompressed Bitcoin Address: {addu}", log_file)
                        log(f"Private key (hex): {hex_key}", log_file)
                        log(f"Private key (decimal): {dec_key}", log_file)

                        save_found_key(hex_key, dec_key, addc, addu, log_file)

                        process.terminate()
                        found = True
                        break
                except Exception as key_error:
                    log(f"[E] Failed to process WIF key: {key_error}")

            elif line.startswith("[E] "):
                log(line)

        if found:
            stop_animation = True  
            sys.stdout.write("\rPRIVATE KEY FOUND!     \n") 
            break

        process.wait()

        if process.returncode != 0:
            log(f"[E] Wrong return code: {process.returncode}")

except KeyboardInterrupt:
    stop_animation = True  
    log("[I] Script stopped by user")

except Exception as e:
    stop_animation = True  
    log(f"[E] Unexpected error: {str(e)}")

finally:
    stop_animation = True  
    log("[I] Script execution ended")