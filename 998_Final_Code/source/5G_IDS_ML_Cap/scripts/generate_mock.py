import argparse, random, requests, json, time, sys

def rand_ip():
    return f"10.{random.randint(0,255)}.{random.randint(0,255)}.{random.randint(1,254)}"

def main(n, attack_rate, host, delay, debug):
    url = f"http://{host}/score"
    print(f"[INFO] Sending {n} mock flows to {url} (attack_rate={attack_rate})", file=sys.stderr)
    for i in range(1, n+1):
        is_attack = random.random() < attack_rate
        pkt_count = random.randint(1200, 3000) if is_attack else random.randint(50, 900)
        byte_count = random.randint(12000, 50000) if is_attack else random.randint(1000, 9000)

        flow = {
            "pkt_count": pkt_count,
            "byte_count": byte_count,
            "src_ip": rand_ip(),
            "dst_ip": rand_ip(),
        }

        try:
            if debug:
                print(f"[DEBUG] ({i}/{n}) -> {flow}", file=sys.stderr)
            r = requests.post(url, json=flow, timeout=5)
            r.raise_for_status()
            print(json.dumps(r.json(), indent=2))
        except Exception as e:
            print(f"[ERROR] ({i}/{n}) failed: {e}", file=sys.stderr)
        time.sleep(delay)

if __name__ == "__main__":
    ap = argparse.ArgumentParser()
    ap.add_argument("--n", type=int, default=5)
    ap.add_argument("--attack-rate", type=float, default=0.5)
    ap.add_argument("--host", type=str, default="127.0.0.1:8000")
    ap.add_argument("--delay", type=float, default=0.2)
    ap.add_argument("--debug", action="store_true")
    args = ap.parse_args()
    main(args.n, args.attack_rate, args.host, args.delay, args.debug)
