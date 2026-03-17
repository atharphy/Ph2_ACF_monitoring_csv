# Chip self-trigger

The CROC chip has an ability to self-trigger based on the HitOr bit value of the whole pixel matrix.
Contary to [triggering on the HitOr bit externally](ExternalTriggers.md#triggering-on-the-croc-hitor) from the FMC card, the self-trigger requires no additional DP cable connections, and should work with modules as well as SCCs.

To use the self-trigger, one has to set the following settings in the XML file:
```xml
<Settings
    SelfTriggerEn = "1"
    SelfTriggerDelay = "30"
    SelfTriggerMultiplier = "10"
    SelfTriggerDeadTime = "0"
    EnOutputDataChipId = "0"
    HitOrPatternLUT = "0xFFFE"
    HIT_SAMPLE_MODE = "0"

    HITOR_MASK_0 = "0"
    HITOR_MASK_1 = "0"
    HITOR_MASK_2 = "0"
    HITOR_MASK_3 = "0"
/>
```

Here:

* `SelfTriggerEn` enables the self trigger.
* `SelfTriggerDelay` sets the wait time in bunch crossings between the observation of a positive HitOr bit and actually sending a trigger.
* `SelfTriggerMultiplier` is similar to `nTRIGxEvent` and sets how many consecutive triggers should be sent after receiving positive HitOr.
* `SelfTriggerDeadTime` allows setting a wait time in bunch crossings after triggering, during which no new HitOr triggers are accepted (can be useful if you don't want your readout of consecutive bunch crossings (set by `SelfTriggerMultiplier`) to overlap with the next trigger sequence).
* `EnOutputDataChipId` sets whether the the Chip ID should be output in the Aurora 64-bit block.
* `HitOrPatternLUT` is a 16-bit number where each bit represents a unique combination of the four HitOr lanes, `0xFFFE` meaning an OR of all combinations.
* `HIT_SAMPLE_MODE` switches between synchronous (=1) and asynchronous (=0) hit sampling modes.
* The `HITOR_MASK_{0..3}` setting has to be set to to **0** for all core columns that you want to **activate**.

!!! info "The HitOr is by definition **asynchronous**."
    Noise masking/Physics should only done in the asynchronous mode (`HIT_SAMPLE_MODE = "0"`).

Moreover, the following registers and calibration settings have to be set to the given values:
```xml
<Register name="fast_cmd_reg_2">
    <Register name="trigger_source"> 2 </Register>
</Register>
<Register name="Aurora_block">
    <Register name="self_trigger_en"> 1 </Register>
</Register>
<Setting name="INJtype"> 3 </Setting>
```

Finally, all `HITBUS` values have to be set to 1 in the txt file.