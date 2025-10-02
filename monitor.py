import serial
import time

try:
    # Открываем COM13 порт
    ser = serial.Serial('COM13', 115200, timeout=1)
    print(f"Монитор порта открыт: {ser.name}")
    print("Нажмите Ctrl+C для выхода")
    print("-" * 50)
    
    while True:
        if ser.in_waiting > 0:
            line = ser.readline().decode('utf-8', errors='ignore').strip()
            if line:
                print(line)
        time.sleep(0.1)
        
except serial.SerialException as e:
    print(f"Ошибка открытия порта: {e}")
    print("Проверьте:")
    print("1. Подключен ли ESP32 к COM13")
    print("2. Не занят ли порт другим приложением")
    print("3. Правильно ли указан номер порта")
except KeyboardInterrupt:
    print("\nМонитор остановлен")
finally:
    if 'ser' in locals() and ser.is_open:
        ser.close()
        print("Порт закрыт")

