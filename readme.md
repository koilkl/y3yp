# 🥾 Next-Generation Ankle Rehabilitation System (Actuation Belt Trainer)

Welcome to the official repository for the **Next-Generation Ankle Rehabilitation System**, a biomechanically engineered solution designed to build structural resilience and prevent trail-induced lower-limb injuries. 

## 👥 Meet the Team
**Hikingggggggg(HKUST 2026 ISDN3001&ISDN3002 Group 1)**
* Teammate:Kwok Tsz Yueng
  * Kwok Tsz Yueng(Tom)
  * Li,Kaile(Koil)
  * Lee Wang Ngai(Cyrus)
  * Yu Kin Shing(Ken)

We are a team of passionate engineers and biomechanics enthusiasts dedicated to bridging the gap between clinical rehabilitation and real-world outdoor sports readiness.

---

## 🏔️ Project Overview

### The Problem: The Hiking Injury Epidemic
With the massive global expansion of hiking participation, there has been a severe spike in trail-induced injuries. 
* **Acute Lateral Sprains** account for an overwhelming 85% of all acute trail traumas.
* In competitive downhill races, lower-limb injury rates spike dramatically to 49%.
* Hikers face a **"Deficit in Terrain Adaptability"**; modern flat surfaces fail to prepare the body for the eccentric braking and instantaneous balance reflexes required on unpredictable mountain terrains (like loose rock and scree).

When a sudden, forceful ankle inversion exceeds 25 degrees, it tears the Anterior Talofibular and Calcaneofibular ligaments. 

### The Solution: The Actuation Belt Trainer
Existing market solutions force a compromise: balance boards train reflexes but lack heavy external load, while clinical dynamometers are expensive and lack multi-axis functional movement. 

After multiple ideation phases (including dynamic treadmills and linear pedal trainers), we finalized the **Actuation Belt Trainer**. This active, stationary device isolates the protective reflex arc by applying changeable, programmable external loads to train specific muscle reaction times across 3 Degrees of Freedom (DOFs).

---

## ⚙️ System Architecture

Our system integrates responsive hardware with a closed-loop software control mechanism to ensure precise resistance and user safety.

### Control Loop Workflow
1. **Command Input:** Users input commands via a terminal to wirelessly control motor force and turning direction for specific muscle targeting.
2. **Processing & Communication:** A Microcontroller Unit (MCU) processes the command using a PID controller and sends signals via a CAN Module to 6 Motor Electronic Speed Controllers (ESCs).
3. **IMU Feedback:** An Inertial Measurement Unit (IMU) under the pedal continuously detects the XYZ angle of the pedal.
4. **Evaluation & Visual Feedback:** The MCU calculates the control error against the target origin angle. If the user successfully holds the pedal within a +/- 10-degree range against the motor resistance for 5 seconds, an LED indicator triggers to signal successful completion.

---

## 🛠️ Component Design

The mechanical structure relies on motor-driven belts and gears to safely and effectively stimulate the ankle:

* **Roll Mechanism (Inversion/Eversion):** Four M3508 motors utilize a 1:1 gear transmission to transfer torque to a pulley system. This produces tension along an elastic belt connected to the pedal, creating a twisting motion that the user must resist.
* **Pitch Mechanism (Dorsiflexion/Plantarflexion):** A motor and gear turn the entire middle module and the central pedal, supported by a large external bearing for smooth turning. A limited angle of gear teeth physically restricts motion to guarantee it remains within an acceptable, safe anatomical range.

---

## 🔬 Biomechanical Validation

The system targets the biological failure points of the ankle by isolating the primary protective muscles:
* **Fibularis (Peroneal) Group:** Primary evertors critical for resisting sudden inward ankle rolling (inversion) on shifting scree.
* **Tibialis Group:** Crucial for dorsiflexion, arch support, and absorbing repeated impact with controlled eccentric braking during steep descents.

By engineering a device that challenges the ankle with resisted eversion and perturbation, the system is theoretically proven to improve strength and balance deficits.

---

## 🚀 Future Roadmap

* **Weeks 1-3:** Develop a dedicated mobile app for user control, design a "hiking-like" training mode, and improve mechanism functionalities.
* **Weeks 4-6:** Design pedals to fit different foot sizes, replace electronics to minimize overall device size, and refine the product outfit.
* **Weeks 6-7:** Conduct extensive user testing, identify potential new user groups, and perform evaluations based on feedback.
* **Weeks 7-8:** Design new features and establish a future plan for product line depth to fit various target demographics.

---

## 📂 Repository Structure

> The firmware projects under `ELEC/` are **git submodules** — clone with
> `git clone --recursive https://github.com/koilkl/y3yp.git` to fetch them.

```
y3yp/
├── DOCUMENT/                 # Presentations & demo video
│   └── Version-1.0/
│       ├── Midterm Presentation.pptx
│       ├── Final Presentation.pdf
│       └── demo video.mp4
├── ELEC/                     # Electronics & firmware
│   ├── Version-1.0/
│   │   └── Year_3_first_prototype/   # submodule: STM32F103C6T6, CAN motors + IMU + PID
│   └── Component/
│       └── Incliometer/              # submodule: STM32F401RETx, Modbus-RTU inclinometer
├── MECH/                     # Mechanical design (CAD)
│   └── Version-1.0/
│       └── Gen1-belt.zip             # Belt-actuation assembly (STEP)
└── readme.md                 # This file
```

## 📁 Resources & Appendices

* **CAD Files & 3D Renders:** [`MECH/Version-1.0/Gen1-belt.zip`](MECH/Version-1.0/Gen1-belt.zip)
* **Source Code (First Prototype):** [`yuu033/Year_3_first_prototype`](https://github.com/yuu033/Year_3_first_prototype) (submodule at [`ELEC/Version-1.0/Year_3_first_prototype/`](ELEC/Version-1.0/))
* **Source Code (Inclinometer Component):** [`koilkl/Incliometer`](https://github.com/koilkl/Incliometer) (submodule at [`ELEC/Component/Incliometer/`](ELEC/Component/))
* **Presentations:** [`DOCUMENT/Version-1.0/`](DOCUMENT/Version-1.0/README.md)
* **Prototype Demo Video:** [`DOCUMENT/Version-1.0/demo video.mp4`](DOCUMENT/Version-1.0/demo%20video.mp4)