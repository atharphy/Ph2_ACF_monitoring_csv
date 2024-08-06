# Crosstalk (X-Talk) studies

The user can define its own enable and injection patterns (`INJtype = 3`) [link](../ConfigFile.md#injection) and then
run an `SCurve` scan to measure possible X-talk

![Scurves](images/xtalk.png){ width=600 }

## Mask generation

Python program to generate enable/injection patterns for x-talk studies: `pyUtilsIT/ManipulateITchipMask.py`

* User can pass enable/injection pattern from le, e.g.:  
   `row 0 col 130 en`  
   `row 1 col 130 inj`  
   `row 2 col 130 en` ...
* User can specify standard injection pattern and read
adjacent <span style="color:red">coupled</span> or <span style="color:green">decoupled</span> pixel

![Scurves](images/xtalk_python.png){ width=400 }
