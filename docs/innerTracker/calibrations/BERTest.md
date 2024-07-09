# Bit Error Rate test

**Scan command:** `bertest`

* BE-FE (i.e. backend-frontend) means that you are running the test between the FC7 and the RD53
* BE-LPGBT (i.e. backend-LpGBT) means that you are running the test between the FC7 and the LpGBT
* LPGBT-FE (i.e. LpGBT-frontend) means that you are running the test between the LpGBT and the RD53

N.B.: the BER counter returns the number of frames with error(s)

## Configuration parameters

| Name             | Typical value | Description |
| ---------------- | ------------- | ----------- |
| `chain2Test`     |               | 0 = BE-FE; 1 = BE-LPGBT; 2 = LPGBT-FE |
| `byTime`         |               | Defines meaning of `framesORtime`: 0 = no. of frames; 1 = time in [s]a |
| `framesORtime`   |               | duration of run: time in [s] or number of frames (see `byTime`) |

## Typical output

![Setup](images/bert.png){ width=600 }
