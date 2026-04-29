# Virginia Tech RockSat-X 2026 Project.

> [!IMPORTANT]
> [Please check out the wiki!](https://github.com/RockSat-X/RSXVT2026/wiki)



# Mission Overview.

> [!CAUTION]
> Incomplete.



# System Overview.

> [!CAUTION]
> Incomplete.

<p align="center">
<kbd>
<img src="./misc/media/labeled_cad_experiment.png" width="600px">
<br>
<br>
<em>Labeled CAD model of the experiment.</em>
<br>
<br>
</kbd>
</p>&nbsp;



### Debug Board.

The Debug Board has a display to indicate the status of subsystems
from the perspective of the Main Flight Computer.
When the experiment is powered on via GSE-1,
the Debug Board will initialize and play the Nokia buzzer tune.
The display will also periodically invert color;
this is just to indicate that the Debug Board MCU is still running.

<p align="center">
<kbd>
<img src="./misc/media/debug_board_1.png" width="600px">
<br>
<br>
<em>Debug board.</em>
<br>
<br>
</kbd>
</p>&nbsp;

The Debug Board displays the following information.

| Field     | Description                                                                                          |
| :-------: | ---------------------------------------------------------------------------------------------------- |
| `DB-T`    | Time elapsed since the debug board was powered on.                                                   |
| `MFC-T`   | Time elapsed since the Main Flight Compiuter was powered on.                                         |
| `SCB-A`   | Voltage of SB-SCA (:warning: TODO: Fix naming).                                                                |
| `SCB-B`   | Voltage of HP-SCA (:warning: TODO: Fix naming).                                                                |
| `te1`     | Whether or not Main Flight Computer is detecting TE-1.                                               |
| `vehicle` | Whether or not Main Flight Computer is communicating with the Vehicle Flight Computer through the vehicle interface. |
| `esp32`   | Whether or not Main Flight Computer is receiving ESP-NOW data packets.                               |
| `lora`    | Whether or not Main Flight Computer is receiving LoRa data packets.                                  |
| `logger`  | Whether or not Main Flight Computer is saving data to its uSD card.                                  |

If the Debug Board has not received the first debug packet from the Main Flight Computer yet,
then the field values will either be `???` or `nan` to indicate this.

&nbsp;

The debug board has four RGB LEDs:

| Label     | Position     | Description                                                                                          |
| :-------: | :----------: | ---------------------------------------------------------------------------------------------------- |
| `A`       | Top Right    | Flashes colors when the buzzer is being driven; this is just an extra indicator that's useful for when the buzzer is mute. |
| `B`       | Top Left     | White when the Main Flight Computer hasn't sent the first debug packet to the Debug Board yet; red when there's a bad status to be aware of such as the Main Flight Computer not having received RF data in a while. |
| `C`       | Bottom Right | Toggles red when the Main Flight Computer hasn't sent a debug packet in a while; toggles green when a debug packet has been received. |
| `D`       | Bottom Left  | White when the uSD card is being formatted; magenta when the Debug Board is failing to initialize the uSD card; toggles green periodically when data is successfully being logged to the uSD card. |

&nbsp;

The debug board plays the following buzzer tunes:

| Name                                        | Description                                                                    |
| :---------:                                 | ------------------------------------------------------------------------------ |
| [Nokia      ](./misc/media/nokia.wav      ) | Debug Board is initializing (e.g. just powered on from GSE-1). |
| [Hazard     ](./misc/media/hazard.wav     ) | Debug Board hasn't received a debug packet from the Main Flight Computer in a while. |
| [Up and Down](./misc/media/up_and_down.wav) | `te1` is active (i.e. burn-wire is going). |
| [Deep Beep  ](./misc/media/deep_beep.wav  ) | Main Flight Computer is reporting a bad status (e.g. haven't received RF data in a while) |
| [Tetris     ](./misc/media/tetris.wave    ) | The Debug Board's uSD card is being reformatted. |

<p align="center">
<kbd>
<img src="./misc/media/debug_board_2.png" width="600px">
<br>
<br>
<em>Debug board.</em>
<br>
<br>
</kbd>
</p>&nbsp;

> [!IMPORTANT]
> A known issue with the Debug Board's display is that it is not resettable.
> That is, if the Debug Board MCU has a communication issue with the display driver,
> there's a small possibility that the display ends up in a corrupted state.
> This may appear as the screen being darker, the resolution looking off,
> or is completely blank.
> The MCU has no way to fix this,
> but power-cycling will resolve this rare issue if it ever happens.

# Wallops Testing Procedure.

> [!CAUTION]
> Incomplete.



# Remove-Before-Flight Procedure.

> [!CAUTION]
> Incomplete.