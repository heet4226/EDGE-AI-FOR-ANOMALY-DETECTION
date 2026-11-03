# Dataset

The raw recordings are **not published**. This page describes the data so that the code and results can be understood, and so that a compatible dataset can be recorded. Only aggregate statistics are shared (`../results/dataset_stats.json`).

## Overview

| | |
|---|---|
| Recording sessions | 60 (one file per session) |
| Feature vectors | 239,367 |
| Features per vector | 14 |
| Feature rate | 66.67 Hz (one vector every 15 ms) |
| Model input | 30 × 14 window (450 ms), step 10 vectors (150 ms) |
| Classes | 7: 4 operating states + 3 fault states |
| Extra test recordings | 7 sessions never used for training, plus one with repeated switching |

## Sessions per class

| Class | Sessions | Feature vectors | Ramp profiles | Direction | Condition |
|---|---:|---:|---|---|---|
| Motor off | 8 | 40,734 | 0.1 / 0.2 / 0.5 / 1.0 s | both | supply on and off |
| Turning off | 8 | 15,101 | 0.1 / 0.2 / 0.5 / 1.0 s | both | – |
| Turning on | 8 | 8,760 | 0.1 / 0.2 / 0.5 / 1.0 s | both | – |
| Normal | 12 | 119,359 | 0.1 / 0.2 / 0.5 / 1.0 s | both | with load (8), without load (4) |
| Electrical fault | 8 | 25,503 | 0.1 / 0.2 / 0.5 / 1.0 s | both | with load |
| Mechanical fault | 8 | 9,519 | 0.2 / 0.5 / 1.0 s | both | with load |
| Thermal fault | 8 | 20,391 | 0.1 / 0.2 / 0.5 / 1.0 s | both | – |

## Sensors and sampling

| Source | Rate | Used for |
|---|---|---|
| INA260 current and voltage (I²C) | 200 Hz | electrical features |
| LIS3DH accelerometer (SPI) | 200 Hz | vibration statistics |
| DS18B20 temperature (1-Wire) | ≈ 1.25 Hz | temperature slope (the last valid value is held) |
| PWM command | per sample | operating context |

## The 14 features

| # | Feature | Source | Definition | What it captures |
|---:|---|---|---|---|
| 1 | `T_slope_10s` | DS18B20 | [T(t) − T(t − 10 s)] / 10 s | active heating, not absolute temperature |
| 2 | `PWM` | controller | duty cycle, 0–1 | operating point; off, start and stop regions |
| 3 | `Irms` | INA260 | √(mean i²) | load and torque demand |
| 4 | `Vrms` | INA260 | √(mean v²) | supply present or absent; voltage droop |
| 5 | `Current_Variance` | INA260 | variance of i | unstable commutation, fluctuating load |
| 6 | `Current_Crest_Factor` | INA260 | max\|i\| / Irms | spikes and impulsive events |
| 7 | `Vib_RMS` | LIS3DH | √(mean a²) | overall vibration energy |
| 8–12 | `Vib_Min`, `Vib_Q1`, `Vib_Median`, `Vib_Q3`, `Vib_Max` | LIS3DH | order statistics | shape and asymmetry of the vibration |
| 13 | `Vib_Variance` | LIS3DH | variance of a | looseness, imbalance |
| 14 | `Vib_Skewness` | LIS3DH | third standardized moment | impacts in one direction |

## Feature analysis

How the data was explored before modelling. Click any image to enlarge it.

<a href="figures/dataset_overview.png"><img src="figures/dataset_overview.png" width="49%" alt="From recordings to model windows"></a>
<a href="figures/pipeline_steps.png"><img src="figures/pipeline_steps.png" width="49%" alt="Acquire, clean, extract, package"></a>

**Each fault leaves a clear signature in at least one feature:**

<a href="figures/thermal_vs_normal.png"><img src="figures/thermal_vs_normal.png" width="32%" alt="Thermal fault: temperature slope"></a>
<a href="figures/electrical_vs_normal.png"><img src="figures/electrical_vs_normal.png" width="32%" alt="Electrical fault: RMS current"></a>
<a href="figures/mechanical_vs_normal.png"><img src="figures/mechanical_vs_normal.png" width="32%" alt="Mechanical fault: vibration RMS"></a>

**Which features carry the information:**

<a href="figures/fisher_scores.png"><img src="figures/fisher_scores.png" width="49%" alt="Fisher score per feature"></a>
<a href="figures/feature_importance.png"><img src="figures/feature_importance.png" width="49%" alt="Random forest and permutation importance"></a>
<a href="figures/feature_correlation.png"><img src="figures/feature_correlation.png" width="49%" alt="Feature correlation and redundancy"></a>
<a href="figures/seven_feature_subset.png"><img src="figures/seven_feature_subset.png" width="49%" alt="Seven features match the full set"></a>

## Expected layout (to run the notebooks)

```
data/
├── Motor_data_model/      training sessions, one .xlsx per session, grouped by class folder
│   ├── normal/  motor_off/  Electrical fault/  Mechanical fault/  ...
└── Test_data/             separate test recordings
```

Each file has one row per feature vector, with the 14 feature columns above plus `timestamp`, `ramp_tag` and `direction`. Values are stored as integers scaled by 10⁶.
