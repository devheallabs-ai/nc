"""
Step 2: Serve the model as an HTTP API.

This turns your .pkl model into an API that NC can call.
Run: python3 serve_model.py
Then: nc serve predict.nc

No Flask needed — uses Python's built-in http.server.
"""

import json, pickle
from http.server import HTTPServer, BaseHTTPRequestHandler

model = pickle.load(open("model.pkl", "rb"))
print("Model loaded. Serving on http://localhost:5000")

class Handler(BaseHTTPRequestHandler):
    def do_POST(self):
        length = int(self.headers.get("Content-Length", 0))
        body = json.loads(self.rfile.read(length)) if length else {}

        if self.path == "/predict":
            features = body.get("features", [])
            prediction = int(model.predict([features])[0])
            confidence = float(max(model.predict_proba([features])[0]))
            result = {
                "prediction": prediction,
                "label": "will_buy" if prediction == 1 else "wont_buy",
                "confidence": round(confidence, 2),
                "features": features
            }
            self.send_response(200)
            self.send_header("Content-Type", "application/json")
            self.end_headers()
            self.wfile.write(json.dumps(result).encode())
        else:
            self.send_response(404)
            self.end_headers()

    def log_message(self, format, *args):
        print(f"  [{args[1]}] {args[0]}")

HTTPServer(("0.0.0.0", 5000), Handler).serve_forever()
