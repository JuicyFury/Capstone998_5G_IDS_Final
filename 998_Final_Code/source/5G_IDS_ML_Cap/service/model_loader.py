# """
# Placeholder where later we can load the trained ML model (Sckit-Learn, XGBoost or ONIX)
# """
import joblib
import numpy as np

class SKModelScorer:

    def __init__(self, path: str = "models/rf_toy.pkl"):
        self.model = joblib.load(path)

    def predict_with_conf(self, flow_dict: dict):
        x = np.array([[flow_dict["pkt_count"], flow_dict["byte_count"]]], dtype=np.float32)
        proba = self.model.predict_proba(x)[0, 1]  # probability of attack
        label = "attack" if proba >= 0.5 else "benign"
        return label, float(proba)

    # Add this wrapper for compatibility with DummyScorer
    def predict(self, flow_dict: dict):
        label, conf = self.predict_with_conf(flow_dict)
        return label, f"ML_conf_{conf:.2f}"


