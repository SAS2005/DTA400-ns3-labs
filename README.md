# DTA400 – ns-3 Lab Assignments

This repository contains the ns-3 lab assignments for the course
DTA400 – Advanced Topics in Computer Engineering.

## Lab 1 – Car Wash

The first lab studies a car wash simulation and investigates how the
system can become overloaded.

The main parameters are:

- `lambda` – arrival rate of cars
- `mu` – service rate per server
- `c` – number of parallel servers

Three ways to overload the system were tested:

1. Increase the arrival rate (`lambda`)
2. Decrease the service rate (`mu`)
3. Decrease the number of servers (`c`)

Example:

```bash
./ns3 run scratch/carwash
./ns3 run "scratch/carwash --lambda=1.3"
./ns3 run "scratch/carwash --mu=0.4"
./ns3 run "scratch/carwash --c=1"

