from fastapi import FastAPI, HTTPException
from pydantic import BaseModel, Field
import ipaddress
import time
import logging
from collections import Counter
from .csv_logger import CSVLogger
from .scorer_ml import MLScorer
import os

# Set up logging once, at import time
logging.basicConfig(
    level=logging.INFO,
    format="%(asctime)s | %(levelname)s | %(message)s"
)
stats = Counter()

# Initialize FastAPI app
app = FastAPI(title="5G IDS service", version="0.1")

# Request model
class Flow(BaseModel):
    # === 28 ML features ===
    time_start: float
    time_end: float
    ue_total: int
    src_port: int
    dst_port: int
    protocol: int
    packet_size: float
    flow_duration: float
    total_bytes_fwd: int
    total_bytes_bwd: int
    total_pkts_fwd: int
    total_pkts_bwd: int
    pkts_per_sec: float
    bytes_per_sec: float
    flow_pkts_per_sec: float
    flow_bytes_per_sec: float
    jitter_ms: float
    delay_ms: float
    src_ip_numeric: int
    src_ip_freq: float
    dst_ip_numeric: int
    dst_ip_freq: float
    fwd_bwd_packets_ratio: float
    fwd_bwd_bytes_ratio: float
    avg_packet_size: float
    flow_duration_log: float
    is_common_port: int
    port_range: int

    # Optional metadata (ignored by ML)
    src_ip: str | None = None
    dst_ip: str | None = None


# Response model
class ScoreResponse(BaseModel):
    label: str
    confidence: float
    rule: str
    latency_ms: float

    
# Initialize CSV logger
csv_logger = CSVLogger("runs/decisions.csv", feature_file="models/features.txt")

# Load the real LightGBM model (native format)
try:
    scorer = MLScorer(
        model_path="models/lightgbm_92.6.txt",
        scaler_json_path="models/scaler_params.json",
        features_path="models/features.txt"
    )
    logging.info("✅ Real ML model loaded successfully.")
except Exception as e:
    logging.error(f"❌ Could not initialize MLScorer: {e}")
    raise e


# GET Requests
@app.get("/health")
def health():
    return {"ok": True}

# Simple metrics endpoint
@app.get("/metrics_simple")
def metrics_simple():
    return {
        "total": stats["requests"],
        "attack": stats["label_attack"],
        "benign": stats["label_benign"],
    }

#Main POST scoring endpoint and execution logic !!!
@app.post("/score", response_model=ScoreResponse)
def score(flow: Flow):
    t0 = time.perf_counter()

    # Convert flow to dict for model input
    flow_dict = flow.model_dump()

    # ---- Predict with MLScorer ----
    if isinstance(scorer, MLScorer):
        label, conf = scorer.predict_with_conf(flow_dict)
        rule = "ml:lightgbm_92.6"
    else:
        label, rule = scorer.predict(flow_dict)
        conf = 0.9 if label == "attack" else 0.1

    # ---- Metrics ----
    dt_ms = (time.perf_counter() - t0) * 1000.0
    stats["requests"] += 1
    stats[f"label_{label}"] += 1

    # ---- Logging ----
    logging.info(
        "Flow scored -> label=%s conf=%.2f rule=%s latency_ms=%.2f",
        label, conf, rule, dt_ms
    )

    # Save to CSV (now only logs label + latency)
    csv_logger.log(flow_dict, label, conf, rule, dt_ms)

    # ---- Response ----
    return {
        "label": label,
        "confidence": round(conf, 3),
        "rule": rule,
        "latency_ms": round(dt_ms, 2)
    }
