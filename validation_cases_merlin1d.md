Validation Cases for Propulsion Model Validation
Reference Engine: Merlin 1D (Sea-Level)

1. Introduction
This document defines the validation cases used to verify the accuracy of the propulsion model developed in this project. The validation is based on publicly available performance data of the Merlin 1D engine. Each case specifies the input parameters, output variables, expected reference values, and data sources.

2. Validation Case 1: Sea-Level Thrust
Input Conditions
* Chamber pressure
* Mixture ratio (LOX/RP-1)
* Ambient pressure (sea-level conditions)
Output Parameter
* Sea-level thrust
Expected Value
* 845,000 N
Reference Source
* SpaceX Falcon 9 User Guide

3. Validation Case 2: Vacuum Specific Impulse
Input Conditions
* Nozzle expansion characteristics
* Near-vacuum ambient pressure
Output Parameter
* Vacuum specific impulse
Expected Value
* 311 s
Reference Source
* SpaceX Falcon 9 User Guide

4. Validation Case 3: Propellant Mass Flow Rate
Input Conditions
* Sea-level thrust
* Sea-level specific impulse
* Standard gravitational acceleration
Output Parameter
* Propellant mass flow rate
Expected Value
* Approximately 305 kg/s
Reference Source
* Calculated using the classical thrust equation
* Sutton & Biblarz, Rocket Propulsion Elements

5. Notes
The expected values used in these validation cases are stored in the reference data file included in the project repository. Depending on the validation scenario, parameters may act as inputs or outputs. This approach allows flexibility in evaluating different aspects of the propulsion model.
Principio del formulario
Final del formulario

