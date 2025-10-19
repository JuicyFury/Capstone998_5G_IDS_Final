# 5G_IDS_ML_Cap
IDS for detecting threats on the 5G network using ML models and Network Simulation.

This is the base skeleton for our Intrusion Detection System on 5G traffic.

## How to run locally

1. Create a virtual environment:
   python -m venv .venv
   #Windows: .venv\Scripts\activate

2. Install Dependencies
    pip install -r requirements.txt

3. Run the API
    uvicorn service.api:app --reload --port 8000

4.  Send mock Traffic
    python scripts/generate_mock.py --n 5 --attack-rate 0.5

5. Then open http://127.0.0.1:8000/docs
    to test endpoints.
