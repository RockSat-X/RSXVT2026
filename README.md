# Virginia Tech RockSat-X 2026 Project.

## Action Items.

- [ ] Onboarding.
    - [ ] All members finishing "Onboarding the Git Workflow".

[^us-persons]: ``(@/pg 7/sec 1.2/`RSX`)``.

- [ ] Design.
  - [ ] Put all design, build, and test action items into months.
  - [ ] Easy-access RBF inhibits.
  - [ ] Debug interface.
    - [ ] Power (e.g. GSE, TE-1, TE-2, etc.).
  - [ ] Main material list (one system of measurement).
  - [ ] Mechanical test beds.
  - [ ] Finite element analysis.
  - [ ] Consider how RF will be tested (GPS rollout)
  - [ ] Determine the on and dwell times for TEs.
  - [ ] Verify independent batteries (and their chargers) to be UL-listed.

- [ ] Build.
  - [ ] 3D printed plates for PCB solder stencils.
  - [ ] Full 3D printed payload.
  - [ ] Aluminium/plywood mock-up of deck plate and experiment.
  - [ ] Integration of function indicators (lights, buzzers, etc.).
  - [ ] Stickers/tags/markings to indicate purpose for each inhibit.
  - [ ] Locktight/hotglue/secure fasteners to prepare for vibe-testing.

- [ ] Test.
  - [ ] Build test beds and run actuation testing.
  - [ ] Full sequence test.
    - [ ] Lightning test (no GSE).
  - [ ] GPS rollout.
  - [ ] Vibration test.
  - [ ] Summer testing preparations.
    - [ ] Inventory sheet.
    - [ ] Spreadsheet of fulltime of timer events, current draw, indicators, etc.
    - [ ] Full walk around inspection.
    - [ ] Procedures checksheets.
    - [ ] Personnel briefing.

- [ ] September.
  - [ ] (**est. 21st?**) Intent-to-fly forms (IFF).
  - [ ] (**est. ???**) Interface Control Documents (ICDs).

- [ ] October.
  - [ ] (**est. ???**) Update ICDs.
  - [ ] (**est. 9th-13th?**) Conceptual Design Review (CoDR).

- [ ] November.
  - [ ] (**est. ???**) Finalize ICDs.
  - [ ] (**est. ???**) Frequency Utilization Request (FDR).
  - [ ] (**est. 13th-17th?**) Preliminary Design Review (PDR).
  - [ ] (**est. 28th?**) Down-selection.

- [ ] December.
  - [ ] (**est. 4th-15th?**) Critical Design Review (CDR).

- [ ] January.

- [ ] Feburary.
  - [ ] (**est. 19th-23rd?**) Subsystem Testing Review (STR).

- [ ] March.
  - [ ] (**est. ???**) Delivery of deckplate, 37-pin female connector, and 15-pin male connector.
  - [ ] (**est. 18th-22nd?**) Integrated Subsystem Testing Review (ISTR).

- [ ] April.
  - [ ] (**est. 22nd-26th?**) Full Mission Simulation Review (FMSR).

- [ ] May.
  - [ ] (**est. 28th?**) Visual Verification Check-in (VVC).

## RockSat-X User Guide Summary.

As of writing, we will be referring to [August 31, 2023 (Rev - Draft)](https://www.nasa.gov/wp-content/uploads/2022/09/rocksat-x-user-guide-2024.pdf) version of the RockSat-X user guide.
Here are some important things to consider:

  - Electrical specifications:

    - The following powerlines are provided:

      - GSE-1 / GSE-2.
        - "Ground Station Equipment".
        - Should be used for experiment initialization.
        - Enabled pre-launch between T-600s to T-180s of our choosing.
        - Consider possibility of power-cycling. :small_red_triangle:
        - Consider possibility of no GSE provided due to lightning strike. :small_red_triangle:
        - Cannot be used for any deployments nor RF. :small_red_triangle:
        - Cutoff at about T+332.
        - Each has polyswitch fuse rated for 1.85A. :small_red_triangle:

      - TE-1 / TE-2 / TE-3.
        - "Non-Redundant Timer Events".
        - Cutoff at about T+332.
        - Each has polyswitch fuse rated for 3.75A. :small_red_triangle:

      - TE-RA / TE-RB.
        - "Redundant Timer Events".
        - Both enabled at the same time.
        - Otherwise, same characteristics as the non-redundant TEs.

    - GSEs and TEs powered by a single 1Ah battery at 28±6 V provided by the rocket. :small_red_triangle:
    - The battery provided by the rocket has max current draw of 3.75A. :small_red_triangle:

    - Additional batteries are allowed.
      - Battery and charger must be UL-listed and approved. :small_red_triangle:
      - Battery can be a rechargable lithium, but it must be charged off-site. :small_red_triangle:

    - Wireless communication is allowed.
      - No RF system should be enabled via GSE. :small_red_triangle:
      - RF system should be verifiable for GPS roll-out. :small_red_triangle:

    - Telemetry provided by the rocket includes:
      - Ten 10-bit 5V ADCs operating at 1KHz.
      - One 19200 baud UART using 8N1 format.[^telemetry-baud-speed]
      - One 16-bit parallel interface.[^parallel-interface]

  - Mechanical specifications.
    - Weight budget of 30±1 lb (13.6±0.5 kg). :small_red_triangle:
    - Height budget of 10.75 in (27.3 cm). :small_red_triangle:
    - Center of gravity within 1 in (2.5 cm) of deck-plate's normal. :small_red_triangle:
    - Withstand ~25 G in all directions with impulses of ~50 G in the thrust axis. :small_red_triangle:
    - Maximum deployment speed of 1 in/s (2.5 cm/s). :small_red_triangle:
    - Speculated reentry temperature of at least 500 °F (260 °C).
    - Experiment will be subjected to vibration testing. :small_red_triangle:
      - Thrust axis sweeping between 10Hz and 144Hz up to 3 in/s (7.6 cm/s).
      - Thrust axis sweeping between 144Hz and 2000Hz of 7G at 4 oct/min.

[^te-eg]: For example, "TE1 set to T+30s" would mean Timer-Event powerlines 1 is enabled 30 seconds after launch. There are three non-redundant TEs: TE-1, TE-2, and TE-3; there is a redundant pair of TE-RA and TE-RB which are enabled at the same time.
[^gse-eg]: For example, "GSE set to T-180s" would mean the GSE powerlines is enabled 180 seconds before launch. The timestamp should be between T-600s to T-180s.
[^telemetry-baud-speed]: The baud speed has been proposed to being increased before in the past if all other experiments on the rocket are capable of handling it.
[^parallel-interface]: Sample rate is defined in the auxilary document "RS-X Telemetry ICD".

> [!NOTE]
> Bullets marked with :small_red_triangle:
> are to be checked off as :small_blue_diamond:
> once we have verified our experiment implementation satisfies or doesn't conflict with it.
> This should be done when minimal hardware changes are to be made.
