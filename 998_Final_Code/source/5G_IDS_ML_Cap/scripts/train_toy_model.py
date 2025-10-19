import numpy as np
from sklearn.ensemble import RandomForestClassifier
from sklearn.model_selection import train_test_split
from sklearn.metrics import classification_report
import joblib, os

# 1. Generate synthetic dataset
rng = np.random.default_rng(42)
N = 6000

# benign flows
pkt_b = rng.integers(40, 950, size=N//2)
byt_b = rng.integers(800, 9000, size=N//2)

# attack flows
pkt_a = rng.integers(1100, 3200, size=N//2)
byt_a = rng.integers(12000, 60000, size=N//2)

X = np.vstack([
    np.c_[pkt_b, byt_b],
    np.c_[pkt_a, byt_a]
]).astype(np.float32)

y = np.r_[np.zeros(len(pkt_b)), np.ones(len(pkt_a))]  # 0=benign, 1=attack

# 2. Train/test split
Xtr, Xte, ytr, yte = train_test_split(X, y, test_size=0.2, random_state=7, stratify=y)

# 3. Train RandomForest
clf = RandomForestClassifier(n_estimators=150, random_state=7, n_jobs=-1)
clf.fit(Xtr, ytr)

# 4. Evaluate
print("Train acc:", clf.score(Xtr, ytr))
print("Test  acc:", clf.score(Xte, yte))
print(classification_report(yte, clf.predict(Xte), digits=3))

# 5. Save model
os.makedirs("models", exist_ok=True)
joblib.dump(clf, "models/rf_toy.pkl")
print("Saved model to models/rf_toy.pkl")
