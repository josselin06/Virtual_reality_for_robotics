\# Cosys-AirSim — SimMode "Both" (UAV + UGV)



This repository contains modifications to the Cosys-AirSim Unreal plugin to support a heterogeneous simulation

with a multirotor UAV and a ground vehicle UGV running simultaneously in the same Unreal Engine instance.



\## Features

\- SimMode = "Both" (heterogeneous SimMode)

\- Dual RPC servers:

&nbsp; - Multirotor RPC on port 41451

&nbsp; - Car/Rover RPC on port 41452 (ApiServerPort + 1)

\- Physics/tick separation to avoid interference between AirLib PhysicsWorld (UAV) and Unreal vehicle physics (UGV)

\- Python test scripts for:

&nbsp; - concurrent control

&nbsp; - data exchange (drone follows rover waypoints)

&nbsp; - exploratory LiDAR experiments



\## How to run

1\. Build the Unreal environment with the modified AirSim plugin.

2\. Configure `settings.json` with:

&nbsp;  - `"SimMode": "Both"`

&nbsp;  - `"ApiServerPort": 41451`

&nbsp;  - vehicles `Drone1` (SimpleFlight) and `Rover1` (PhysXCar/Skid)

3\. Start the Unreal Editor, open the environment, press Play.

4\. Run Python scripts.





