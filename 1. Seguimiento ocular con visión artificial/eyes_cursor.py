import cv2
import numpy as np
import mediapipe as mp
import pyautogui

# Inicializar FaceMesh
mp_face_mesh = mp.solutions.face_mesh
face_mesh = mp_face_mesh.FaceMesh(max_num_faces=1, min_detection_confidence=0.7, min_tracking_confidence=0.7)

# Tamaño de la pantalla
screen_width, screen_height = pyautogui.size()

# Captura de cámara
cap = cv2.VideoCapture(0)

# Landmarks del ojo derecho
eye_right_indices = [33, 133, 160, 159, 158, 144, 153, 154, 155, 173]

while True:
    ret, frame = cap.read()
    if not ret:
        break

    h, w, _ = frame.shape
    frame_rgb = cv2.cvtColor(frame, cv2.COLOR_BGR2RGB)
    result = face_mesh.process(frame_rgb)

    if result.multi_face_landmarks:
        for face_landmarks in result.multi_face_landmarks:
            eye_points = []
            for idx in eye_right_indices:
                x = int(face_landmarks.landmark[idx].x * w)
                y = int(face_landmarks.landmark[idx].y * h)
                eye_points.append((x, y))

            x_coords = [p[0] for p in eye_points]
            y_coords = [p[1] for p in eye_points]
            x_min, x_max = min(x_coords), max(x_coords)
            y_min, y_max = min(y_coords), max(y_coords)

            padding = 5
            x_min = max(x_min - padding, 0)
            x_max = min(x_max + padding, w)
            y_min = max(y_min - padding, 0)
            y_max = min(y_max + padding, h)

            eye_roi = frame[y_min:y_max, x_min:x_max]
            gray_eye = cv2.cvtColor(eye_roi, cv2.COLOR_BGR2GRAY)
            gray_eye = cv2.GaussianBlur(gray_eye, (7, 7), 0)
            _, thresh_eye = cv2.threshold(gray_eye, 30, 255, cv2.THRESH_BINARY_INV)

            contours, _ = cv2.findContours(thresh_eye, cv2.RETR_TREE, cv2.CHAIN_APPROX_SIMPLE)
            if contours:
                biggest = max(contours, key=cv2.contourArea)
                (x, y, w_box, h_box) = cv2.boundingRect(biggest)

                cx = x + w_box // 2
                cy = y + h_box // 2

                # Escalamos al tamaño de pantalla
                roi_height, roi_width = eye_roi.shape[:2]
                screen_x = int(screen_width * cx / roi_width)
                screen_y = int(screen_height * cy / roi_height)

                pyautogui.moveTo(screen_x, screen_y, duration=0.1)  # Movimiento suave

                # Mostrar para depuración
                cv2.rectangle(eye_roi, (x, y), (x + w_box, y + h_box), (255, 0, 0), 1)
                cv2.circle(eye_roi, (cx, cy), 3, (0, 255, 0), -1)
                cv2.imshow("Ojo Derecho", eye_roi)

    cv2.imshow("Camara", frame)

    if cv2.waitKey(1) & 0xFF == ord('q'):
        break

cap.release()
cv2.destroyAllWindows()
