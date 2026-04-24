# Crosstalk (X-Talk) Studies

The user can define its own <u>enable</u> and <u>injection</u> patterns (`INJtype = 4`) [link](../ConfigFile.md#injection) and then
run an [`SCurve` scan](SCurve.md) to measure possible X-talk

![Scurves](images/xtalk/xtalk.png){width=600}

## Mask generation

Python program to generate enable/injection patterns for x-talk studies: [`pythonUtils/pyUtilsIT/ManipulateITchipMask.py`](https://gitlab.cern.ch/cms_tk_ph2/Ph2_ACF/-/blob/Dev/pythonUtils/pyUtilsIT/ManipulateITchipMask.py?ref_type=heads)

* User can pass enable/injection pattern, e.g.:
  ```
  row 0 col 130 en
  row 1 col 130 inj
  row 2 col 130 en
  
  row 0 col 129 en
  row 1 col 129 en 
  row 2 col 129 en

  row 0 col 131 en
  row 1 col 131 en
  row 2 col 131 en
  ```
  Users can also specify the group number, i.e. pattern number among the several possible `(0, NROWS - 1)`.

* User can specify standard injection pattern and read adjacent <span style="color:red"><u>coupled</u> (`INJtype` = 5)</span> or <span style="color:green"><u>decoupled</u> (`INJtype` = 6)</span> pixel

![Scurves](images/xtalk/xtalk_python.png){width=400}

Another possibility is to use [PixelAlive](PixelAlive.md) for X-talk studies. More details can be found [here](PixelAlive.md#crosstalk-x-talk-test).
