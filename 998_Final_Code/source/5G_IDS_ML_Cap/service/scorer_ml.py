import json
import numpy as np
import lightgbm as lgb
import logging

class MLScorer:
    def __init__(self, model_path: str, scaler_json_path: str, features_path: str):
        # Load LightGBM model
        self.model = lgb.Booster(model_file=model_path)
        logging.info(f"✅ Loaded LightGBM model from {model_path}")

        # Load scaler parameters
        try:
            with open(scaler_json_path, "r") as f:
                params = json.load(f)
            self.mean_ = np.array(params["mean_"])
            self.scale_ = np.array(params["scale_"])
            self.n_features = params["n_features_in_"]
            logging.info("✅ Loaded scaler parameters from JSON.")
        except Exception as e:
            logging.warning(f"⚠️ Could not load scaler JSON: {e}")
            self.mean_ = None
            self.scale_ = None
            self.n_features = None

        # Load feature names
        try:
            with open(features_path, "r") as f:
                self.features = [x.strip() for x in f.readlines() if x.strip()]
            logging.info(f"✅ Loaded {len(self.features)} feature names from {features_path}")
        except Exception as e:
            raise RuntimeError(f"❌ Could not load features file: {e}")

    def _apply_scaler(self, X: np.ndarray) -> np.ndarray:
        """Apply normalization using provided mean and scale arrays."""
        if self.mean_ is not None and self.scale_ is not None:
            if X.shape[1] == len(self.mean_):
                return (X - self.mean_) / self.scale_
            else:
                logging.warning(
                    f"⚠️ Feature mismatch: model expects {len(self.mean_)}, got {X.shape[1]}"
                )
        return X

    def predict_with_conf(self, flow_dict: dict):
        """Predict label and confidence."""
        try:
            x = np.array([flow_dict[f] for f in self.features], dtype=float).reshape(1, -1)
        except KeyError as e:
            raise ValueError(f"Missing feature in input: {e}")

        # Apply manual scaling
        x_scaled = self._apply_scaler(x)

        # LightGBM prediction
        y_pred = self.model.predict(x_scaled)
        conf = float(y_pred[0])
        label = "attack" if conf > 0.5 else "benign"

        return label, conf
