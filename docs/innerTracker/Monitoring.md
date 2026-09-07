# Monitoring

To enable the periodic readout of quantities like voltages, currents and temperatures, you need to enable the monitoring process via these lines in the configuration file:

```xml
<MonitoringSettings>
    <Monitoring type="RD53B" enable="1" silentRunning="0">
      <MonitoringSleepTime> 1000 </MonitoringSleepTime>
      <MonitoringElement device="RD53"  register="VINA"                 enable="1"/>
      <MonitoringElement device="RD53"  register="VDDA"                 enable="1"/>
      <MonitoringElement device="RD53"  register="ANA_IN_CURR"          enable="1"/>
      <MonitoringElement device="RD53"  register="VIND"                 enable="1"/>
      <MonitoringElement device="RD53"  register="VDDD"                 enable="1"/>
      <MonitoringElement device="RD53"  register="DIG_IN_CURR"          enable="1"/>
      <MonitoringElement device="RD53"  register="Iref"                 enable="1"/>
      <MonitoringElement device="RD53"  register="POLY_TEMPSENS_TOP"    enable="1"/>
      <MonitoringElement device="RD53"  register="POLY_TEMPSENS_BOTTOM" enable="1"/>
      <MonitoringElement device="RD53"  register="TEMPSENS_ANA_SLDO"    enable="1"/>
      <MonitoringElement device="RD53"  register="TEMPSENS_DIG_SLDO"    enable="1"/>
      <MonitoringElement device="RD53"  register="TEMPSENS_CENTER"      enable="1"/>
      <MonitoringElement device="RD53"  register="INTERNAL_NTC"         enable="1"/>

      <MonitoringElement device="LpGBT" register="ADC4"                 enable="0"/>
      <MonitoringElement device="LpGBT" register="ADC7"                 enable="0"/>
      <MonitoringElement device="LpGBT" register="TEMP"                 enable="0"/>
      <MonitoringElement device="LpGBT" register="VDDTX"                enable="0"/>
      <MonitoringElement device="LpGBT" register="VDDRX"                enable="0"/>
      <MonitoringElement device="LpGBT" register="VDDA"                 enable="0"/>
      <MonitoringElement device="LpGBT" register="VDD"                  enable="0"/>
      <MonitoringElement device="LpGBT" register="PUSMStatus"           enable="0"/>
    </Monitoring>
  </MonitoringSettings>
```
Here, <code style="color:CornflowerBlue;">MonitoringSleepTime</code> sets the monitoring period in μs.
To avoid monitoring printout on the screen, set <code>silentRunning=<t style="color: MediumSeaGreen;">"1"</t></code>.

## Realtime monitor-only run

To read the enabled monitoring values directly from the configured devices without running physics data-taking or another calibration loop, use the `realtimemonitor` calibration:

```bash
CMSITminiDAQ -f your_hw_description_file.xml -c realtimemonitor -t 60
```

The `realtimemonitor` calibration uses the enabled `<MonitoringElement>` entries in the XML. If `-t` is provided, monitoring runs for that many seconds. If `-t` is omitted, monitoring runs until the user presses Enter:

```bash
CMSITminiDAQ -f your_hw_description_file.xml -c realtimemonitor
```

## CSV monitoring and Prometheus export

For RD53 systems, enabling XML monitoring writes corrected physical and virtual register values to durable CSV files. Ph2_ACF does not open an HTTP port. Its deployment-specific options are kept outside the hardware XML in:

```text
$PH2ACF_BASE_DIR/settings/monitoring_settings.conf
```

Important settings include:

```ini
enabled = true
csv_output_directory = monitoring_csv
csv_rotate_size_mb = 100
csv_rotate_minutes = 60
csv_include_errors = true
csv_register_allowlist = *
virtual_register_config = settings/virtual_registers.conf
```

Files are placed in a `monitoring_csv` directory beside the hardware XML by default and use the form:

```text
calibration[_run]_YYYYMMDD_HHMMSS.csv
```

The run component is omitted when no run number is assigned. Rotated files receive a `_partNNN` suffix. Every row identifies the board, optical group, hybrid, chip, eFuse, module, date, and time. Physical and virtual registers are columns. Module-scope virtual values are repeated on the chip rows belonging to that module.

### DCA lookup

Run the standalone DCA lookup before starting CMSITminiDAQ. Ph2_ACF does not connect to or authenticate with DCA; it only reads the generated eFuse-to-module CSV. With `dca_mapping_file = auto`, the expected file is `<xml-basename>.csv` beside the hardware XML. `dca_mapping_failure_policy` accepts `warn` or `abort`.

The hardware XML must contain the eFuse values obtained for the actual connected chips. Default or copied eFuse values do not provide a reliable detector identity.

### Monitoring windows

`monitor_schedule` accepts:

- `always`: monitor for the complete calibration.
- `percent`: monitor inside `monitor_percent_windows`, for example `0-10,45-55,90-100`, using native RD53 scan progress.

Per-calibration entries use the form `calibration.NAME.monitor_schedule` and `calibration.NAME.monitor_percent_windows`. Percentage mode is intended for scans that populate `RD53RunProgress`; open-ended modes such as `physics` and `realtimemonitor` use `always`. Outside configured windows, the RD53 monitoring cycle is skipped, reducing hardware traffic as well as CSV output.

### External exporter

Start the standalone service from the sibling `prometheus_exporter` directory:

```bash
./run_exporter.sh "$PH2ACF_BASE_DIR/settings/monitoring_settings.conf"
```

It incrementally tails current and rotated CSV files and exposes `cmsit_monitor_value`, `cmsit_monitor_error`, and `cmsit_monitor_last_update_seconds`. Configure Prometheus to scrape it:

```yaml
scrape_configs:
  - job_name: cmsit
    scrape_interval: 1s
    metrics_path: /metrics
    static_configs:
      - targets: ["localhost:9101"]
```

The exporter reads `exporter_listen_address`, `exporter_port`, `exporter_metrics_path`, and `exporter_scan_interval_seconds` from the same configuration file. Set `CMSIT_MONITORING_CONFIG` to select another file for Ph2_ACF:

```bash
export CMSIT_MONITORING_CONFIG=/path/to/monitoring_settings.conf
```

`silentRunning` in the XML controls monitoring log messages only. CSV collection remains active whenever XML monitoring and CSV monitoring are enabled.

## Output plots

Ad-hoc "observable vs time" ROOT historical plots can be produce produced. E.g.:
![LpGBT Eye Opening Diagram](images/Monitoring.png){width=500}

## RD53A monitoring

To readout all quantities of RD53A you need to put the following on your Single Chip Card (SCC):

* 0 Ohm resistor on <t style="color: Red">R51</t> (<t style="color: Blue">R17</t>) to have the right connection on the display port for the "<t style="color: Red">Bonn</t>" card ("<t style="color: Blue">Zurich</t>" card)
* 0 Ohm resistor on <t style="color: Red">R57</t> (<t style="color: Blue">R23</t>) to have proper GND connection for the "<t style="color: Red">Bonn</t>" card ("<t style="color: Blue">Zurich</t>" card)
* 10 kOhm resistor on <t style="color: Red">R45</t> (<t style="color: Blue">R32</t>) for the "<t style="color: Red">Bonn</t>" card ("<t style="color: Blue">Zurich</t>" card)
* Jumper on I-MUX for current read back

See also tables in [RD53A](https://cds.cern.ch/record/2287593) and [RD53B](https://cds.cern.ch/record/2665301) manuals.

## Temperature monitoring

There are two ways to readout the chip POLY temperatures:

* Using the value which depends on the ADC voltage ("`_REL`", faster):  
  ```xml
  <MonitoringSettings>
    <Monitoring type="RD53B" enable="1" silentRunning="0">
      <MonitoringSleepTime> 1000 </MonitoringSleepTime>
      . . .
      <MonitoringElement device="RD53" register="POLY_REL_TEMPSENS_TOP"    enable="1"/>
      <MonitoringElement device="RD53" register="POLY_REL_TEMPSENS_BOTTOM" enable="1"/>
      . . .
  ```
* Using the value independent on the ADC voltage ("`_ABS`", slower):  
  ```xml
      . . .
      <MonitoringElement device="RD53" register="POLY_ABS_TEMPSENS_TOP"    enable="1"/>
      <MonitoringElement device="RD53" register="POLY_ABS_TEMPSENS_BOTTOM" enable="1"/>
      . . .
  ```

There are two ways to readout the chip NTC temperature:

* Using the value which depends on the ADC voltage ("`_REL`", faster):  
  ```xml
  <MonitoringSettings>
    <Monitoring type="RD53B" enable="1" silentRunning="0">
      <MonitoringSleepTime> 1000 </MonitoringSleepTime>
      . . .
      <MonitoringElement device="RD53" register="INTERNAL_NTC_REL" enable="1"/>
      <MonitoringElement device="RD53" register="INTERNAL_NTC_ABS" enable="1"/>
      . . .
  ```
* Using the value independent on the ADC voltage ("`_ABS`", slower):  
  ```xml
      . . .
      <MonitoringElement device="RD53" register="POLY_ABS_TEMPSENS_TOP"    enable="1"/>
      <MonitoringElement device="RD53" register="POLY_ABS_TEMPSENS_BOTTOM" enable="1"/>
      . . .
  ```

## NTC monitoring

### LpGBT ADC

If you intend to readout the NTCs with the LpGBT ADC (see settings that have <code>device=<t style="color: MediumSeaGreen;">"LpGBT"<t></code> above), then you need to provide also the `lookUpTable` for the NTCs, e.g.:
```xml
<OpticalGroup Id="0" enable="1" FMCId="L12">
      <NTCProperties type="Sensor" ADC="ADC4" lookUpTable="${PH2ACF_BASE_DIR}/settings/NTCFiles/ntc_1k.csv"/>
      <NTCProperties type="VTRx"   ADC="ADC7" lookUpTable="${PH2ACF_BASE_DIR}/settings/NTCFiles/vtrx_ntc_1k.csv"/>
      . . .
```

Here:

* <code style="color: MediumSeaGreen;">"Sensor"</code> means that the NTC is near the sensor
* <code style="color: MediumSeaGreen;">"VTRx"</code> means that the NTC is near the VTRx

### Internal ADC
In order to use the internal ADC set the following jumper:
![MUX Jumper](images/MUXjumper.png){width=450}

### NTC1
NTC1 can be measured through the FMC ADC (just by enabling the *Monitoring* block of the DAQ), and it’s used as reference for temperature sensor calibration.
**In order to do so, you need to short the CONF1 and CONF2 jumpers on the back of the board:**
![CONF1/CONF2 Jumpers](images/CONF12.png){width=350}

NTC1 can also be read out through the NTC1 Pins:
![NTC1 Pins](images/NTC1.png){width=500}

### NTC2
NTC2 can be measured through the CROC ADC via the *Monitoring* block of the DAQ, called <code style="color: MediumSeaGreen;">"INTERNAL_NTC_REL"</code> or <code style="color: MediumSeaGreen;">"INTERNAL_NTC_ABS"</code>.

Not strictly necessary:

* One can also verify/calibrate the NTC2 by reading it with an external multimeter through these pins:

![Vmux Pins](images/VmuxPins.png){width=400}

* One can also calibrate the CROC ADC by reading its output by one of the two setups (pins or LEMO, see below) and connect them to a multimeter:

![Vmux Pins](images/VmuxLemo.png){width=450}

## Special registers

The user can control some readout chip-related parameters through special registers:

| RD53A                   | CROCv1/CROCv2               |
| ----------------------- | --------------------------- |
| `RESISTORI2V`           | `RESISTORI2V`               |
|                         | `REFTEMP`                   |
|                         | `RES_MEAS_TOP`              |
|                         | `RES_MEAS_BOTTOM`           |
| `NTCBETA`               | `NTCBETA`                   |
|                         | `RNTCAT25C`                 |
| `ADC_OFFSET_VOLT`       | `ADC_OFFSET_VOLT`           |
| `ADC_MAXIMUM_VOLT`      | `ADC_MAXIMUM_VOLT`          |
| `TEMPSENS_IDEAL_FACTOR` | `TEMPSENS_IDEAL_FACTOR`     |
|                         | `TEMPSENS_IDEAL_FACTOR_ANA` |
|                         | `TEMPSENS_IDEAL_FACTOR_DIG` |
|                         | `RADSENS_IDEAL_FACTOR`      |
|                         | `RADSENS_IDEAL_FACTOR_ANA`  |
|                         | `RADSENS_IDEAL_FACTOR_DIG`  |
|                         | `TEMPSENS_OFFSET_TOP`       |
|                         | `TEMPSENS_OFFSET_BOTTOM`    |
| `SAMPLE_N_TIMES`        | `SAMPLE_N_TIMES` (sample observable `N` times) |
|                         | `SAMPLE_NTC_SLOPE` (used to estimate `INTERNAL_NTC_ABS` slope) |
|                         | `WAIT_MUX_CONFIG`           |
| `VREF_ADC`              | `VREF_ADC`                  |
| `INJ_CAP`               | `INJ_CAP`                   |

## CROC Auto-Read feature

The RD53B (CROC) chip has Auto-Read features, basically the user can monitor the content of the chip registers.
To enable this feature the user has to:

* Write in one of the `AutoRead` registers, e.g. `AutoRead0`, the address the register that you want to monitor, in the XML, e.g.:  
  ```xml
  <Settings
    AutoRead0 = "0x86"   ➜ address of BCID_CNT register
  />
  ```
* Write in the `MonitoringSettings` section of the XML, the register's name that you want to measure, and add "`_AUTORA/B`" (`A` or `B` depends on the `AutoReadX` that you choose, see [RD53 chip manual](https://cds.cern.ch/record/2890222)), e.g.:  
  ```xml
  <MonitoringSettings>
    <Monitoring type="RD53B" enable="1" silentRunning="0">
      <MonitoringElement device="RD53" register="BCID_CNT_AUTORA" enable="1"/>
  ```
