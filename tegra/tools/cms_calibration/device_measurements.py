# Change these to the absolute gray values of your measurements
gray_abs = [0, 16, 32, 48, 64, 80, 96, 112, 128, 144, 160, 176, 192, 208, 224, 240, 255]

# Change these to the measured gray values from your panel
gamma_device_scaled = [0.0009, 0.0026, 0.0076, 0.0189, 0.0383, 0.0668, 0.1147, 0.1722, 0.2426, 0.3332, 0.4291, 0.5309, 0.6154, 0.7238, 0.8287, 0.9279, 1.0000]

assert len(gray_abs) == len(gamma_device_scaled)
