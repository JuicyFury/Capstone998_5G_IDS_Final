# dashboard.py 
import os 
import requests 
import streamlit as st 
import pandas as pd
import random, json

# Function to load offline mode controls

# Manual test form
import json
import requests
import streamlit as st

def manual_test_ui():
    st.subheader("Manual Flow Test")
    st.markdown(
        "Paste a single flow in JSON format matching your model’s expected schema (see `models/features.txt`)."
    )

    # Example template to guide user
    example_flow = {
        "time_start": 1.23,
        "time_end": 2.34,
        "ue_total": 5,
        "src_port": 80,
        "dst_port": 443,
        "protocol": 6,
        "packet_size": 512,
        "flow_duration": 1.11,
        "total_bytes_fwd": 2000,
        "total_bytes_bwd": 1500,
        "total_pkts_fwd": 20,
        "total_pkts_bwd": 15,
        "pkts_per_sec": 25.3,
        "bytes_per_sec": 1200.4,
        "flow_pkts_per_sec": 25.3,
        "flow_bytes_per_sec": 1200.4,
        "jitter_ms": 2.5,
        "delay_ms": 1.1,
        "src_ip_numeric": 12345,
        "src_ip_freq": 0.1,
        "dst_ip_numeric": 54321,
        "dst_ip_freq": 0.2,
        "fwd_bwd_packets_ratio": 1.33,
        "fwd_bwd_bytes_ratio": 1.22,
        "avg_packet_size": 430.2,
        "flow_duration_log": 0.33,
        "is_common_port": 1,
        "port_range": 2
    }

    st.code(json.dumps(example_flow, indent=2), language="json")

    # Input text area for user JSON
    user_input = st.text_area("Paste your flow JSON below:", height=250, placeholder="Enter your flow data here...")

    if st.button("🔍 Score Flow", use_container_width=True):
        if not user_input.strip():
            st.warning("Please paste valid JSON data before scoring.")
            return
        try:
            # Parse and validate JSON
            flow_data = json.loads(user_input)
            resp = requests.post(f"{API_BASE}/score", json=flow_data)
            if resp.ok:
                result = resp.json()
                st.success("✅ Scoring successful!")
                st.json(result)
            else:
                st.error(f"Server returned {resp.status_code}: {resp.text}")
        except json.JSONDecodeError as e:
            st.error(f"Invalid JSON: {e}")
   
# Traffic generator                 
def generate_traffic_ui():
    st.subheader("Generate Synthetic Traffic")
    n = st.slider("Number of Samples", 1, 50, 10)
    features = load_feature_columns()

    if st.button("Generate and Send"):
        st.info("Generating random flows and scoring via API...")
        rows = []
        progress = st.progress(0)

        for i in range(n):
            sample = {}
            for f in features:
                # === Match correct types based on your Flow model ===
                if f in ["ue_total", "src_port", "dst_port", "is_common_port", "port_range", "protocol"]:
                    sample[f] = random.randint(0, 65535)
                elif f in [
                    "total_bytes_fwd",
                    "total_bytes_bwd",
                    "total_pkts_fwd",
                    "total_pkts_bwd"
                ]:
                    sample[f] = random.randint(10, 50000)
                elif f in [
                    "src_ip_numeric",
                    "dst_ip_numeric"
                ]:
                    sample[f] = random.randint(100000, 999999999)
                elif "freq" in f:
                    sample[f] = round(random.uniform(0.0, 5.0), 5)
                elif "ratio" in f:
                    sample[f] = round(random.uniform(0.0, 10.0), 5)
                elif "duration" in f or "delay" in f or "jitter" in f:
                    sample[f] = round(random.uniform(0.01, 10.0), 5)
                elif "bytes" in f or "pkts" in f or "size" in f:
                    sample[f] = round(random.uniform(0.01, 5000.0), 5)
                elif "log" in f:
                    sample[f] = round(random.uniform(0.1, 3.0), 5)
                elif f.startswith("time_"):
                    sample[f] = round(random.uniform(0.0, 100.0), 5)
                else:
                    sample[f] = round(random.uniform(0.0, 1.0), 5)

            # ✅ POST correctly to FastAPI
            try:
                resp = requests.post(f"{API_BASE}/score", json=sample)
                if resp.ok:
                    result = resp.json()
                    sample.update(result)
                    rows.append(sample)
                else:
                    st.warning(f"Server returned {resp.status_code}: {resp.text}")
            except Exception as e:
                st.warning(f"Error sending sample: {e}")

            progress.progress((i + 1) / n)

        if rows:
            df = pd.DataFrame(rows)
            st.success(f"✅ Simulation complete — {len(rows)} samples scored.")
            st.dataframe(df, use_container_width=True)
        else:
            st.error("No valid results returned.")
            
# CSV upload and batch scoring
def upload_csv_ui():
    st.subheader("Upload CSV for Scoring")
    uploaded_file = st.file_uploader("Upload a CSV file", type=["csv"])
    if uploaded_file is not None:
        df = pd.read_csv(uploaded_file)
        st.write("📄 Preview:")
        st.dataframe(df.head(), use_container_width=True)

        if st.button("Score All Rows"):
            results = []
            for i, row in df.iterrows():
                flow_dict = row.to_dict()
                # convert all numpy types to Python types
                flow_dict = {k: (int(v) if isinstance(v, (float, int)) and v.is_integer() else float(v) if isinstance(v, (float, int)) else v) for k, v in flow_dict.items()}
                try:
                    resp = requests.post(f"{API_BASE}/score", json=flow_dict)
                    if resp.ok:
                        results.append(resp.json())
                    else:
                        st.warning(f"Row {i+1} failed: {resp.text}")
                except Exception as e:
                    st.error(f"Row {i+1}: {e}")

            if results:
                st.success("✅ Scoring complete")
                st.dataframe(pd.DataFrame(results))


# Load feature names from features.txt
def load_feature_columns():
    """Reads features.txt and returns a clean list of feature names."""
    try:
        with open("models/features.txt") as f:
            features = [line.strip() for line in f if line.strip()]
        return features
    except FileNotFoundError:
        return []

# ---------- Config ----------
st.set_page_config(page_title="5G IDS Dashboard", layout="wide")
API_BASE = (
    st.secrets.get("api", {}).get("base")
    or os.environ.get("IDS_API_BASE")
    or "http://127.0.0.1:8000"
)
DEFAULT_USER = os.environ.get("IDS_ADMIN_USER", "admin")
DEFAULT_PASS = os.environ.get("IDS_ADMIN_PASS", "ids2025")

# ---------- Helpers ----------
def api_health() -> bool:
    try:
        r = requests.get(f"{API_BASE}/health", timeout=2)
        return r.ok
    except Exception:
        return False

def get_metrics() -> dict:
    try:
        r = requests.get(f"{API_BASE}/metrics_simple", timeout=5)
        return r.json()
    except Exception:
        return {"total": 0, "attack": 0, "benign": 0}

# ---------- Login ----------
def login_view():
    st.title("🔐 5G IDS — Admin Login")
    st.caption("Demo credentials can be changed later via env vars or Streamlit secrets.")
    u = st.text_input("Username", value="")
    p = st.text_input("Password", value="", type="password")

    if st.button("Sign in", use_container_width=True):
        auth_user = st.secrets.get("auth", {}).get("username", DEFAULT_USER)
        auth_pass = st.secrets.get("auth", {}).get("password", DEFAULT_PASS)
        if u == auth_user and p == auth_pass:
            st.session_state.auth = True
            st.session_state.user = u
            st.session_state.mode = "🏠 Home"   # ✅ Go to Home view directly
            st.success("Signed in ✅")
            st.rerun()                         # ✅ Refresh to render home_view()
        else:
            st.error("Invalid credentials")

# ---------- Modes ----------
def mode_selector():
    return st.sidebar.radio(
        "🧭 Mode Selector",
        ["Offline (current API)", "Online (NS3)"],
        help="Offline uses your FastAPI + ML model. Online is a placeholder for live NS3 feed."
    )

def header_bar(mode: str):
    """Top header with title, mode, user and logout button."""
    st.markdown(
        """
        <style>
        .header-wrap {background-color:#0E1117;border-radius:10px;padding:10px 16px;margin-bottom:10px;}
        .hdr-title {font-size:1.2rem;font-weight:600;margin:0.2rem 0;}
        .hdr-sub {opacity:0.85;font-size:0.9rem;margin-top:0.2rem;}
        </style>
        """,
        unsafe_allow_html=True
    )

    with st.container():
        col1, col2, col3 = st.columns([6, 3, 2])
        with col1:
            st.markdown(
                '<div class="header-wrap"><div class="hdr-title">🛰️ 5G IDS – Threat Detection Dashboard</div></div>',
                unsafe_allow_html=True
            )
        with col2:
            st.markdown(
                f'<div class="header-wrap"><div class="hdr-sub">Mode: <b>{mode}</b></div></div>',
                unsafe_allow_html=True
            )
        with col3:
            user = st.session_state.get("user", "admin")
            st.markdown(
                f'<div class="header-wrap"><div class="hdr-sub">👤 {user}</div></div>',
                unsafe_allow_html=True
            )
        if st.button("🚪 Logout", key="logout_top", use_container_width=True):
            st.session_state.auth = False
            st.session_state.user = None
            st.session_state.mode = "🏠 Home"
            st.rerun()


def offline_shell():
    st.header("🧪 Offline Mode")
    api_ok = api_health()
    st.info(f"API base: **{API_BASE}** — Health: {'🟢 UP' if api_ok else '🔴 DOWN'}")

    tab1, tab2, tab3 = st.tabs(["Manual Test", "Generate Traffic", "Upload CSV"])
    with tab1:
        manual_test_ui()
    with tab2:
        generate_traffic_ui()
    with tab3:
        upload_csv_ui()

    st.divider()
    st.subheader("Quick Metrics")
    metrics = get_metrics()
    c1, c2, c3 = st.columns(3)
    c1.metric("Total Requests", metrics.get("total", 0))
    c2.metric("Attack", metrics.get("attack", 0))
    c3.metric("Benign", metrics.get("benign", 0))

def online_placeholder():
    st.header("📡 Online Mode (NS3 Live Feed)")
    st.markdown("---")
    st.subheader("Coming Soon: Real-Time NS3 Integration")

    st.info(
        "This mode will simulate live traffic streaming from the NS3 5G network emulator."
    )

    st.markdown("""
    ### Planned Features
    - 🔗 **Connect to NS3**: Establish a socket or REST bridge to receive live network flow data.  
    - 🧠 **Feature Extraction**: Convert raw NS3 traffic into model-ready feature vectors.  
    - 🚨 **Real-Time Scoring**: Run each flow through your ML IDS to predict 'attack' vs 'benign'.  
    - 📈 **Visual Analytics**: Real-time charts showing detected threats, confidence levels, and latency.  
    - 🔔 **Alert System**: Push alerts to dashboard or email on high-severity anomalies.
    """)

    st.warning("🕓 This section is under development. No live NS3 feed available yet.")
    st.button("Connect to NS3 (disabled)", disabled=True)


# --- Temporarily hide the Recent Decisions table until CSV logging is fixed ---
# st.subheader("Recent Decisions (runs/decisions.csv)")
# st.info("🧩 Decision logging temporarily disabled while CSV schema is being updated.")
    # st.subheader("Recent Decisions (runs/decisions.csv)")
    # features = load_feature_columns()

    # try:
    #     if os.path.exists("runs/decisions.csv"):
    #         with open("runs/decisions.csv", "r", errors="ignore") as f:
    #             lines = [line.strip() for line in f if line.strip()]
    #         header = lines[0].split(",")
    #         max_cols = len(header)

    #         cleaned = []
    #         for line in lines[1:]:
    #             parts = line.split(",")
    #             if len(parts) < max_cols:
    #                 parts += [""] * (max_cols - len(parts))
    #             cleaned.append(parts[:max_cols])

    #         df = pd.DataFrame(cleaned, columns=header)

    #         expected_cols = load_feature_columns() + ["label", "confidence", "rule", "latency_ms", "timestamp"]
    #         for col in expected_cols:
    #             if col not in df.columns:
    #                 df[col] = None

    #         df = df[expected_cols]

    #         st.dataframe(df.tail(15).iloc[::-1], use_container_width=True)

    #         # ✅ Unique key prevents "duplicate button ID" error
    #         if st.button("🔄 Refresh Table", key="refresh_decision_table"):
    #             st.rerun()
    #     else:
    #         st.caption("📭 No decisions.csv found yet. It will appear after you start scoring.")
    # except Exception as e:
    #     st.warning(f"⚠️ Error loading decisions.csv: {e}")
        
# ---------- Auth ----------
if "auth" not in st.session_state:
    st.session_state.auth = False

if not st.session_state.auth:
    login_view()
    st.stop()

# ---------- Sidebar ----------
# ✅ Only show Home link after login
# ---------- Sidebar ----------
with st.sidebar:
    st.sidebar.success(f"Signed in as: {st.session_state.get('user','admin')}")
    st.sidebar.write(f"API: {'🟢' if api_health() else '🔴'}")

    # 🧭 Unified navigation (Home + modes)
    mode = st.radio(
        "🧭 Navigation",
        ["🏠 Home", "🧪 Offline (current API)", "📡 Online (NS3)"],
        index=0 if "mode" not in st.session_state else
              ["🏠 Home", "🧪 Offline (current API)", "📡 Online (NS3)"].index(st.session_state.get("mode", "🏠 Home")),
        key="mode_selector",
        help="Navigate between Home, Offline mode, and Online NS3 placeholder."
    )

    # Store mode persistently
    st.session_state.mode = mode

# ---------- MAIN ----------
header_bar(mode)
st.title("5G IDS Dashboard")

mode = st.session_state.get("mode", "🏠 Home")

# ---------- HOME CONTENT ----------
def home_view():
    st.title("🛰️ 5G IDS – Intrusion Detection System")
    st.subheader("A Real-Time Threat Detection Platform for 5G Networks")

    st.markdown("""
    This project demonstrates an AI-driven Intrusion Detection System (IDS) built for **5G network environments**.
    It combines **FastAPI**, **LightGBM ML model**, and a **Streamlit dashboard** for visualization and interaction.

    ### 💡 Key Features
    - Offline Mode (Current API): Test the model and view results using generated or uploaded flow data.  
    - Online Mode (NS3 Live Feed): Future live integration from NS3 5G traffic simulator.  
    - Batch scoring, CSV uploads, and automatic decision logging.  
    - Scalable and modular structure ready for production use.
    """)

    st.divider()
    st.subheader("Get Started 🔗")

    col1, col2 = st.columns(2)

    with col1:
        st.markdown("#### ⚙️ Offline (current API)")
        st.write("Use the local FastAPI backend and LightGBM model for scoring network flows.")
        if st.button("Launch Offline Mode", type="primary", use_container_width=True, key="go_offline"):
            st.session_state.mode = "🧪 Offline (current API)"
            st.rerun()

    with col2:
        st.markdown("#### 📡 Online (NS3)")
        st.write("Placeholder for NS3 integration and real-time scoring.")
        if st.button("Launch Online Mode", use_container_width=True, key="go_online"):
            st.session_state.mode = "📡 Online (NS3)"
            st.rerun()

    st.markdown("---")
    st.caption("© 2025 5G IDS Capstone – Built with FastAPI, LightGBM, and Streamlit")

# ---------- NAVIGATION HANDLER ----------
if mode.startswith("🏠"):
    home_view()
elif "Offline" in mode:
    offline_shell()
elif "Online" in mode:
    online_placeholder()
else:
    st.warning("⚙️ Unknown mode selected.")