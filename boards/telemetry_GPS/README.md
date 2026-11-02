## Cart Orders

Mouser Cart(enough for 1 board, with some +/- tolerance in the number of parts): [https://www.mouser.com/en/Tools/SavedCart/Share?AccessID=581db3815b](https://www.mouser.com/en/Tools/SavedCart/Share?AccessID=141096782c)

Digikey Cart(enough for 2 transmitters, 2 receivers): [https://www.digikey.com/short/j81pmn1m](https://www.digikey.com/short/2zj99zv8)](https://www.digikey.com/short/7t22bw8j)](https://www.digikey.com/short/md8pf59v)

E-Shop Order (these need to be added to the carts):

<img width="731" height="167" alt="Screenshot 2026-10-06 083104" src="https://github.com/user-attachments/assets/e29c6927-ff49-4845-a407-302bd37e28b1" />

## Block Diagram

<img width="1074" height="511" alt="image" src="https://github.com/user-attachments/assets/b3426f0e-749b-41b7-ae15-fa8eaa48ae5a" />


## Common Issues/Tips

- The recommended way to solder this board is to use a stencil for the top side, place all components (except for buttons & connectors) on the solder paste, and use a reflow oven. For the back side, it's recommended to use the backside stencil as well as a heat gun. Try to be careful with applying heat around the UBLOX NEO-M8N area. DO NOT UNDER ANY CIRCUMSTANCES SOLDER THE BACK SIDE IN AN OVEN AFTER REFLOWING THE TOP SIDE.
- Always use a microscope when debugging your soldering of the telemetry "Scout" board
- Stencil
  - Make sure that the stencil is relatively flat onto the board, otherwise you'll do multiple tries and end up with way too much paste
  - Pins shouldn't have a ton of solder but always make sure to check that EVERY pin has solder
- MCU Orientation
  - The dot on the bottom left of the MCU marks pin 1, don't be fooled by anything else. If you did it correctly, the ST logo should be on the bottom left while looking at the board top-down, and the U1 label on the left side of the MCU
- BGA
  - The BALFHB uses BGA pins. These pins already come pre-tinned so do not apply solder on those pads, simply place the component down onto the pads.
  - If you suspect that the component is not flush/a couple pins are unconnected, apply flux and use the heat gun, flux is your best friend
