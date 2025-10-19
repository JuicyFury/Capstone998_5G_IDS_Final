# service/csv_logger.py
import csv
import os
from datetime import datetime

class CSVLogger:
    def __init__(self, path: str, feature_file: str = "models/features.txt"):
        self.path = path
        self.feature_file = feature_file
        os.makedirs(os.path.dirname(path), exist_ok=True)
        self.fields = self._load_features()

        # Append metadata columns
        self.fields += ["label", "confidence", "rule", "latency_ms", "timestamp"]

        # Create file with headers if not present
        if not os.path.exists(path):
            with open(path, "w", newline="") as f:
                writer = csv.DictWriter(f, fieldnames=self.fields)
                writer.writeheader()

    def _load_features(self):
        try:
            with open(self.feature_file, "r") as f:
                features = [line.strip() for line in f.readlines() if line.strip()]
            return features
        except Exception:
            # fallback: empty if file not found
            return []

    def log(self, flow_dict, label, confidence, rule, latency_ms):
        # Start with all features, then append metadata
        row = {}

        # Fill each feature column properly
        for f in self.fields:
            if f in flow_dict:
                row[f] = flow_dict[f]
            elif f == "label":
                row[f] = label
            elif f == "confidence":
                row[f] = confidence
            elif f == "rule":
                row[f] = rule
            elif f == "latency_ms":
                row[f] = round(latency_ms, 2)
            elif f == "timestamp":
                row[f] = datetime.now().strftime("%Y-%m-%d %H:%M:%S")
            else:
                row[f] = ""  # blank if missing

        try:
            with open(self.path, "a", newline="") as f:
                writer = csv.DictWriter(f, fieldnames=self.fields)
                writer.writerow(row)
        except Exception as e:
            print(f"⚠️ CSVLogger error: {e}")