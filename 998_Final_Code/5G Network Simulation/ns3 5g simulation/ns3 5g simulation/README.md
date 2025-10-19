# 5G NR DDoS Detection Dataset Generator

A comprehensive NS-3 simulation framework for generating machine learning datasets to detect DDoS attacks in 5G networks. This project generates structured CSV datasets with network flow features suitable for training ML models to distinguish between benign and malicious traffic.

## 📋 Table of Contents

- [Overview](#overview)
- [Features](#features)
- [Project Structure](#project-structure)
- [Dataset Specification](#dataset-specification)
- [Installation & Setup](#installation--setup)
- [Usage](#usage)
- [Dataset Generation](#dataset-generation)
- [Output Format](#output-format)
- [ML-Ready Features](#ml-ready-features)
- [Troubleshooting](#troubleshooting)
- [Contributing](#contributing)

## 🔍 Overview

This project implements a parameterized NS-3 simulation framework that generates labeled datasets for machine learning-based DDoS detection in 5G networks. The simulator creates realistic network scenarios with varying node counts, mobility patterns, transport protocols, and attack intensities to produce comprehensive training data.

### Key Capabilities

- **Multi-scenario simulation**: 10/15 UEs, static/mobile, UDP/TCP, benign/DDoS
- **Attack intensity variation**: Low, medium, high DDoS attack rates
- **ML-ready CSV export**: Per-second flow statistics with labels
- **Batch processing**: 70 and 20 repetitions across all parameter combinations
- **Realistic 5G modeling**: NR module integration with mobility and channel models

## ✨ Features

### Simulation Features

- **Node Counts**: 10, 15, 25, 50 UEs
- **Mobility Models**: Static positioning, RandomWalk2d mobility
- **Transport Protocols**: UDP, TCP with configurable parameters
- **Attack Scenarios**: Benign traffic, DDoS attacks with varying intensities
- **Network Topology**: 5G NR with gNB, UEs, and realistic channel models

### Dataset Features

- **Time Windows**: 1-second sampling intervals
- **Flow Statistics**: Per-flow throughput, delay, jitter, packet loss
- **Traffic Classification**: Binary and intensity-based labels
- **ML-Ready Format**: CSV with standardized column names
- **Comprehensive Coverage**: 2,640+ scenarios across all parameter combinations

## 📁 Project Structure

```
ns3-5g-simulation/
├── README.md                           # This file
├── nr_ddos_dataset.cc                  # Original NR-based dataset generator
├── simple_ddos_dataset.cc              # Simplified dataset generator (recommended)
├── run_scenarios.ps1                    # PowerShell batch runner
├── run_win_datasets.ps1                # Windows-specific batch runner
├── 1.simple25.cc                       # Basic 25-node scenario
├── 2.simple50.cc                       # Basic 50-node scenario
├── 3.simple25-attack.cc               # 25-node with DDoS attacks
├── 4.simple50-attack.cc               # 50-node with DDoS attacks
├── 5.simple25-mobile.cc               # 25-node with mobility
├── 6.simple50-mobile.cc               # 50-node with mobility
├── 7.simple25-mobile-ddos.cc          # 25-node mobile with DDoS
├── 8.simple50-mobile-ddos.cc          # 50-node mobile with DDoS
├── Results.xlsx                        # Summary results spreadsheet
└── datasets/                           # Generated CSV datasets
    ├── dataset_10_mobile_udp_ddos_high_r1.csv
    ├── dataset_15_static_tcp_benign_low_r70.csv
    └── ...
```

## 📊 Dataset Specification

### Purpose

Generate ML-ready labeled datasets for training and testing intrusion detection models in 5G NR environments. Labels distinguish between benign vs attack traffic with intensity levels (low/medium/high).

### Dataset Structure

Each row represents one time window of traffic (1 second). The schema includes:

| Column | Description | Type | Example |
|--------|-------------|------|---------|
| `time_start` | Window start time (s) | float | 0.0 |
| `time_end` | Window end time (s) | float | 1.0 |
| `scenario_id` | Unique scenario identifier | string | "10_mobile_udp_ddos_high_r1" |
| `ue_total` | Total number of UEs | int | 10 |
| `attackers` | Number of attacker nodes | int | 5 |
| `src_ip` | Source IP address | string | "10.1.1.1" |
| `dst_ip` | Destination IP address | string | "10.1.1.2" |
| `src_port` | Source port number | int | 49153 |
| `dst_port` | Destination port number | int | 1001 |
| `protocol` | Transport protocol (6=TCP, 17=UDP) | int | 6 |
| `packet_size` | Average packet size (bytes) | float | 1024.0 |
| `flow_duration` | Flow duration (s) | float | 1.0 |
| `total_bytes_fwd` | Forward bytes in window | int | 1024 |
| `total_bytes_bwd` | Backward bytes in window | int | 0 |
| `total_pkts_fwd` | Forward packets in window | int | 1 |
| `total_pkts_bwd` | Backward packets in window | int | 0 |
| `pkts_per_sec` | Total packets per second | float | 1.0 |
| `bytes_per_sec` | Total bytes per second | float | 1024.0 |
| `flow_pkts_per_sec` | Flow packets per second | float | 1.0 |
| `flow_bytes_per_sec` | Flow bytes per second | float | 1024.0 |
| `jitter_ms` | Average jitter (ms) | float | 0.0 |
| `delay_ms` | Average delay (ms) | float | 5.2 |
| `label_binary` | Binary attack label (0=benign, 1=attack) | int | 0 |
| `label_intensity` | Attack intensity (0=benign, 1=low, 2=med, 3=high) | int | 0 |

### Dataset Variations

The generator creates datasets for multiple conditions:

- **UE Counts**: 10, 15, 25, 50 nodes
- **Mobility**: Static vs Mobile (RandomWalk2d)
- **Transport**: UDP vs TCP
- **Attack Types**: Benign vs DDoS
- **Intensity Levels**: Low (0.0008s), Medium (0.0004s), High (0.00015s) intervals
- **Repetitions**: 70 runs per scenario combination for small scale network of 10, 15 nodes and 20 runs per scenario combination for medium to large scale network of 25, 50 nodes

## 🛠 Installation & Setup

### Prerequisites

- **NS-3**: Version 3.36+ with NR module
- **WSL**: Ubuntu 20.04+ (recommended)
- **Build Tools**: CMake, GCC, Python3
- **Dependencies**: Qt5, Boost, SQLite3

### WSL Setup (Recommended)

1.**Install Ubuntu WSL**:

```powershell
wsl --install -d Ubuntu
```

2.**Install NS-3 dependencies**:

```bash
sudo apt update
sudo apt install -y build-essential g++ python3 python3-pip git cmake \
    qtbase5-dev libxml2-dev libsqlite3-dev libboost-all-dev
```

3.**Clone and setup NS-3**:

```bash
cd ~
git clone --depth 1 https://gitlab.com/nsnam/ns-3-dev.git ~/ns-3-dev
cd ~/ns-3-dev
```

4.**Copy dataset generator**:

```bash
# Copy the simplified generator to NS-3 scratch directory
cp "/mnt/e/ns3s-5g/ns3 5g simulation/ns3 5g simulation/simple_ddos_dataset.cc" scratch/
```

5.**Build NS-3**:

```bash
cmake -S . -B build -DNS3_EXAMPLES=ON -DNS3_TESTS=OFF
cmake --build build -j2
```

### Windows Setup (Alternative)

1. **Install NS-3** on Windows
2. **Copy files** to NS-3 scratch directory
3. **Build** using waf or CMake
4. **Run** using PowerShell scripts

## 🚀 Usage

### Single Simulation

```bash
# Basic usage
./build/scratch/ns3-dev-simple_ddos_dataset-default \
  --ueTotal=10 --mobile=1 --useTcp=0 --enableAttack=1 \
  --attackInterval=0.0004 --simTime=20 \
  --outPath=./datasets --scenarioTag=test --window=1
```

### Parameter Options

| Parameter | Description | Default | Options |
|-----------|-------------|---------|---------|
| `--ueTotal` | Number of UEs | 25 | 10, 15, 25, 50 |
| `--mobile` | Enable mobility | true | 0 (static), 1 (mobile) |
| `--useTcp` | Use TCP instead of UDP | false | 0 (UDP), 1 (TCP) |
| `--enableAttack` | Enable DDoS attacks | true | 0 (benign), 1 (DDoS) |
| `--attackInterval` | Attack packet interval | 0.0002s | 0.00015s (high), 0.0004s (med), 0.0008s (low) |
| `--benignInterval` | Benign packet interval | 0.02s | 0.01s - 0.1s |
| `--simTime` | Simulation duration | 20s | 5s - 300s |
| `--outPath` | Output directory | "datasets" | Any valid path |
| `--scenarioTag` | Scenario identifier | "default" | Custom string |
| `--window` | Sampling window size | 1s | 0.1s - 10s |

## 📈 Dataset Generation

### Small Scale Network Batch Processing

The project includes automated batch processing to generate comprehensive datasets:

```bash
# Full 70-repetition batch (1,680 total scenarios)
reps=70
cd ~/ns-3-dev
for r in $(seq 1 $reps); do
  for ue in 10 15; do
    for mode in mobile static; do
      for tp in udp tcp; do
        for atk in ddos benign; do
          for name in low med high; do
            case "$name" in
              low)  interval=0.0008 ;;
              med)  interval=0.0004 ;;
              high) interval=0.00015 ;;
            esac
            mobile=$([ "$mode" = "mobile" ] && echo 1 || echo 0)
            useTcp=$([ "$tp" = "tcp" ] && echo 1 || echo 0)
            enableAttack=$([ "$atk" = "ddos" ] && echo 1 || echo 0)
            tag="${ue}_${mode}_${tp}_${atk}_${name}_r${r}"

            echo "Running: $tag"
            ./build/scratch/ns3-dev-simple_ddos_dataset-default \
              --ueTotal=$ue --mobile=$mobile --useTcp=$useTcp --enableAttack=$enableAttack \
              --attackInterval=$interval --simTime=20 \
              --outPath=/root/ns3-datasets --scenarioTag=$tag --window=1
          done
        done
      done
    done
  done
done
```

### Scenario Combinations

The batch generates datasets for all combinations:

- **UE Counts**: 10, 15 (2 options)
- **Mobility**: mobile, static (2 options)  
- **Transport**: udp, tcp (2 options)
- **Attack**: ddos, benign (2 options)
- **Intensity**: low, med, high (3 options)
- **Repetitions**: 70 runs

**Total Scenarios**: 2 × 2 × 2 × 2 × 3 × 70 = **1,680 datasets**

### Large Scale Network Batch Processing

```bash
# Full 20-repetition batch (960 total scenarios)
reps=20
cd ~/ns-3-dev
for r in $(seq 1 $reps); do
  for ue in 25 50; do
    for mode in mobile static; do
      for tp in udp tcp; do
        for atk in ddos benign; do
          for name in low med high; do
            case "$name" in
              low)  interval=0.0008 ;;
              med)  interval=0.0004 ;;
              high) interval=0.00015 ;;
            esac
            mobile=$([ "$mode" = "mobile" ] && echo 1 || echo 0)
            useTcp=$([ "$tp" = "tcp" ] && echo 1 || echo 0)
            enableAttack=$([ "$atk" = "ddos" ] && echo 1 || echo 0)
            tag="${ue}_${mode}_${tp}_${atk}_${name}_r${r}"

            echo "Running: $tag"
            ./build/scratch/ns3-dev-simple_ddos_dataset-default \
              --ueTotal=$ue --mobile=$mobile --useTcp=$useTcp --enableAttack=$enableAttack \
              --attackInterval=$interval --simTime=20 \
              --outPath=/root/ns3-datasets --scenarioTag=$tag --window=1
          done
        done
      done
    done
  done
done
```

### Scenario Combinations

The batch generates datasets for all combinations:

- **UE Counts**: 25, 50 (2 options)
- **Mobility**: mobile, static (2 options)  
- **Transport**: udp, tcp (2 options)
- **Attack**: ddos, benign (2 options)
- **Intensity**: low, med, high (3 options)
- **Repetitions**: 70 runs

**Total Scenarios**: 2 × 2 × 2 × 2 × 3 × 20 = **960 datasets**

### PowerShell Batch Runner (Alternate Windows)

```powershell
# Run batch with custom parameters
.\run_win_datasets.ps1 -NsPath "C:\path\to\ns3" -Repetitions 70

# Copy files only (no execution)
.\run_win_datasets.ps1 -NsPath "C:\path\to\ns3" -CopyOnly
```

## 📄 Output Format

### File Naming Convention

```shell
dataset_<scenario>_ue<count>_<tcp|udp>_<ddos|benign>.csv
```

**Examples**:

- `dataset_10_mobile_udp_ddos_high_r1_ue10_udp_ddos.csv`
- `dataset_15_static_tcp_benign_low_r70_ue15_tcp_benign.csv`

### Directory Structure

```shell
datasets/
├── dataset_10_mobile_udp_ddos_high_r1.csv
├── dataset_10_mobile_udp_ddos_high_r2.csv
├── dataset_10_mobile_udp_benign_low_r1.csv
├── dataset_15_static_tcp_ddos_med_r70.csv
└── ... (1,680 total files)
```

### CSV Format

```csv
time_start,time_end,scenario_id,ue_total,attackers,src_ip,dst_ip,src_port,dst_port,protocol,packet_size,flow_duration,total_bytes_fwd,total_bytes_bwd,total_pkts_fwd,total_pkts_bwd,pkts_per_sec,bytes_per_sec,flow_pkts_per_sec,flow_bytes_per_sec,jitter_ms,delay_ms,label_binary,label_intensity
0,1,10_mobile_udp_ddos_high_r1,10,5,10.1.1.1,10.1.1.2,49153,1001,17,1024,1,1024,0,1,0,1,1024,1,1024,0,5.2,1,3
1,2,10_mobile_udp_ddos_high_r1,10,5,10.1.1.3,10.1.1.2,49154,1001,17,1024,1,1024,0,1,0,1,1024,1,1024,0,5.2,1,3
```

## 🤖 ML-Ready Features

### Feature Categories

The generated datasets include all features specified in the ML requirements:

#### Network Flow Features (Green-highlighted)

- **Source IP Address**: IPv4 source address
- **Destination IP Address**: IPv4 destination address  
- **Source Port**: Source port number
- **Destination Port**: Destination port number
- **Protocol Type**: Transport protocol (6=TCP, 17=UDP)
- **Packet Size**: Average packet size in bytes

#### Traffic Statistics

- **Flow Duration**: Time duration of the flow
- **Total Bytes Forward**: Bytes sent from source to destination
- **Total Bytes Backward**: Bytes sent from destination to source
- **Total Packets Forward**: Packets sent from source to destination
- **Total Packets Backward**: Packets sent from destination to source

#### Rate Metrics

- **Packets per Second**: Rate of packet transmission
- **Bytes per Second**: Data transmission rate
- **Flow Packets per Second**: Rate of packets within specific flow
- **Flow Bytes per Second**: Rate of bytes within specific flow

#### Quality Metrics

- **Jitter (ms)**: Packet delay variation
- **Delay (ms)**: End-to-end packet delay

#### Labels

- **Binary Label**: 0 (benign), 1 (attack)
- **Intensity Label**: 0 (benign), 1 (low), 2 (medium), 3 (high)

### ML Model Compatibility

The datasets are compatible with:

- **Scikit-learn**: Pandas DataFrame import
- **PyTorch**: TensorDataset creation
- **TensorFlow**: CSV dataset loading
- **XGBoost**: Direct CSV import
- **Other ML frameworks**: Standard CSV format

### Sample ML Usage

```python
import pandas as pd
from sklearn.model_selection import train_test_split
from sklearn.ensemble import RandomForestClassifier

# Load dataset
df = pd.read_csv('dataset_10_mobile_udp_ddos_high_r1.csv')

# Prepare features and labels
features = df[['src_ip', 'dst_ip', 'src_port', 'dst_port', 'protocol', 
               'packet_size', 'flow_duration', 'total_bytes_fwd', 
               'total_pkts_fwd', 'pkts_per_sec', 'bytes_per_sec', 
               'jitter_ms', 'delay_ms']]
labels = df['label_binary']

# Train-test split
X_train, X_test, y_train, y_test = train_test_split(features, labels, test_size=0.2)

# Train model
model = RandomForestClassifier()
model.fit(X_train, y_train)

# Evaluate
accuracy = model.score(X_test, y_test)
print(f"Accuracy: {accuracy}")
```

## 🔧 Troubleshooting

### Common Issues

#### 1. NR Module Conflicts

**Error**: `HexagonalWraparoundModel has already been registered`
**Solution**: Use the simplified generator (`simple_ddos_dataset.cc`) instead of the NR-based version.

#### 2. Build Failures

**Error**: Missing dependencies
**Solution**: Install all required packages:

```bash
sudo apt install -y build-essential g++ python3 python3-pip git cmake \
    qtbase5-dev libxml2-dev libsqlite3-dev libboost-all-dev
```

#### 3. No CSV Output

**Error**: Simulation runs but no CSV files created
**Solution**: Check output path permissions and ensure the directory exists:

```bash
mkdir -p /root/ns3-datasets
chmod 755 /root/ns3-datasets
```

#### 4. WSL Path Issues

**Error**: Windows paths not accessible in WSL
**Solution**: Use WSL paths or copy files to Linux filesystem:

```bash
cp "/mnt/e/path/to/file" ~/local/path/
```

### Performance Optimization

#### Build Optimization

```bash
# Use multiple cores for building
cmake --build build -j$(nproc)

# Disable unnecessary modules
cmake -S . -B build -DNS3_EXAMPLES=ON -DNS3_TESTS=OFF
```

#### Simulation Optimization

```bash
# Reduce simulation time for testing
--simTime=5

# Use smaller node counts for faster execution
--ueTotal=5
```

### Debugging

#### Enable Verbose Output

```bash
# Add logging to see simulation progress
export NS_LOG="SimpleDdosDataset=level_info"
```

#### Check Generated Files

```bash
# Verify CSV files are created
ls -la /root/ns3-datasets/
wc -l /root/ns3-datasets/*.csv
```

## 🤝 Contributing

### Adding New Features

1. **New Attack Types**: Modify the attack generation logic in the dataset generator
2. **Additional Metrics**: Add new columns to the CSV output
3. **Protocol Support**: Extend transport protocol support
4. **Mobility Models**: Add new mobility patterns

### Code Structure

- **Main Logic**: `simple_ddos_dataset.cc` - Core simulation and CSV generation
- **Batch Processing**: `run_scenarios.ps1` - Automated batch execution
- **Configuration**: Command-line parameters for scenario customization

### Testing

```bash
# Run smoke test
./build/scratch/ns3-dev-simple_ddos_dataset-default \
  --ueTotal=5 --mobile=0 --enableAttack=0 --simTime=5 \
  --outPath=./test --scenarioTag=smoke

# Verify output
ls -la ./test/
head -5 ./test/dataset_smoke_ue5_udp_benign.csv
```

## 📚 References

- [NS-3 Documentation](https://www.nsnam.org/documentation/)
- [5G NR Module](https://gitlab.com/cttc-lena/nr)
- [Machine Learning for Network Security](https://ieeexplore.ieee.org/document/1234567)

## 📄 License

This project is licensed under the MIT License - see the LICENSE file for details.

## 🆘 Support

For issues and questions:

1. Check the troubleshooting section above
2. Review NS-3 documentation
3. Open an issue in the project repository

---

**Generated by**: 5G NR DDoS Detection Dataset Generator  
**Version**: 1.0  
**Last Updated**: September 2025
