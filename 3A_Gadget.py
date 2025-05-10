import cv2
import mediapipe as mp
import pyautogui
import serial
import serial.tools.list_ports
import keyboard
import threading

# Configuración global
smooth_factor = 0.2
amplify = 10
neutral_x, neutral_y = None, None
prev_x, prev_y = 0, 0
estado_actual = None
comando_pendiente = None
corriendo = True

# Detectar puerto serial del Wio Terminal
def detectar_puerto_wio():
    puertos = serial.tools.list_ports.comports()
    for puerto in puertos:
        descripcion = puerto.description.lower()
        if any(term in descripcion for term in ["wio", "seeeduino", "usb serial", "dispositivo"]):
            return puerto.device
    return None

# Seguimiento ocular y control del cursor
def procesar_seguimiento_ocular(face_mesh, screen_w, screen_h, cam):
    global neutral_x, neutral_y, prev_x, prev_y
    _, frame = cam.read()
    frame = cv2.flip(frame, 1)
    rgb_frame = cv2.cvtColor(frame, cv2.COLOR_BGR2RGB)
    output = face_mesh.process(rgb_frame)
    landmark_points = output.multi_face_landmarks

    if landmark_points:
        landmarks = landmark_points[0].landmark
        mid_x = (landmarks[475].x + landmarks[470].x) / 2
        mid_y = (landmarks[475].y + landmarks[470].y) / 2

        if neutral_x is None or neutral_y is None:
            neutral_x, neutral_y = mid_x, mid_y

        offset_x = mid_x - neutral_x
        offset_y = mid_y - neutral_y

        screen_x = screen_w / 2 + offset_x * screen_w * amplify
        screen_y = screen_h / 2 + offset_y * screen_h * amplify

        curr_x = prev_x + (screen_x - prev_x) * smooth_factor
        curr_y = prev_y + (screen_y - prev_y) * smooth_factor

        curr_x = max(5, min(screen_w - 5, curr_x))
        curr_y = max(5, min(screen_h - 5, curr_y))

        pyautogui.moveTo(curr_x, curr_y)
        prev_x, prev_y = curr_x, curr_y

# Procesar acciones desde el Wio Terminal
def procesar_comando_serial(linea, estado_actual, screen_w, screen_h):
    global neutral_x, neutral_y
    if linea == "CLICK DERECHO":
        if estado_actual != "right":
            pyautogui.mouseDown(button="right")
            return "right"
    elif linea == "CLICK DERECHO SOLTADO":
        pyautogui.mouseUp(button="right")
        return None
    elif linea == "CLICK IZQUIERDO PRESIONADO":
        if estado_actual != "left":
            pyautogui.mouseDown(button="left")
            return "left"
    elif linea == "CLICK IZQUIERDO SOLTADO":
        pyautogui.mouseUp(button="left")
        return None
    elif linea == "CENTRO":
        if estado_actual != "center":
            pyautogui.moveTo(screen_w // 2, screen_h // 2)
            neutral_x, neutral_y = None, None
            return "center"
    elif linea == "CENTRO SOLTADO":
        return None
    return estado_actual

# Leer el puerto serial y guardar comando pendiente
def leer_serial(ser):
    global comando_pendiente, corriendo
    while corriendo:
        try:
            linea = ser.readline().decode('utf-8').strip()
            if linea:
                comando_pendiente = linea
        except:
            break

# Función principal
def main():
    global corriendo, comando_pendiente, estado_actual

    puerto = detectar_puerto_wio()
    if not puerto:
        print("No se detectó el Wio Terminal.")
        return
    print(f"Puerto detectado: {puerto}")

    cam = cv2.VideoCapture(0)
    face_mesh = mp.solutions.face_mesh.FaceMesh(refine_landmarks=True)
    screen_w, screen_h = pyautogui.size()

    try:
        with serial.Serial(puerto, 115200, timeout=0.05) as ser:
            hilo_serial = threading.Thread(target=leer_serial, args=(ser,), daemon=True)
            hilo_serial.start()

            print("Presiona 'q' para salir.")
            while corriendo:
                procesar_seguimiento_ocular(face_mesh, screen_w, screen_h, cam)

                if comando_pendiente:
                    estado_actual = procesar_comando_serial(comando_pendiente, estado_actual, screen_w, screen_h)
                    comando_pendiente = None

                if keyboard.is_pressed('q'):
                    print("Finalizando...")
                    corriendo = False
                    break

    except Exception as e:
        print("Error:", e)
    finally:
        cam.release()
        cv2.destroyAllWindows()

if __name__ == "__main__":
    main()
