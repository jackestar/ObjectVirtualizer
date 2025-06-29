import flask
import json
import math
import csv
import time
from flask_cors import CORS

# --- Central State Management ---
# This dictionary will hold the entire state of our application.
# The UI and ESP32 will sync with this state.
STATE = {
    "config": {
        "speedElv": 15,
        "speedTab": 10,
        "readThreshold": 3500,  # Analog value (0-4095)
        "stepPerRead": 10,
        "zStepIncrement": 592, # Steps for the elevator to move up
        "logoSampling": 100,
        # Calibration values: map analog readings to real-world distance (e.g., mm)
        "calib_analog_near": 400,
        "calib_dist_near": 10,  # e.g., 10mm
        "calib_analog_far": 3800,
        "calib_dist_far": 150   # e.g., 150mm
    },
    "esp_status": {
        "q1": False,
        "q2": False,
        "q3": False,
        "z_pos": 0,
        "last_seen": "never"
    },
    "scan_data": {
        # Cartesian points for 3D plotting
        "points_cartesian": [],
        # Original cylindrical data for saving
        "points_cylindrical": []
    }
}

app = flask.Flask(__name__)
CORS(app)

# --- Frontend Routes ---
@app.route('/')
def index():
    """Serves the main control panel HTML page."""
    return flask.render_template('index.html')

@app.route('/save_points', methods=['GET'])
def save_points():
    """Saves the collected points to a CSV file."""
    points_to_save = STATE['scan_data']['points_cylindrical']
    if not points_to_save:
        return flask.jsonify({"status": "info", "message": "No points to save"}), 200

    filename = "collected_points.csv"
    try:
        with open(filename, 'w', newline='') as csvfile:
            fieldnames = ['r_mm', 'theta_rad', 'z_steps']
            writer = csv.DictWriter(csvfile, fieldnames=fieldnames)
            writer.writeheader()
            for p in points_to_save:
                writer.writerow({'r_mm': p['r'], 'theta_rad': p['theta'], 'z_steps': p['z']})
        
        # Optionally, clear points after saving. Let's make this a separate action.
        # STATE['scan_data']['points_cartesian'] = []
        # STATE['scan_data']['points_cylindrical'] = []
        return flask.jsonify({"status": "success", "message": f"Points saved to {filename}"}), 200
    except Exception as e:
        return flask.jsonify({"status": "error", "message": str(e)}), 500

@app.route('/clear_points', methods=['POST'])
def clear_points():
    """Clears all scanned data from the server's memory."""
    STATE['scan_data']['points_cartesian'] = []
    STATE['scan_data']['points_cylindrical'] = []
    return flask.jsonify({"status": "success", "message": "Scanned data cleared."})

# --- API Routes for ESP32 and Frontend ---

@app.route('/api/status', methods=['GET'])
def get_status():
    """Provides the full system state to the ESP32 and frontend."""
    return flask.jsonify(STATE)

@app.route('/api/config', methods=['POST'])
def update_config():
    """Receives configuration updates from the frontend."""
    try:
        data = flask.request.get_json()
        for key, value in data.items():
            # Basic type casting and validation
            if key in STATE['config']:
                original_type = type(STATE['config'][key])
                STATE['config'][key] = original_type(value)
        
        print(f"Updated config: {STATE['config']}")
        return flask.jsonify({"status": "success", "message": "Config updated."})
    except Exception as e:
        return flask.jsonify({"status": "error", "message": str(e)}), 400

@app.route('/api/esp_status', methods=['POST'])
def receive_esp_status():
    """Receives status updates from the ESP32."""
    try:
        data = flask.request.get_json()
        STATE['esp_status']['q1'] = data.get('q1', False)
        STATE['esp_status']['q2'] = data.get('q2', False)
        STATE['esp_status']['q3'] = data.get('q3', False)
        STATE['esp_status']['z_pos'] = data.get('z_pos', 0)
        STATE['esp_status']['last_seen'] = time.strftime("%Y-%m-%d %H:%M:%S")
        return flask.jsonify({"status": "success"})
    except Exception as e:
        return flask.jsonify({"status": "error", "message": str(e)}), 400

@app.route('/api/data', methods=['POST'])
def receive_data_point():
    """Receives a single scanned data point (cylindrical) from the ESP32."""
    try:
        data = flask.request.get_json()
        r = float(data['r'])       # radius in mm (already calibrated by ESP)
        theta = float(data['theta']) # angle in radians
        z = float(data['z'])       # z-height in steps

        # Store the original cylindrical data
        STATE['scan_data']['points_cylindrical'].append({"r": r, "theta": theta, "z": z})

        # Convert to Cartesian for 3D plotting
        # We need a scaling factor for Z to make it visually proportional to r (in mm)
        # Let's assume a Z-scaling factor, e.g., 1 step = 0.1 mm. This can be adjusted.
        Z_SCALE_FACTOR = 0.05 
        x = r * math.cos(theta)
        y = r * math.sin(theta)
        z_scaled = z * Z_SCALE_FACTOR
        
        STATE['scan_data']['points_cartesian'].append({"x": x, "y": y, "z": z_scaled})

        print(f"Received Point: r={r:.2f}mm, theta={theta:.2f}rad, z={z}steps -> Stored Cartesian: x={x:.2f}, y={y:.2f}, z_scaled={z_scaled:.2f}")
        return flask.jsonify({"status": "success"}), 200
    except Exception as e:
        print(f"Error receiving data point: {e}")
        return flask.jsonify({"status": "error", "message": str(e)}), 400

if __name__ == '__main__':
    app.run(host='0.0.0.0', port=5000, debug=True, use_reloader=False)